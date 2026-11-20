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

/**
 * @file SystemAppearance.cpp
 *
 * The framework's half of the high-contrast and system-appearance story: turn a platform's system
 * colours into a theme, and decide when that theme is usable. The platform half is one query
 * (`WindowPlatform::systemAppearance`) and one event (`WindowHost::systemAppearanceChanged`), so a
 * backend has no policy of its own and the two cannot disagree about what high contrast means.
 */

#include <ui/SystemAppearance.h>

#include <cmath>

namespace coral::ui {

namespace {

/** @return the ARGB value as a `Color`; the channels are unpremultiplied, as `Color` expects. */
[[nodiscard]] Color fromArgb(const uint32_t argb) noexcept {
  return Color::MakeRGBA8(static_cast<uint8_t>((argb >> 16) & 0xFFu),
    static_cast<uint8_t>((argb >> 8) & 0xFFu), static_cast<uint8_t>(argb & 0xFFu),
    static_cast<uint8_t>((argb >> 24) & 0xFFu));
}

} // namespace

bool isUsable(const SystemColors& colors) noexcept {
  if (!colors.valid) return false;
  // Every role the theme maps has to be there. A backend that reports only some of them (a platform
  // without a highlight colour, say) keeps the app theme rather than drawing a half-mapped palette.
  const uint32_t required[] = { colors.window, colors.windowText, colors.buttonFace,
    colors.buttonText, colors.highlight, colors.highlightText, colors.grayText };
  for (const uint32_t value: required) {
    if (value == 0) return false;
  }
  return true;
}

Theme highContrastTheme(const SystemColors& colors) {
  Theme theme = Theme::dark();
  theme.background = fromArgb(colors.window);
  theme.surface = theme.background;
  theme.surfaceRaised = fromArgb(colors.buttonFace);
  theme.text = fromArgb(colors.windowText);
  theme.textMuted = fromArgb(colors.grayText);
  theme.accent = fromArgb(colors.highlight);
  // A scrim over content has to be readable rather than pretty: the contrast mode is not a place for
  // a dark veil, so the highlight colour, fully opaque, is what a modal dims to.
  theme.scrim = fromArgb(colors.highlight);
  theme.scrim.alpha = 0.85f;
  theme.focusRingWidth = 2.0f;

  theme.controls.background = fromArgb(colors.buttonFace);
  theme.controls.backgroundHover = fromArgb(colors.highlight);
  theme.controls.backgroundPressed = fromArgb(colors.highlight);
  theme.controls.border = fromArgb(colors.buttonText);
  theme.controls.borderFocused = fromArgb(colors.highlight);
  theme.controls.foreground = fromArgb(colors.buttonText);
  theme.controls.muted = fromArgb(colors.grayText);
  theme.controls.accent = fromArgb(colors.highlight);
  theme.controls.disabled = fromArgb(colors.grayText);
  theme.controls.track = fromArgb(colors.buttonFace);
  theme.controls.knob = fromArgb(colors.windowText);

  // Decorative elevation is noise in this mode: the edges are what carry the hierarchy.
  for (Shadow* shadow: { &theme.shadow.sm, &theme.shadow.md, &theme.shadow.lg }) {
    shadow->color.alpha = 0.0f;
  }
  return theme;
}

} // namespace coral::ui
