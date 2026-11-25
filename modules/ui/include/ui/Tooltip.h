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

#ifndef CORAL_UI_TOOLTIP_H
#define CORAL_UI_TOOLTIP_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <memory>
#include <chrono>
#include <string>

namespace coral::ui {

struct Theme;

/**
 * A small passive label shown near a pointer or an anchor.
 *
 * It is intentionally not hit-testable and sits at a high z-index, so it never steals a click from
 * the control it describes. Showing/hiding is explicit; a hover timer can call `showAt` later.
 */
class Tooltip final : public Layer {
 public:
  Tooltip(std::string text, std::shared_ptr<const Font> font);

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground);

  void showAt(const Point& position);
  void showAt(const Rect& anchor);
  /**
   * Arms a hover timer: after @p delay the tooltip shows at @p anchor.
   *
   * Re-arming restarts the delay, so moving between controls does not flash the previous text. The
   * timer runs on the layer's `onFrame` ticks, which the window already drives once per frame.
   */
  void scheduleShowAt(Point anchor, std::chrono::milliseconds delay = std::chrono::milliseconds(600));
  /** Disarms a pending timer (the pointer left before it fired). */
  void cancelScheduled();
  [[nodiscard]] bool scheduled() const noexcept {
    return mPending;
  }
  void hide();
  [[nodiscard]] bool isOpen() const noexcept {
    return visible();
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mPending;
  }

  [[nodiscard]] const char* defaultStyleName() const override {
    return "tooltip";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mText;
  std::shared_ptr<const Font> mFont;
  bool mPending = false;
  Point mPendingAnchor{};
  std::chrono::steady_clock::time_point mShowAt{};
  Color mBackground = Color::MakeRGBA8(52, 56, 66, 255);
  Color mBorder = Color::MakeRGBA8(72, 80, 94, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  float mPadding = 8.0f;
  float mRadius = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_TOOLTIP_H
