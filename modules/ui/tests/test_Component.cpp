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

#include <ui/ColorLayer.h>
#include <ui/Component.h>
#include <ui/State.h>
#include <ui/Reconciler.h>
#include <ui/Ref.h>
#include <ui/Window.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace coral::ui {
namespace {

TEST(UiState, NotifiesObserversAndIgnoresEqualWrites) {
  State<int> state(1);
  int changes = 0;
  int last = 0;
  auto observation = state.observe([&changes, &last](const int& value) {
    ++changes;
    last = value;
  });

  EXPECT_FALSE(state.set(1)) << "an equal write is a no-op";
  EXPECT_EQ(changes, 0);
  EXPECT_TRUE(state.set(2));
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(last, 2);

  observation.reset();
  EXPECT_TRUE(state.set(3));
  EXPECT_EQ(changes, 1) << "a reset observation no longer fires";
  EXPECT_EQ(state.get(), 3);
}

TEST(UiState, ImmediateObservationDeliversTheCurrentValue) {
  State<int> state(7);
  int seen = 0;
  auto observation = state.observe([&seen](const int& value) { seen = value; }, true);
  EXPECT_EQ(seen, 7);
}

/** A component that counts lifecycle calls and rebuilds when its own state changes. */
class CounterComponent final : public Component {
 public:
  ElementPtr build(ComponentContext& context) override {
    ++builds;
    if (builds == 1) {
      observation = count.observe([&context](const int&) { context.invalidate(); });
    }
    return std::make_shared<ColorElement>(Color::MakeRGBA8(10, 20, 30, 255), "root");
  }

  void onMount(ComponentContext& context) override {
    ++mounts;
    context.onDispose([this] { disposed = true; });
  }

  void onUpdate(ComponentContext&) override {
    ++updates;
  }

  void onUnmount() override {
    unmounted = true;
  }

  State<int> count{0};
  int builds = 0;
  int mounts = 0;
  int updates = 0;
  bool unmounted = false;
  bool disposed = false;
  observer::Observation observation;
};

std::shared_ptr<Window> headlessWindow() {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  return Window::create(options);
}

TEST(UiComponent, HostBuildsMountsAndRebuildsOnStateChange) {
  auto window = headlessWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto component = std::make_shared<CounterComponent>();
  ComponentHost host(*window, component);
  host.mount();

  EXPECT_TRUE(host.mounted());
  ASSERT_NE(host.root(), nullptr);
  EXPECT_EQ(component->builds, 1);
  EXPECT_EQ(component->mounts, 1);
  EXPECT_EQ(host.context().theme(), &window->theme());

  component->count.set(42);
  window->pump();
  EXPECT_EQ(component->builds, 2) << "invalidate becomes one posted rebuild";
  EXPECT_EQ(component->updates, 1);
  EXPECT_EQ(component->count.get(), 42);
}

TEST(UiComponent, DisposeRunsUnmountAndDisposalsAndStopsRebuilds) {
  auto window = headlessWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto component = std::make_shared<CounterComponent>();
  ComponentHost host(*window, component);
  host.mount();
  host.dispose();

  EXPECT_FALSE(host.mounted());
  EXPECT_TRUE(component->unmounted);
  EXPECT_TRUE(component->disposed);

  component->count.set(1);
  window->pump();
  EXPECT_EQ(component->builds, 1) << "a disposed host ignores invalidations";
}

TEST(UiComponent, BuildFailuresReachTheErrorHandlerAndKeepAFallbackTree) {
  auto window = headlessWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  class ThrowingComponent final : public Component {
   public:
    ElementPtr build(ComponentContext&) override {
      throw std::runtime_error("component build failed");
    }
  };

  auto component = std::make_shared<ThrowingComponent>();
  ComponentHost host(*window, component);
  int errors = 0;
  host.setErrorHandler([&errors](std::exception_ptr error) {
    EXPECT_NE(error, nullptr);
    ++errors;
  });
  host.mount();

  EXPECT_EQ(errors, 1);
  EXPECT_TRUE(host.mounted()) << "the host survives a failed build";
  EXPECT_NE(host.root(), nullptr) << "a transparent fallback is mounted instead";
}


TEST(UiComponent, ContextProvidesAndRequiresServices) {
  struct Service {
    int value = 7;
  };

  auto window = headlessWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  auto component = std::make_shared<CounterComponent>();
  ComponentHost host(*window, component);
  auto service = std::make_shared<Service>();
  host.context().provide(service);

  EXPECT_EQ(host.context().find<Service>(), service);
  EXPECT_EQ(host.context().require<Service>()->value, 7);
  EXPECT_EQ(host.context().find<int>(), nullptr);
  EXPECT_THROW(host.context().require<int>(), std::runtime_error);
  host.mount();
  EXPECT_TRUE(host.mounted());
}

TEST(UiComponent, LayerRefResolvesAfterMountAndExpiresWithTheLayer) {
  auto ref = std::make_shared<LayerRef>();
  ElementProps props;
  props.ref = ref;
  {
    Reconciler reconciler;
    auto root = reconciler.mount(components::WithProps(
      std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "child"), props, "wrapper"));
    ASSERT_NE(root, nullptr);
    ASSERT_TRUE(ref->valid());
    EXPECT_EQ(ref->layer(), root) << "the props wrapper is transparent to the ref";
  }
  EXPECT_FALSE(ref->valid()) << "the weak ref expires with the detached layer";
  EXPECT_EQ(ref->layer(), nullptr);
}



TEST(UiComponent, ChildComponentsMountUpdateAndUnmountWithTheTree) {
  ComponentContext parent([] {}, nullptr);
  auto child = std::make_shared<CounterComponent>();
  const auto tree = [&parent, &child](const bool includeChild) -> ElementPtr {
    std::vector<ElementPtr> children;
    if (includeChild) children.push_back(ChildComponent(child, parent, "child"));
    return std::make_shared<StackElement>(Axis::Column, std::move(children), "root");
  };

  Reconciler reconciler;
  ASSERT_NE(reconciler.mount(tree(true)), nullptr);
  EXPECT_EQ(child->builds, 1);
  EXPECT_EQ(child->mounts, 1);

  reconciler.update(tree(true));
  EXPECT_EQ(child->builds, 2) << "a reconcile rebuilds the child subtree";
  EXPECT_EQ(child->updates, 1);

  reconciler.update(tree(false));
  EXPECT_TRUE(child->unmounted) << "removing the child from the tree unmounts it";
  EXPECT_TRUE(child->disposed);
  EXPECT_EQ(child->builds, 2) << "an unmounted child is not rebuilt";
}

TEST(UiComponent, ReplacingAChildComponentInstanceUnmountsTheOldOne) {
  ComponentContext parent([] {}, nullptr);
  auto first = std::make_shared<CounterComponent>();
  auto second = std::make_shared<CounterComponent>();
  const auto tree = [&parent](std::shared_ptr<Component> component) {
    return std::make_shared<StackElement>(Axis::Column,
      std::vector<ElementPtr>{ ChildComponent(std::move(component), parent, "child") }, "root");
  };

  Reconciler reconciler;
  ASSERT_NE(reconciler.mount(tree(first)), nullptr);
  EXPECT_EQ(first->mounts, 1);
  reconciler.update(tree(second));
  EXPECT_TRUE(first->unmounted);
  EXPECT_EQ(second->mounts, 1);
  EXPECT_EQ(second->builds, 1);
}

TEST(UiComponent, ChildStateInvalidationUsesTheParentRoute) {
  int invalidations = 0;
  ComponentContext parent([&invalidations] { ++invalidations; }, nullptr);
  auto child = std::make_shared<CounterComponent>();
  Reconciler reconciler;
  ASSERT_NE(reconciler.mount(std::make_shared<StackElement>(Axis::Column,
    std::vector<ElementPtr>{ ChildComponent(child, parent, "child") }, "root")), nullptr);

  child->count.set(5);
  EXPECT_GE(invalidations, 1) << "the child context forwards invalidation to the parent route";
}


TEST(UiComponent, LayerRefIsClearedWhenItsElementLeavesTheTree) {
  auto layer = std::make_shared<ColorLayer>(Color::MakeRGBA8(7, 8, 9, 255));
  auto ref = std::make_shared<LayerRef>();
  ElementProps props;
  props.ref = ref;

  Reconciler reconciler;
  ASSERT_NE(reconciler.mount(components::WithProps(components::Layer(layer, "host"), props,
    "wrapper")), nullptr);
  ASSERT_TRUE(ref->valid());
  EXPECT_EQ(ref->layer(), layer);

  // Replace the wrapper: the layer object is still held by `layer`, so only an explicit clear can
  // make the ref invalid here.
  reconciler.update(std::make_shared<ColorElement>(Color::MakeRGBA8(1, 1, 1, 255), "other"));
  EXPECT_FALSE(ref->valid()) << "leaving the tree clears the ref even while the layer is alive";
  EXPECT_EQ(ref->layer(), nullptr);
}

TEST(UiComponent, HostForwardsProvidedServices) {
  auto window = headlessWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  struct Store {
    int value = 3;
  };
  auto component = std::make_shared<CounterComponent>();
  ComponentHost host(*window, component);
  auto store = std::make_shared<Store>();
  host.provide(store);
  EXPECT_EQ(host.context().require<Store>(), store);
  EXPECT_EQ(host.context().require<Store>()->value, 3);
  host.mount();
}

} // namespace
} // namespace coral::ui
