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

#include <ui/StyledText.h>

#include <ui/RichTextLayer.h>
#include <ui/Window.h>

#include <utility>

namespace coral::ui {

StyledText::StyledText(TextDocument document) {
  mText = std::make_shared<RichTextLayer>();
  addChild(mText);
  setDocument(std::move(document));
}

StyledText::~StyledText() = default;

void StyledText::setDocument(TextDocument document) {
  mDocument = std::move(document);
  rebuildRuns();

  const TextParagraphStyle& paragraph = mDocument.paragraph;
  mText->setSelectable(paragraph.selectable);
  mText->setTextAlign(paragraph.align);
  mText->setLineHeight(paragraph.lineHeight);
  mText->setMaxLines(paragraph.maxLines);
  mText->setEllipsis(paragraph.ellipsis);
  mText->setWrapWidth(paragraph.wrapWidth);
  setNeedsLayout();
  setNeedsPaint();
}

void StyledText::rebuildRuns() {
  std::vector<RichTextRun> runs;
  runs.reserve(mDocument.spans.size());
  mLinks.clear();

  size_t offset = 0;
  for (const TextSpan& span: mDocument.spans) {
    RichTextRun run;
    run.text = span.text;
    run.font = span.style.font;
    run.color = span.style.color;
    run.underline = span.style.underline;
    runs.push_back(std::move(run));

    if (!span.linkId.empty() && !span.text.empty()) {
      mLinks.push_back(LinkRange{ offset, offset + span.text.size(), span.linkId });
    }
    offset += span.text.size();
  }
  mText->setRuns(std::move(runs));
}

void StyledText::setOnLinkActivated(std::function<void(std::string_view)> callback) {
  mOnLinkActivated = std::move(callback);
}

std::string StyledText::linkAt(const Point& point) const {
  if (mText == nullptr || mLinks.empty()) return {};
  const Rect area = bounds();
  if (point.x < area.getLeft() || point.x >= area.getRight() || point.y < area.getTop() ||
      point.y >= area.getBottom()) {
    return {};
  }

  const size_t index = mText->indexAt(point);
  for (const LinkRange& link: mLinks) {
    if (index >= link.start && index < link.end) return link.id;
  }
  return {};
}

Size StyledText::measure(const Size& available) {
  if (mText == nullptr) return { 0.0f, 0.0f };
  const Size measured = mText->measure(available);
  setMeasuredSize(measured);
  return measured;
}

void StyledText::layout() {
  if (mText == nullptr) return;
  mText->setBounds(bounds());
  mText->layout();
}

bool StyledText::onPointerDown(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  const std::string link = linkAt(event.position);
  if (link.empty()) return false;
  if (mOnLinkActivated) mOnLinkActivated(link);
  return true;
}

bool StyledText::onPointerMove(const PointerEvent& event) {
  if (Window* target = window(); target != nullptr) {
    const bool overLink = !linkAt(event.position).empty();
    const Window::Cursor cursor = overLink ? Window::Cursor::Hand
      : mDocument.paragraph.selectable ? Window::Cursor::IBeam : Window::Cursor::Arrow;
    target->setCursor(cursor);
  }
  return false;
}

bool StyledText::onPointerLeave() {
  if (Window* target = window(); target != nullptr) target->setCursor(Window::Cursor::Arrow);
  return false;
}

} // namespace coral::ui
