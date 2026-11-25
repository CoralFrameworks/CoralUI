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

#ifndef CORAL_UI_TAGLIST_H
#define CORAL_UI_TAGLIST_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class Chip;
class TextLayer;

/** A single-line list of tags with a +N overflow marker. */
class TagList final : public Layer {
 public:
  TagList(std::vector<std::string> tags, std::shared_ptr<const Font> font,
    int maxVisible = 4);

  void setTags(std::vector<std::string> tags);
  [[nodiscard]] const std::vector<std::string>& tags() const noexcept {
    return mTags;
  }
  void setMaxVisible(int count);

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "taglist";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuild();

  std::vector<std::string> mTags;
  std::vector<std::shared_ptr<Chip>> mChips;
  std::shared_ptr<TextLayer> mMore;
  std::shared_ptr<const Font> mFont;
  int mMaxVisible = 4;
  float mGap = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_TAGLIST_H
