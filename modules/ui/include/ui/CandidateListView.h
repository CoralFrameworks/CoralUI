/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_CANDIDATELISTVIEW_H
#define CORAL_UI_CANDIDATELISTVIEW_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

/**
 * The input method's candidate list, drawn by the UI.
 *
 * XIM has no candidate-list protocol and usually lets the input method draw its own popup; IBus
 * reports a lookup table instead. This layer is the shared presentation for that lookup table: the
 * platform/IME backend fills it, the window places it near the caret, and keyboard navigation stays
 * in the focused text control.
 */
class CandidateListView final : public Layer {
 public:
  explicit CandidateListView(std::shared_ptr<const Font> font);
  ~CandidateListView() override;

  void setFont(std::shared_ptr<const Font> font);
  void setCandidates(std::vector<std::string> candidates, size_t selected = 0);
  void setSelected(size_t selected);
  [[nodiscard]] const std::vector<std::string>& candidates() const noexcept { return mCandidates; }
  [[nodiscard]] size_t selected() const noexcept { return mSelected; }

  /** The size the rows and padding need at the current font. */
  [[nodiscard]] Size preferredSize() const;
  [[nodiscard]] float rowHeight() const;

  void drawSelf(RenderContext& context) override;

 private:
  std::shared_ptr<const Font> mFont;
  std::vector<std::string> mCandidates;
  size_t mSelected = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_CANDIDATELISTVIEW_H
