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

#include <ui/TitleBar.h>

#include <ui/Button.h>
#include <ui/Flex.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include <algorithm>

namespace coral::ui {

namespace {
constexpr float kTitleBarHeight = 40.0f;
}

TitleBar::TitleBar(std::string title, std::shared_ptr<const Font> font, const Color titleColor) {
  mBar = std::make_shared<Flex>(Axis::Row);
  mBar->setPadding({ 12.0f, 0.0f, 8.0f, 0.0f });
  mBar->setGap(4.0f);
  mBar->setCrossAlign(Align::Center);
  mBar->setJustify(Justify::SpaceBetween);
  mBar->setBackgroundColorRole(ThemeColorRole::SurfaceRaised);

  mTitle = std::make_shared<TextLayer>(std::move(title), std::move(font), titleColor);
  // A title is a single line; the bar is the only place that decides where it is clipped.
  mTitle->setWrapWidth(10000.0f);
  mBar->addChild(mTitle);

  auto controls = std::make_shared<Flex>(Axis::Row);
  controls->setGap(2.0f);
  controls->setCrossAlign(Align::Center);
  mMinimize = std::make_shared<Button>("-", mTitle->font());
  mMaximize = std::make_shared<Button>("[]", mTitle->font());
  mClose = std::make_shared<Button>("x", mTitle->font());
  controls->addChild(mMinimize);
  controls->addChild(mMaximize);
  controls->addChild(mClose);
  mBar->addChild(controls);

  addChild(mBar);
  setRepaintBoundary(true);
}

void TitleBar::setTitle(std::string title) {
  if (mTitle != nullptr) mTitle->setText(std::move(title));
}

const std::string& TitleBar::title() const noexcept {
  static const std::string empty;
  return mTitle != nullptr ? mTitle->text() : empty;
}

void TitleBar::setOnMinimize(std::function<void()> callback) {
  if (mMinimize != nullptr) mMinimize->setOnClick(std::move(callback));
}

void TitleBar::setOnMaximize(std::function<void()> callback) {
  if (mMaximize != nullptr) mMaximize->setOnClick(std::move(callback));
}

void TitleBar::setOnClose(std::function<void()> callback) {
  if (mClose != nullptr) mClose->setOnClick(std::move(callback));
}

void TitleBar::setShowMinimize(const bool show) {
  if (mMinimize != nullptr) mMinimize->setVisible(show);
}

void TitleBar::setShowMaximize(const bool show) {
  if (mMaximize != nullptr) mMaximize->setVisible(show);
}

void TitleBar::setShowClose(const bool show) {
  if (mClose != nullptr) mClose->setVisible(show);
}

Size TitleBar::measure(const Size& available) {
  if (mBar != nullptr) mBar->measure(Size{ available.width, kTitleBarHeight });
  return Size{ available.width, kTitleBarHeight };
}

void TitleBar::layout() {
  if (mBar != nullptr) {
    mBar->setBounds(Rect::MakeWH(bounds().getWidth(), kTitleBarHeight));
    mBar->layout();
  }
}

} // namespace coral::ui
