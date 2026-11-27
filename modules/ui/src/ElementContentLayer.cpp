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

#include <ui/ElementContentLayer.h>

#include <ui/Reconciler.h>

#include <utility>

namespace coral::ui {

ElementContentLayer::ElementContentLayer(ElementPtr content) {
  setContent(std::move(content));
}

ElementContentLayer::~ElementContentLayer() = default;

void ElementContentLayer::setContent(ElementPtr content) {
  if (mRoot != nullptr) {
    removeChild(mRoot);
    mRoot.reset();
  }
  mContent = std::move(content);
  mReconciler = std::make_unique<Reconciler>();
  if (mContent != nullptr) {
    mRoot = mReconciler->mount(mContent);
    if (mRoot != nullptr) addChild(mRoot);
  }
  setNeedsLayout();
  setNeedsPaint();
}

Size ElementContentLayer::measure(const Size& available) {
  if (mRoot == nullptr) {
    setMeasuredSize({});
    return measuredSize();
  }
  const Size measured = mRoot->measure(available);
  setMeasuredSize(measured);
  return measured;
}

void ElementContentLayer::layout() {
  if (mRoot != nullptr) {
    mRoot->setBounds(bounds());
    mRoot->layout();
  }
  clearNeedsLayout();
}

} // namespace coral::ui

