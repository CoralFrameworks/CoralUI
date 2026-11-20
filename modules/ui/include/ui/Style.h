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

#ifndef CORAL_UI_STYLE_H
#define CORAL_UI_STYLE_H

#include <ui/Geometry.h>
#include <ui/Theme.h>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui {

/** A control's interaction state; styles select variants from these. */
enum class ControlState : uint8_t {
  Normal,
  Hovered,
  Pressed,
  Focused,
  Disabled,
  Selected,
  Checked,
};

/** A set of interaction states; several can be active at once (hover + focused). */
using StateMask = uint32_t;

constexpr StateMask stateFlag(const ControlState state) {
  return state == ControlState::Normal ? 0u : (1u << (static_cast<uint32_t>(state) - 1u));
}

constexpr StateMask states(const std::initializer_list<ControlState> list) {
  StateMask mask = 0;
  for (const ControlState state: list) mask |= stateFlag(state);
  return mask;
}

/** True when every state in @p required is active. An empty mask always matches. */
constexpr bool matchesStates(const StateMask active, const StateMask required) {
  return (active & required) == required;
}

/** Semantic text size; the concrete size comes from the theme's font ladder. */
enum class FontRole : uint8_t {
  Caption,
  Body,
  Title,
  Heading,
  Display,
};

/**
 * One layer of style values; every field is optional so a variant only overrides what it needs.
 *
 * The unset fields fall through to the theme defaults, then to the base patch, then to each matching
 * variant in declaration order.
 */
struct StylePatch {
  std::optional<Color> background;
  std::optional<Color> foreground;
  std::optional<Color> border;
  std::optional<float> borderWidth;
  std::optional<float> radius;
  std::optional<Insets> padding;
  std::optional<FontRole> font;
};

/** A patch that only applies while all of @p when's states are active. */
struct StyleVariant {
  StateMask when = 0;
  StylePatch patch;
};

/**
 * A declarative control style: one base patch plus state variants.
 *
 * ```cpp
 * Style style;
 * style.base.background = Color::MakeRGBA8(30, 34, 42, 255);
 * style.variant(stateFlag(ControlState::Hovered), { .background = hover });
 * style.variant(states({ ControlState::Hovered, ControlState::Focused }), { .border = focus });
 * ```
 *
 * Variants are applied in declaration order and a later match wins, so a compound variant placed
 * after the single-state one overrides it. That is the same precedence a "more specific selector
 * last" CSS sheet produces, without a selector engine.
 */
struct Style {
  StylePatch base;
  std::vector<StyleVariant> variants;

  Style& variant(const StateMask when, StylePatch patch) {
    variants.push_back(StyleVariant{ when, std::move(patch) });
    return *this;
  }
};

/** The concrete values a control paints with after resolving a style. */
struct ResolvedStyle {
  Color background{};
  Color foreground{};
  Color border{};
  float borderWidth = 1.0f;
  float radius = 4.0f;
  Insets padding{};
  FontRole font = FontRole::Body;
};

[[nodiscard]] float fontSize(const Theme& theme, FontRole role) noexcept;

/**
 * Resolves @p style for @p active against @p theme.
 *
 * Theme tokens supply state-aware defaults first (disabled beats pressed beats hovered), then the
 * base patch and every matching variant override them. A style with no values therefore still
 * follows the theme's control colours, which keeps unstyled controls consistent.
 */
[[nodiscard]] ResolvedStyle resolveStyle(const Style& style, StateMask active, const Theme& theme);

/** A named collection of styles; a control resolves one by name. */
class StyleSheet {
 public:
  void set(std::string name, Style style);
  [[nodiscard]] const Style* find(std::string_view name) const;
  [[nodiscard]] bool contains(std::string_view name) const;
  [[nodiscard]] std::vector<std::string> names() const;
  [[nodiscard]] std::optional<ResolvedStyle> resolve(std::string_view name, StateMask active,
    const Theme& theme) const;
  [[nodiscard]] std::size_t size() const noexcept {
    return mStyles.size();
  }
  void clear();

 private:
  std::map<std::string, Style> mStyles;
};

} // namespace coral::ui

#endif // CORAL_UI_STYLE_H
