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

#ifndef CORAL_UI_AVATARGROUP_H
#define CORAL_UI_AVATARGROUP_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class Avatar;
class TextLayer;

/** Overlapping avatars with a +N overflow marker. */
class AvatarGroup final : public Layer {
 public:
  AvatarGroup(std::vector<std::string> initials, std::shared_ptr<const Font> font,
    int maxVisible = 4, float size = 28.0f);

  void setInitials(std::vector<std::string> initials);
  [[nodiscard]] const std::vector<std::string>& initials() const noexcept {
    return mInitials;
  }
  void setMaxVisible(int count);
  void setSize(float size);
  [[nodiscard]] float size() const noexcept {
    return mSize;
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "avatargroup";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuild();

  std::vector<std::string> mInitials;
  std::vector<std::shared_ptr<Avatar>> mAvatars;
  std::shared_ptr<TextLayer> mMore;
  std::shared_ptr<const Font> mFont;
  int mMaxVisible = 4;
  float mSize = 28.0f;
  float mStep = 20.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_AVATARGROUP_H
