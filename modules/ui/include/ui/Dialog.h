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

#ifndef CORAL_UI_DIALOG_H
#define CORAL_UI_DIALOG_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class Button;
class TextPainter;

/**
 * A modal-looking dialog: a dimmed overlay, a centred panel, a title, a message and a row of buttons.
 *
 * It is an ordinary layer as far as the window is concerned (the caller adds it to the root and gives
 * it the window bounds). While shown it covers the window and therefore receives clicks before the
 * layers behind it, which is the part of modality the first editor dialogs need.
 */
class Dialog final : public Layer {
public:
  Dialog(std::string title, std::shared_ptr<const Font> font);
  ~Dialog() override;

  void setTitle(std::string title);
  void setMessage(std::string message);
  /** Adds a button; @return its index, which is the dialog result when it is pressed. */
  int addButton(std::string label, std::function<void()> action);
  /** Arbitrary content placed between the message and the button row. */
  void setContent(std::shared_ptr<Layer> content);
  /** The button Enter activates; -1 means none. */
  void setDefaultButton(int index);
  [[nodiscard]] int defaultButton() const noexcept {
    return mDefaultButton;
  }
  /** Called with the pressed button index, or -1 for Escape/dismissal. */
  void setOnResult(std::function<void(int)> callback);
  /** Hides the dialog and reports @p result to the result callback. */
  void close(int result);
  [[nodiscard]] int lastResult() const noexcept {
    return mLastResult;
  }

  void show();
  void hide();

  [[nodiscard]] bool isOpen() const noexcept {
    return visible();
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;

  /** The panel rectangle inside the layer's bounds, exposed for tests and anchoring. */
  [[nodiscard]] Rect panelBounds() const;

  [[nodiscard]] const char* defaultStyleName() const override {
    return "dialog";
  }

protected:
  void applyTheme(const Theme& theme) override;

private:
  void activate(int index);

  std::shared_ptr<const Font> mFont;
  std::string mTitle;
  std::string mMessage;
  std::shared_ptr<Layer> mContent;
  std::vector<std::shared_ptr<Button>> mButtons;
  std::vector<std::function<void()>> mButtonActions;
  std::function<void(int)> mOnResult;
  int mDefaultButton = -1;
  int mLastResult = 0;
  float mPanelHeight = 240.0f;
  Color mPanel = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mTitleColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mMessageColor = Color::MakeRGBA8(148, 163, 184, 255);
  Color mOverlay = Color::MakeRGBA8(0, 0, 0, 150);
  float mPanelPadding = 20.0f;
  float mRadius = 8.0f;
  std::unique_ptr<TextPainter> mTitlePainter;
  std::unique_ptr<TextPainter> mMessagePainter;
};

} // namespace coral::ui

#endif // CORAL_UI_DIALOG_H
