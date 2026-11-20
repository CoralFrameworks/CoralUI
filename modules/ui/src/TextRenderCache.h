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

#ifndef CORAL_UI_TEXTRENDERCACHE_H
#define CORAL_UI_TEXTRENDERCACHE_H

#include "TextPainter.h"

#include <graphics/Font.h>
#include <graphics/Paint.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace coral::ui {

/**
 * Per-window cache for text drawn through `RenderContext::drawText`.
 *
 * Custom layers do not have a natural member to hold a `TextPainter`, and allocating a new shaper per
 * frame would defeat the point. The window owns one cache; a painter is keyed by the logical font and
 * the line text, so repeated frames reuse the shaped blob while different strings/fonts get their own
 * entry.
 */
class TextRenderCache {
 public:
  void draw(RenderContext& context, const std::string& text, float x, float baseline,
      const graphics::Font& font, const graphics::Paint& paint) {
    auto& byText = mEntries[&font];
    auto& painter = byText[text];
    if (painter == nullptr) painter = std::make_unique<TextPainter>();
    painter->draw(context, text, x, baseline, font, paint);
  }

 private:
  std::unordered_map<const graphics::Font*,
    std::unordered_map<std::string, std::unique_ptr<TextPainter>>> mEntries;
};

} // namespace coral::ui

#endif // CORAL_UI_TEXTRENDERCACHE_H
