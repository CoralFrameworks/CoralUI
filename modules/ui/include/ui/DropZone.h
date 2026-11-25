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

#ifndef CORAL_UI_DROPZONE_H
#define CORAL_UI_DROPZONE_H

#include <ui/Types.h>

#include <ui/DragDrop.h>
#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;

/** A dashed-looking file drop target with a hover state. */
class DropZone final : public Layer {
 public:
  DropZone(std::string label, std::shared_ptr<const Font> font);
  ~DropZone() override;

  void setLabel(std::string label);
  void setFont(std::shared_ptr<const Font> font);
  void setOnFilesDropped(std::function<void(std::vector<std::string>)> callback) {
    mOnFilesDropped = std::move(callback);
  }
  void setOnTextDropped(std::function<void(std::string)> callback) {
    mOnTextDropped = std::move(callback);
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  DragEffect onDragEnter(const DragEvent& event) override;
  DragEffect onDragOver(const DragEvent& event) override;
  void onDragLeave() override;
  bool onDrop(const DropEvent& event) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "dropzone";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::string mLabel;
  std::shared_ptr<const Font> mFont;
  std::function<void(std::vector<std::string>)> mOnFilesDropped;
  std::function<void(std::string)> mOnTextDropped;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(90, 100, 116, 255);
  Color mHover = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  bool mHovered = false;
};

} // namespace coral::ui

#endif // CORAL_UI_DROPZONE_H
