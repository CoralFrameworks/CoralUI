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

#ifndef CORAL_UI_STEPINDICATOR_H
#define CORAL_UI_STEPINDICATOR_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class TextLayer;

/** A horizontal wizard/progress step indicator. */
class StepIndicator final : public Layer {
 public:
  StepIndicator(std::vector<std::string> steps, std::shared_ptr<const Font> font,
    int current = 0);

  void setSteps(std::vector<std::string> steps);
  [[nodiscard]] const std::vector<std::string>& steps() const noexcept {
    return mSteps;
  }
  void setCurrent(int current);
  [[nodiscard]] int current() const noexcept {
    return mCurrent;
  }
  void setFont(std::shared_ptr<const Font> font);
  [[nodiscard]] const std::shared_ptr<const Font>& font() const noexcept {
    return mFont;
  }
  void setOnChanged(std::function<void(int)> callback) {
    mOnChanged = std::move(callback);
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "stepindicator";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] float stepWidth() const;
  [[nodiscard]] int stepAt(float x) const;
  void rebuild();

  std::vector<std::string> mSteps;
  std::shared_ptr<const Font> mFont;
  std::function<void(int)> mOnChanged;
  Color mCompleted = Color::MakeRGBA8(80, 200, 140, 255);
  Color mCurrentColor = Color::MakeRGBA8(86, 156, 255, 255);
  Color mPending = Color::MakeRGBA8(70, 76, 88, 255);
  Color mText = Color::MakeRGBA8(226, 232, 240, 255);
  int mCurrent = 0;
  float mGap = 24.0f;
  float mCircle = 22.0f;
  std::vector<std::shared_ptr<TextLayer>> mLabels;
  std::vector<std::shared_ptr<TextLayer>> mNumbers;
};

} // namespace coral::ui

#endif // CORAL_UI_STEPINDICATOR_H
