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

#ifndef CORAL_UI_TAGINPUT_H
#define CORAL_UI_TAGINPUT_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class Chip;
class TextInput;

/** An input that turns submitted text into removable Chips. */
class TagInput final : public Layer {
 public:
  explicit TagInput(std::shared_ptr<const Font> font, std::string placeholder = "Add tag");

  void setTokens(std::vector<std::string> tokens);
  [[nodiscard]] const std::vector<std::string>& tokens() const noexcept {
    return mTokens;
  }
  void setPlaceholder(std::string placeholder);
  void setFont(std::shared_ptr<const Font> font);
  void setOnChanged(std::function<void(const std::vector<std::string>&)> callback) {
    mOnChanged = std::move(callback);
  }
  [[nodiscard]] const std::shared_ptr<TextInput>& input() const noexcept {
    return mInput;
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "taginput";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuild();
  void addToken(const std::string& token);
  void removeToken(int index);

  std::vector<std::string> mTokens;
  std::vector<std::shared_ptr<Chip>> mChips;
  std::shared_ptr<TextInput> mInput;
  std::function<void(const std::vector<std::string>&)> mOnChanged;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  float mGap = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_TAGINPUT_H
