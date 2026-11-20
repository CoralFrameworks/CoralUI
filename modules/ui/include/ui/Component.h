/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_COMPONENT_H
#define CORAL_UI_COMPONENT_H

#include <ui/Element.h>

#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace coral::ui {

class ElementHost;
class Layer;
class Theme;
class Window;

/**
 * What a component's `build` may use: rebuild, theme, disposable resources and the error boundary.
 *
 * The context is owned by the `ComponentHost` and stays valid for the component's whole lifetime, so
 * a component may keep a reference to it; after disposal `alive()` turns false and `invalidate()`
 * becomes a no-op. It is intentionally not a service locator: data and commands stay explicit
 * constructor parameters of the component, which keeps the data flow readable and testable.
 */
class ComponentContext {
 public:
  using Invalidate = std::function<void()>;

  ComponentContext(Invalidate invalidate, const Theme* theme);

  ComponentContext(const ComponentContext&) = delete;
  ComponentContext& operator=(const ComponentContext&) = delete;

  /** Schedules one coalesced rebuild of the component tree. */
  void invalidate() const;

  /** The window theme at mount time, or null when the host has none configured. */
  [[nodiscard]] const Theme* theme() const noexcept {
    return mTheme;
  }

  [[nodiscard]] bool alive() const noexcept;

  /** Registers a callback that runs when the host disposes the component (reverse order). */
  void onDispose(std::function<void()> callback);

  /**
   * Runs @p build with the error boundary.
   *
   * A thrown exception is reported to the host's handler and replaced by a transparent fallback, so
   * one broken component does not tear down the window's layer tree.
   */
  [[nodiscard]] ElementPtr guard(const std::function<ElementPtr()>& build) const;

  void setErrorHandler(std::function<void(std::exception_ptr)> handler);

  void reportError(std::exception_ptr error) const;

  /**
   * Publishes a service for this component's subtree.
   *
   * Providers are the component-model equivalent of DI: the host or a parent component provides a
   * model/store/window wrapper, and a build function asks for it by type instead of receiving it
   * through every constructor. A child context copies the provider map when it is created, so a
   * component may override one service for its own subtree.
   */
  template <typename T>
  void provide(std::shared_ptr<T> value) {
    mProviders[std::type_index(typeid(T))] = std::move(value);
  }
  template <typename T>
  [[nodiscard]] std::shared_ptr<T> find() const {
    const auto position = mProviders.find(std::type_index(typeid(T)));
    if (position == mProviders.end()) return nullptr;
    return std::static_pointer_cast<T>(position->second);
  }
  /** Like `find`, but a missing provider is a programming error and throws. */
  template <typename T>
  [[nodiscard]] std::shared_ptr<T> require() const {
    std::shared_ptr<T> value = find<T>();
    if (value == nullptr) {
      throw std::runtime_error("component context is missing a required provider");
    }
    return value;
  }

  /** Runs and clears the registered disposals; the host calls this during `dispose`. */
  void runDisposals();

  /**
   * Creates a child context for a nested component.
   *
   * The child shares the liveness token and the invalidation function (a child rebuild invalidates the
   * host tree) and copies the provider map, so it can override one service without affecting the
   * parent.
   */
  [[nodiscard]] std::shared_ptr<ComponentContext> makeChild() const;

 private:
  std::shared_ptr<bool> mAlive;
  Invalidate mInvalidate;
  const Theme* mTheme = nullptr;
  std::function<void(std::exception_ptr)> mErrorHandler;
  std::vector<std::function<void()>> mDisposals;
  std::unordered_map<std::type_index, std::shared_ptr<void>> mProviders;
};

/**
 * A unit of UI with lifecycle.
 *
 * `build` runs inside `ComponentHost`'s reconcile and must be a pure function of the component's
 * state; it returns a fresh immutable element tree. `onMount` runs after the first tree is mounted,
 * `onUpdate` after every rebuild, `onUnmount` when the host is disposed. A component that needs to
 * react to state keeps an `observer::Observation` from `State::observe` and invalidates the context.
 */
class Component {
 public:
  virtual ~Component() = default;

  [[nodiscard]] virtual ElementPtr build(ComponentContext& context) = 0;
  virtual void onMount(ComponentContext& context) {
    (void)context;
  }
  virtual void onUpdate(ComponentContext& context) {
    (void)context;
  }
  virtual void onUnmount() {}
};

/**
 * Mounts one root component into a window.
 *
 * This is the component-model entry point: the host owns the `ElementHost` (hence the reconciler and
 * the coalesced rebuild queue), calls the component lifecycle, and turns `invalidate()` into one
 * posted rebuild. A failure inside `build` goes through `setErrorHandler` and leaves the previous
 * tree mounted.
 *
 * ```cpp
 * auto component = std::make_shared<EditorPage>(model);
 * ComponentHost host(window, component);
 * host.setErrorHandler([](std::exception_ptr error) { report(error); });
 * host.mount();
 * ```
 */
class ComponentHost {
 public:
  ComponentHost(Window& window, std::shared_ptr<Component> root);
  ~ComponentHost();

  ComponentHost(const ComponentHost&) = delete;
  ComponentHost& operator=(const ComponentHost&) = delete;

  /** Builds and mounts the first tree, then makes it the window root. */
  void mount();
  /** Coalesces a rebuild into one posted task. */
  void invalidate();
  /** Unmounts: component `onUnmount`, disposals, and the layer tree is detached from the window. */
  void dispose();

  [[nodiscard]] std::shared_ptr<Layer> root() const;
  [[nodiscard]] bool mounted() const noexcept {
    return mMounted;
  }
  [[nodiscard]] ComponentContext& context() noexcept {
    return mContext;
  }

  void setErrorHandler(std::function<void(std::exception_ptr)> handler);

  /** Publishes a service for this host's component tree (forwarded to the root context). */
  template <typename T>
  void provide(std::shared_ptr<T> service) {
    mContext.provide(std::move(service));
  }

 private:
  Window& mWindow;
  std::shared_ptr<Component> mRoot;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  ComponentContext mContext;
  std::unique_ptr<ElementHost> mHost;
  bool mMounted = false;
};


/**
 * Builds a component node for the reconciler: a child component with its own lifecycle.
 *
 * The parent context supplies the invalidation route and the inherited providers; the element stores
 * the child context so a rebuild of the parent does not recreate it. Use this instead of
 * `components::Component` when the child needs `onMount`/`onUpdate`/`onUnmount` and per-child state.
 */
[[nodiscard]] ElementPtr ChildComponent(std::shared_ptr<Component> component,
  ComponentContext& parent, std::string key = {});

} // namespace coral::ui

#endif // CORAL_UI_COMPONENT_H
