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

#ifndef CORAL_UI_STAT_H
#define CORAL_UI_STAT_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;

enum class StatTrend {
  None,
  Up,
  Down,
  Flat,
};

/** A compact dashboard statistic: label, value and optional trend/caption. */
class Stat final : public Layer {
 public:
  Stat(std::string label, std::string value, std::shared_ptr<const Font> font,
    std::string caption = {}, StatTrend trend = StatTrend::None);

  void setLabel(std::string label);
  void setValue(std::string value);
  void setCaption(std::string caption);
  void setTrend(StatTrend trend);
  void setFont(std::shared_ptr<const Font> font);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "stat";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLabel;
  std::string mValue;
  std::string mCaption;
  std::shared_ptr<const Font> mFont;
  StatTrend mTrend = StatTrend::None;
  Color mLabelColor = Color::MakeRGBA8(148, 163, 184, 255);
  Color mValueColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mUpColor = Color::MakeRGBA8(80, 200, 140, 255);
  Color mDownColor = Color::MakeRGBA8(230, 100, 100, 255);
  Color mFlatColor = Color::MakeRGBA8(148, 163, 184, 255);
  float mGap = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_STAT_H
