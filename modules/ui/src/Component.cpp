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

#include <ui/Component.h>

#include <ui/ElementHost.h>
#include <ui/Window.h>

#include <utility>

namespace coral::ui {

namespace {

/** A completely transparent element stands in for a component whose build failed. */
[[nodiscard]] ElementPtr errorFallback() {
  return std::make_shared<ColorElement>(Color::MakeRGBA8(0, 0, 0, 0), "component-error");
}

} // namespace

ComponentContext::ComponentContext(Invalidate invalidate, const Theme* theme)
    : mAlive(std::make_shared<bool>(true)), mInvalidate(std::move(invalidate)), mTheme(theme) {}

void ComponentContext::invalidate() const {
  if (!alive() || !mInvalidate) return;
  mInvalidate();
}

bool ComponentContext::alive() const noexcept {
  return mAlive != nullptr && *mAlive;
}

void ComponentContext::onDispose(std::function<void()> callback) {
  if (callback) mDisposals.push_back(std::move(callback));
}

ElementPtr ComponentContext::guard(const std::function<ElementPtr()>& build) const {
  if (!build) return errorFallback();
  try {
    ElementPtr result = build();
    return result != nullptr ? result : errorFallback();
  } catch (...) {
    reportError(std::current_exception());
    return errorFallback();
  }
}

void ComponentContext::setErrorHandler(std::function<void(std::exception_ptr)> handler) {
  mErrorHandler = std::move(handler);
}

void ComponentContext::reportError(std::exception_ptr error) const {
  if (mErrorHandler) mErrorHandler(std::move(error));
}

void ComponentContext::runDisposals() {
  // Reverse order matches construction order: the last acquired resource is released first.
  for (auto position = mDisposals.rbegin(); position != mDisposals.rend(); ++position) {
    if (*position) (*position)();
  }
  mDisposals.clear();
}

ComponentHost::ComponentHost(Window& window, std::shared_ptr<Component> root)
    : mWindow(window), mRoot(std::move(root)),
      mContext([this] {
        if (mHost != nullptr) mHost->invalidate();
      }, &window.theme()) {}

ComponentHost::~ComponentHost() {
  dispose();
}

void ComponentHost::mount() {
  if (mMounted || mRoot == nullptr) return;
  mHost = std::make_unique<ElementHost>(mWindow, [this] {
    return mContext.guard([this] { return mRoot->build(mContext); });
  });
  mHost->setOnMounted([this] { mRoot->onMount(mContext); });
  mHost->setOnUpdated([this] { mRoot->onUpdate(mContext); });
  mHost->setOnUnmounted([this] { mRoot->onUnmount(); });
  mHost->mount();
  mWindow.setRoot(mHost->root());
  mMounted = true;
}

void ComponentHost::invalidate() {
  if (mHost != nullptr) mHost->invalidate();
}

void ComponentHost::dispose() {
  if (!mMounted) return;
  mMounted = false;
  if (mAlive != nullptr) *mAlive = false;
  // ElementHost's destructor calls onUnmounted (component onUnmount) and then drops the tree.
  mHost.reset();
  mContext.runDisposals();
}

std::shared_ptr<Layer> ComponentHost::root() const {
  return mHost != nullptr ? mHost->root() : nullptr;
}

void ComponentHost::setErrorHandler(std::function<void(std::exception_ptr)> handler) {
  mContext.setErrorHandler(std::move(handler));
}


std::shared_ptr<ComponentContext> ComponentContext::makeChild() const {
  auto child = std::make_shared<ComponentContext>(mInvalidate, mTheme);
  child->mAlive = mAlive;
  child->mProviders = mProviders;
  child->mErrorHandler = mErrorHandler;
  return child;
}

ElementPtr ChildComponent(std::shared_ptr<Component> component, ComponentContext& parent,
    std::string key) {
  return std::make_shared<ComponentElement>(std::move(component), parent.makeChild(),
    std::move(key));
}

} // namespace coral::ui