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

#ifndef CORAL_UI_DSLCORE_H
#define CORAL_UI_DSLCORE_H

#include <ui/Element.h>
#include <ui/Theme.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui::dsl {

/**
 * A build-scoped theme.
 *
 * The DSL reads the current theme from here; widgets that do not pass an explicit font or colour use
 * `theme.body`/`theme.title` and the theme's text roles. Scopes nest, and the previous theme is
 * restored on destruction.
 */
class ThemeScope {
 public:
  explicit ThemeScope(const Theme& theme);
  ~ThemeScope();

  ThemeScope(const ThemeScope&) = delete;
  ThemeScope& operator=(const ThemeScope&) = delete;

 private:
  const Theme* mPrevious = nullptr;
};

/** The active theme, or the default dark theme outside a `ThemeScope`. */
[[nodiscard]] const Theme& theme();

struct StackOptions {
  float gap = 0.0f;
  Insets padding{};
  Align align = Align::Stretch;
  std::string key;
};

struct GridOptions {
  int columns = 2;
  float gap = 8.0f;
  Insets padding{};
  std::string key;
};

struct TextOptions {
  std::shared_ptr<const graphics::Font> font;
  std::optional<Color> color;
  std::string key;
};

struct ButtonOptions {
  std::shared_ptr<const graphics::Font> font;
  bool enabled = true;
  std::string key;
};

template <class... Children>
[[nodiscard]] ElementPtr Column(StackOptions options, Children&&... children) {
  std::vector<ElementPtr> list;
  list.reserve(sizeof...(Children));
  (list.push_back(std::forward<Children>(children)), ...);
  return std::make_shared<StackElement>(Axis::Column, std::move(list), std::move(options.key),
    options.gap, options.padding, options.align);
}

template <class... Children>
[[nodiscard]] ElementPtr Column(Children&&... children) {
  return Column(StackOptions{}, std::forward<Children>(children)...);
}

[[nodiscard]] inline ElementPtr Column(StackOptions options,
    std::vector<ElementPtr> children) {
  return std::make_shared<StackElement>(Axis::Column, std::move(children),
    std::move(options.key), options.gap, options.padding, options.align);
}

template <class... Children>
[[nodiscard]] ElementPtr Row(StackOptions options, Children&&... children) {
  std::vector<ElementPtr> list;
  list.reserve(sizeof...(Children));
  (list.push_back(std::forward<Children>(children)), ...);
  return std::make_shared<StackElement>(Axis::Row, std::move(list), std::move(options.key),
    options.gap, options.padding, options.align);
}

template <class... Children>
[[nodiscard]] ElementPtr Row(Children&&... children) {
  return Row(StackOptions{}, std::forward<Children>(children)...);
}

[[nodiscard]] inline ElementPtr Row(StackOptions options, std::vector<ElementPtr> children) {
  return std::make_shared<StackElement>(Axis::Row, std::move(children), std::move(options.key),
    options.gap, options.padding, options.align);
}

template <class... Children>
[[nodiscard]] ElementPtr Grid(GridOptions options, Children&&... children) {
  std::vector<ElementPtr> list;
  list.reserve(sizeof...(Children));
  (list.push_back(std::forward<Children>(children)), ...);
  return std::make_shared<GridElement>(options.columns, std::move(list), std::move(options.key),
    options.gap, options.padding);
}

template <class... Children>
[[nodiscard]] ElementPtr Grid(const int columns, Children&&... children) {
  return Grid(GridOptions{ .columns = columns }, std::forward<Children>(children)...);
}

[[nodiscard]] inline ElementPtr Grid(GridOptions options, std::vector<ElementPtr> children) {
  return std::make_shared<GridElement>(options.columns, std::move(children),
    std::move(options.key), options.gap, options.padding);
}

[[nodiscard]] inline ElementPtr Text(std::string text, TextOptions options = {}) {
  const Theme& current = theme();
  return std::make_shared<TextElement>(std::move(text),
    options.font != nullptr ? options.font : current.body,
    options.color.value_or(current.text), std::move(options.key));
}

[[nodiscard]] inline ElementPtr Text(std::string text, const Color color, std::string key = {}) {
  return Text(std::move(text), TextOptions{ {}, color, std::move(key) });
}

[[nodiscard]] inline ElementPtr Title(std::string text, std::string key = {}) {
  return Text(std::move(text), TextOptions{ theme().title, theme().text, std::move(key) });
}

[[nodiscard]] inline ElementPtr Caption(std::string text, std::string key = {}) {
  return Text(std::move(text), TextOptions{ theme().body, theme().textMuted, std::move(key) });
}

[[nodiscard]] inline ElementPtr Button(std::string label, std::function<void()> onClick = {},
    ButtonOptions options = {}) {
  return components::Button(std::move(label),
    options.font != nullptr ? options.font : theme().body, std::move(onClick),
    std::move(options.key), options.enabled);
}

[[nodiscard]] inline ElementPtr Spacer(std::string key = "dsl.spacer") {
  ElementProps props;
  props.layout.grow = 1.0f;
  props.layout.shrink = 1.0f;
  return components::WithProps(
    std::make_shared<ColorElement>(Color::MakeRGBA8(0, 0, 0, 0)), props, std::move(key));
}

[[nodiscard]] inline ElementPtr FormField(std::string label, ElementPtr content,
    std::string help = {}, std::string key = {}) {
  return components::FormField(std::move(label), theme().body, std::move(content), std::move(help),
    {}, std::move(key));
}

template <class... Children>
[[nodiscard]] ElementPtr Card(std::string title, Children&&... children) {
  auto content = Column(StackOptions{ .gap = 8.0f, .padding = Insets{}.all(4.0f) },
    std::forward<Children>(children)...);
  return components::Card(std::move(title), theme().title, std::move(content), {}, {});
}

template <class... Children>
[[nodiscard]] ElementPtr Section(std::string title, Children&&... children) {
  auto body = Column(StackOptions{ .gap = 8.0f }, std::forward<Children>(children)...);
  auto header = components::SectionHeader(std::move(title), theme().title, {}, {}, {});
  return Column(StackOptions{ .gap = 8.0f }, std::move(header), std::move(body));
}

template <class... Children>
[[nodiscard]] ElementPtr Page(std::string title, Children&&... children) {
  auto body = Column(StackOptions{ .gap = 12.0f }, std::forward<Children>(children)...);
  auto page = Column(StackOptions{ .gap = 16.0f, .padding = Insets{}.all(24.0f) },
    Title(std::move(title)), std::move(body));
  ElementProps props;
  props.layout.grow = 1.0f;
  props.layout.shrink = 1.0f;
  props.backgroundRole = ThemeColorRole::Background;
  return components::WithProps(std::move(page), props, "dsl.page");
}

/** A solid swatch with a fixed radius; the primitive behind colour wells. */
[[nodiscard]] inline ElementPtr Swatch(Color color, const float width, const float height,
    const float radius = 0.0f, std::string key = {}) {
  ElementProps props;
  props.bounds = Rect::MakeWH(width, height);
  props.background = color;
  props.cornerRadius = radius;
  // A rounded mask on a tiny leaf is not worth the fractional-DPI clip risk in the demo.
  props.clip = false;
  props.layout.minWidth = width;
  props.layout.minHeight = height;
  return components::WithProps(std::make_shared<ColorElement>(color, key + ".color"), props,
    std::move(key));
}
/** Hosts a retained Layer in a DSL tree; use for custom drawing surfaces. */
[[nodiscard]] inline ElementPtr Canvas(std::shared_ptr<Layer> layer, const float minHeight = 0.0f,
    std::string key = {}) {
  ElementProps props;
  if (minHeight > 0.0f) props.layout.minHeight = minHeight;
  props.layout.grow = 1.0f;
  props.layout.shrink = 1.0f;
  return components::WithProps(components::Layer(std::move(layer)), props, std::move(key));
}

/**
 * Maps a typed range to a vertical stack of keyed DSL rows.
 *
 * The builder receives the item and its index; pass an explicit key to each returned element when the
 * order can change.
 */
template <class T, class Build>
[[nodiscard]] ElementPtr ForEach(const std::vector<T>& items, Build&& build,
    StackOptions options = {}) {
  return Column(std::move(options),
    components::ForEach(items, std::forward<Build>(build)));
}

/** A keyed conditional branch. Both branches must be DSL elements. */
[[nodiscard]] inline ElementPtr When(const bool condition, ElementPtr thenBranch,
    ElementPtr elseBranch = {}, std::string key = {}) {
  return components::When(condition, std::move(thenBranch), std::move(elseBranch),
    std::move(key));
}
} // namespace coral::ui::dsl

#endif // CORAL_UI_DSLCORE_H
