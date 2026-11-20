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

#include <ui/Style.h>

#include <utility>

namespace coral::ui {

namespace {

void applyPatch(ResolvedStyle& resolved, const StylePatch& patch) {
  if (patch.background) resolved.background = *patch.background;
  if (patch.foreground) resolved.foreground = *patch.foreground;
  if (patch.border) resolved.border = *patch.border;
  if (patch.borderWidth) resolved.borderWidth = *patch.borderWidth;
  if (patch.radius) resolved.radius = *patch.radius;
  if (patch.padding) resolved.padding = *patch.padding;
  if (patch.font) resolved.font = *patch.font;
}

} // namespace

float fontSize(const Theme& theme, const FontRole role) noexcept {
  switch (role) {
    case FontRole::Caption: return theme.fonts.caption;
    case FontRole::Body: return theme.fonts.body;
    case FontRole::Title: return theme.fonts.title;
    case FontRole::Heading: return theme.fonts.heading;
    case FontRole::Display: return theme.fonts.display;
  }
  return theme.fonts.body;
}

ResolvedStyle resolveStyle(const Style& style, const StateMask active, const Theme& theme) {
  ResolvedStyle resolved;
  // The theme's control tokens already encode the interaction order, so an unstyled control still
  // reacts to hover/press/focus. Disabled is applied last because it wins over every other state.
  resolved.background = theme.controls.background;
  resolved.foreground = theme.controls.foreground;
  resolved.border = theme.controls.border;
  resolved.borderWidth = 1.0f;
  resolved.radius = theme.radius.md;
  resolved.padding = Insets{ theme.spacing.sm, theme.spacing.xs, theme.spacing.sm,
    theme.spacing.xs };
  resolved.font = FontRole::Body;

  if (matchesStates(active, stateFlag(ControlState::Focused))) {
    resolved.border = theme.controls.borderFocused;
  }
  if (matchesStates(active, stateFlag(ControlState::Hovered))) {
    resolved.background = theme.controls.backgroundHover;
  }
  if (matchesStates(active, stateFlag(ControlState::Pressed))) {
    resolved.background = theme.controls.backgroundPressed;
  }
  if (matchesStates(active, stateFlag(ControlState::Selected)) ||
      matchesStates(active, stateFlag(ControlState::Checked))) {
    resolved.border = theme.controls.accent;
  }
  if (matchesStates(active, stateFlag(ControlState::Disabled))) {
    resolved.background = theme.controls.disabled;
    resolved.foreground = theme.controls.muted;
    resolved.border = theme.controls.border;
  }

  applyPatch(resolved, style.base);
  for (const StyleVariant& variant: style.variants) {
    if (matchesStates(active, variant.when)) applyPatch(resolved, variant.patch);
  }
  return resolved;
}

void StyleSheet::set(std::string name, Style style) {
  if (name.empty()) return;
  mStyles[std::move(name)] = std::move(style);
}

const Style* StyleSheet::find(const std::string_view name) const {
  const auto position = mStyles.find(std::string(name));
  return position == mStyles.end() ? nullptr : &position->second;
}

bool StyleSheet::contains(const std::string_view name) const {
  return find(name) != nullptr;
}

std::vector<std::string> StyleSheet::names() const {
  std::vector<std::string> result;
  result.reserve(mStyles.size());
  for (const auto& entry: mStyles) result.push_back(entry.first);
  return result;
}

std::optional<ResolvedStyle> StyleSheet::resolve(const std::string_view name,
    const StateMask active, const Theme& theme) const {
  const Style* style = find(name);
  if (style == nullptr) return std::nullopt;
  return resolveStyle(*style, active, theme);
}

void StyleSheet::clear() {
  mStyles.clear();
}

} // namespace coral::ui
