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

#ifndef CORAL_UI_STYLEDTEXT_H
#define CORAL_UI_STYLEDTEXT_H

#include <ui/Layer.h>
#include <ui/RichText.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

class RichTextLayer;

/** One run's visual style. */
struct TextStyle {
  std::shared_ptr<const Font> font;
  Color color = Color::White();
  bool underline = false;

  friend bool operator==(const TextStyle& left, const TextStyle& right) = default;
};

/** One styled run of a document. An empty `linkId` means the run is not clickable. */
struct TextSpan {
  std::string text;
  TextStyle style;
  std::string linkId;

  friend bool operator==(const TextSpan& left, const TextSpan& right) = default;
};

/** Paragraph-level options for a styled document. */
struct TextParagraphStyle {
  TextAlign align = TextAlign::Left;
  /** Fixed line box height; 0 uses the largest font metrics on each line. */
  float lineHeight = 0.0f;
  /** 0 means unlimited; a positive value clamps the line count. */
  int maxLines = 0;
  bool ellipsis = false;
  bool selectable = false;
  /** 0 measures against the available width; a positive value pins the wrap width. */
  float wrapWidth = 0.0f;

  friend bool operator==(const TextParagraphStyle& left, const TextParagraphStyle& right) = default;
};

/**
 * A declarative rich-text document.
 *
 * This is a value type: the element layer copies it, compares it by value and only re-lays out the
 * text when it changes. `RichTextLayer` remains the low-level renderer; `StyledText` is the stable
 * control/document API over it.
 */
struct TextDocument {
  std::vector<TextSpan> spans;
  TextParagraphStyle paragraph;

  [[nodiscard]] static TextDocument plain(std::string text, TextStyle style,
      TextParagraphStyle paragraph = {}) {
    TextDocument document;
    document.spans.push_back(TextSpan{ std::move(text), std::move(style), {} });
    document.paragraph = paragraph;
    return document;
  }

  friend bool operator==(const TextDocument& left, const TextDocument& right) = default;
};

/**
 * A read-only styled-text control.
 *
 * R1 renders spans, paragraph options and selection through a child `RichTextLayer`; R2 adds link
 * hit-testing and `onLinkActivated`. Editing is deliberately not part of this control.
 */
class StyledText final : public Layer {
 public:
  explicit StyledText(TextDocument document = {});
  ~StyledText() override;

  void setDocument(TextDocument document);
  [[nodiscard]] const TextDocument& document() const noexcept {
    return mDocument;
  }

  void setOnLinkActivated(std::function<void(std::string_view)> callback);
  [[nodiscard]] const std::function<void(std::string_view)>& onLinkActivated() const noexcept {
    return mOnLinkActivated;
  }

  /** The link id at a window-space point, or an empty string. */
  [[nodiscard]] std::string linkAt(const Point& point) const;

  Size measure(const Size& available) override;
  void layout() override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "styledtext";
  }

 private:
  struct LinkRange {
    size_t start = 0;
    size_t end = 0;
    std::string id;
  };

  void rebuildRuns();

  TextDocument mDocument;
  std::shared_ptr<RichTextLayer> mText;
  std::vector<LinkRange> mLinks;
  std::function<void(std::string_view)> mOnLinkActivated;
};

} // namespace coral::ui

#endif // CORAL_UI_STYLEDTEXT_H
