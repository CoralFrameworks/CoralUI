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

#include <ui/ToastStack.h>

#include <ui/Theme.h>
#include <ui/Toast.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

ToastStack::ToastStack(std::shared_ptr<const Font> font, const int maxVisible)
    : mFont(std::move(font)), mMaxVisible(std::max(1, maxVisible)) {}

void ToastStack::setMaxVisible(const int count) {
  mMaxVisible = std::max(1, count);
  while (static_cast<int>(mToasts.size()) > mMaxVisible) {
    const auto oldest = mToasts.front();
    mToasts.erase(mToasts.begin());
    if (oldest != nullptr) removeChild(oldest);
  }
  setNeedsLayout();
}

void ToastStack::show(std::string text, const std::chrono::milliseconds duration) {
  if (static_cast<int>(mToasts.size()) >= mMaxVisible) {
    const auto oldest = mToasts.front();
    mToasts.erase(mToasts.begin());
    if (oldest != nullptr) removeChild(oldest);
  }
  auto toast = std::make_shared<Toast>(text, mFont);
  const auto alive = mAlive;
  toast->setOnDismissed([this, alive, toast] {
    if (!*alive) return;
    mToasts.erase(std::remove(mToasts.begin(), mToasts.end(), toast), mToasts.end());
    removeChild(toast);
    setNeedsLayout();
  });
  toast->showFor(std::move(text), duration);
  addChild(toast);
  mToasts.push_back(std::move(toast));
  setNeedsLayout();
}

void ToastStack::clear() {
  for (const auto& toast: mToasts) {
    if (toast != nullptr) {
      toast->setOnDismissed({});
      removeChild(toast);
    }
  }
  mToasts.clear();
  setNeedsLayout();
}

Size ToastStack::measure(const Size& available) {
  float width = 180.0f;
  float height = 0.0f;
  for (const auto& toast: mToasts) {
    if (toast == nullptr) continue;
    const Size size = toast->measure(available);
    width = std::max(width, size.width);
    height += size.height + mGap;
  }
  if (height > 0.0f) height -= mGap;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void ToastStack::layout() {
  float top = bounds().getTop();
  for (const auto& toast: mToasts) {
    if (toast == nullptr) continue;
    const Size size = toast->measure({ bounds().getWidth(), 0.0f });
    toast->setBounds(Rect::MakeXYWH(bounds().getLeft(), top, bounds().getWidth(), size.height));
    toast->layout();
    top += size.height + mGap;
  }
  clearNeedsLayout();
}

void ToastStack::applyTheme(const Theme& theme) {
  mGap = theme.spacing.sm;
  setNeedsLayout();
}

} // namespace coral::ui
