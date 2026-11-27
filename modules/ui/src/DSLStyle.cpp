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

#include <ui/DSLStyle.h>

#include <algorithm>

namespace coral::ui::dsl {
namespace {

Color mix(const Color& color, const Color& target, const float amount) {
  const float t = std::clamp(amount, 0.0f, 1.0f);
  return Color::MakeRGBA8(
    static_cast<uint8_t>(std::lround((color.red + (target.red - color.red) * t) * 255.0f)),
    static_cast<uint8_t>(std::lround((color.green + (target.green - color.green) * t) * 255.0f)),
    static_cast<uint8_t>(std::lround((color.blue + (target.blue - color.blue) * t) * 255.0f)),
    static_cast<uint8_t>(std::lround((color.alpha + (target.alpha - color.alpha) * t) * 255.0f)));
}

Style buttonStyle(const Theme& theme, const Color background, const Color foreground,
    const Color border, const Color hover, const Color pressed) {
  Style style;
  style.base.background = background;
  style.base.foreground = foreground;
  style.base.border = border;
  style.base.radius = theme.radius.md;
  style.variant(stateFlag(ControlState::Hovered), StylePatch{ .background = hover });
  style.variant(stateFlag(ControlState::Pressed), StylePatch{ .background = pressed });
  style.variant(stateFlag(ControlState::Focused),
    StylePatch{ .border = theme.controls.borderFocused,
      .borderWidth = theme.focusRingWidth });
  style.variant(stateFlag(ControlState::Disabled),
    StylePatch{ .background = theme.controls.disabled,
      .foreground = theme.controls.muted,
      .border = theme.controls.border });
  return style;
}

} // namespace

std::shared_ptr<const StyleSheet> defaultStyleSheet(const Theme& theme) {
  auto sheet = std::make_shared<StyleSheet>();

  const Color accent = theme.accent;
  sheet->set("button.primary", buttonStyle(theme, accent, Color::White(),
    mix(accent, Color::White(), 0.15f), mix(accent, Color::White(), 0.12f),
    mix(accent, Color::Black(), 0.16f)));

  sheet->set("button.secondary", buttonStyle(theme, theme.controls.background,
    theme.controls.foreground, theme.controls.border, theme.controls.backgroundHover,
    theme.controls.backgroundPressed));

  Style ghost = buttonStyle(theme, Color::MakeRGBA8(0, 0, 0, 0), theme.controls.foreground,
    Color::MakeRGBA8(0, 0, 0, 0), theme.controls.backgroundHover,
    theme.controls.backgroundPressed);
  sheet->set("button.ghost", std::move(ghost));

  const Color danger = Color::MakeRGBA8(214, 69, 69, 255);
  sheet->set("button.danger", buttonStyle(theme, danger, Color::White(),
    mix(danger, Color::White(), 0.12f), mix(danger, Color::White(), 0.10f),
    mix(danger, Color::Black(), 0.18f)));

  return sheet;
}

} // namespace coral::ui::dsl
