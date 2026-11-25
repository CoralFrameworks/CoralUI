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

#ifndef CORAL_UI_TIMELINE_H
#define CORAL_UI_TIMELINE_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class TextLayer;

enum class TimelineState {
  Complete,
  Current,
  Pending,
};

struct TimelineEntry {
  std::string title;
  std::string detail;
  std::string time;
  TimelineState state = TimelineState::Pending;
};

/** A vertical activity timeline with state dots and two text rows per entry. */
class Timeline final : public Layer {
 public:
  Timeline(std::vector<TimelineEntry> entries, std::shared_ptr<const Font> font);

  void setEntries(std::vector<TimelineEntry> entries);
  [[nodiscard]] const std::vector<TimelineEntry>& entries() const noexcept {
    return mEntries;
  }
  void setFont(std::shared_ptr<const Font> font);

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "timeline";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  struct Row {
    std::shared_ptr<TextLayer> text;
    std::shared_ptr<TextLayer> detail;
    std::shared_ptr<TextLayer> time;
  };
  void rebuild();
  void updateColors();

  std::vector<TimelineEntry> mEntries;
  std::vector<Row> mRows;
  std::shared_ptr<const Font> mFont;
  Color mComplete = Color::MakeRGBA8(80, 200, 140, 255);
  Color mCurrent = Color::MakeRGBA8(86, 156, 255, 255);
  Color mPending = Color::MakeRGBA8(90, 100, 116, 255);
  Color mTitle = Color::MakeRGBA8(226, 232, 240, 255);
  Color mDetail = Color::MakeRGBA8(148, 163, 184, 255);
};

} // namespace coral::ui

#endif // CORAL_UI_TIMELINE_H
