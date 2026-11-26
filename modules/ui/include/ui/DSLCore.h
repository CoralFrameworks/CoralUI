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

#include <ui/Component.h>
#include <ui/Element.h>
#include <ui/Theme.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui::dsl {

/** The stable-core API revision. Additive changes keep the major value; removals require a bump. */
inline constexpr int kApiVersion = 1;

/** A spacing density for a DSL build. Typography and colours are unchanged. */
enum class Density {
  Comfortable,
  Compact,
};

/**
 * A build-scoped theme.
 *
 * The DSL reads the current theme from here; widgets that do not pass an explicit font or colour use
 * `theme.body`/`theme.title` and the theme's text roles. Scopes nest, and the previous theme is
 * restored on destruction. A compact scope owns a copy with its spacing scale reduced.
 */
class ThemeScope {
 public:
  explicit ThemeScope(const Theme& theme);
  ThemeScope(const Theme& theme, Density density);
  ~ThemeScope();

  ThemeScope(const ThemeScope&) = delete;
  ThemeScope& operator=(const ThemeScope&) = delete;

 private:
  const Theme* mPrevious = nullptr;
  std::optional<Theme> mOwned;
};

/** The active theme, or the default dark theme outside a `ThemeScope`. */
[[nodiscard]] const Theme& theme();

/**
 * A value snapshot of the design tokens the DSL uses during a build.
 *
 * The snapshot is intentional: a build must not observe a theme half-way through a control. Read it
 * once at the start of a component and pass the values down.
 */
struct Tokens {
  Spacing spacing;
  Radius radius;
  FontSizes fontSizes;
  Color background;
  Color surface;
  Color surfaceRaised;
  Color text;
  Color textMuted;
  Color accent;
  ControlColors controls;
  std::shared_ptr<const graphics::Font> body;
  std::shared_ptr<const graphics::Font> title;
};

[[nodiscard]] inline Tokens tokens() {
  const Theme& current = theme();
  return Tokens{ current.spacing, current.radius, current.fonts, current.background,
    current.surface, current.surfaceRaised, current.text, current.textMuted, current.accent,
    current.controls, current.body, current.title };
}

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
  /** Optional named style installed through Window::setStyleSheet (e.g. "button.primary"). */
  std::string styleName;
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

/** The large display role. It uses the theme's title face until the theme carries a display face. */
[[nodiscard]] inline ElementPtr Display(std::string text, std::string key = {}) {
  return Text(std::move(text), TextOptions{ theme().title, theme().text, std::move(key) });
}

/** A section heading. */
[[nodiscard]] inline ElementPtr Heading(std::string text, std::string key = {}) {
  return Text(std::move(text), TextOptions{ theme().title, theme().text, std::move(key) });
}

/** Body copy. */
[[nodiscard]] inline ElementPtr Body(std::string text, std::string key = {}) {
  return Text(std::move(text), TextOptions{ theme().body, theme().text, std::move(key) });
}

/** A control or form label. */
[[nodiscard]] inline ElementPtr Label(std::string text, std::string key = {}) {
  return Text(std::move(text), TextOptions{ theme().body, theme().controls.foreground, std::move(key) });
}

[[nodiscard]] inline ElementPtr Button(std::string label, std::function<void()> onClick = {},
    ButtonOptions options = {}) {
  auto button = components::Button(std::move(label),
    options.font != nullptr ? options.font : theme().body, std::move(onClick), options.key,
    options.enabled);
  if (options.styleName.empty()) return button;
  ElementProps props;
  props.styleName = std::move(options.styleName);
  return components::WithProps(std::move(button), props,
    options.key.empty() ? "dsl.button.style" : options.key + ".style");
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

struct PanelOptions {
  /** -1 means "use theme.spacing.md". */
  float padding = -1.0f;
  /** -1 means "use theme.spacing.sm". */
  float gap = -1.0f;
  /** Use the raised surface role instead of the normal surface role. */
  bool raised = false;
  /** -1 means "use theme.radius.md". */
  float radius = -1.0f;
  std::string key;
};

/**
 * A default-styled surface container without a title.
 *
 * This is the design-system answer to "wrap children in a surface": callers do not need to construct
 * `ElementProps` or know which colour role a panel uses.
 */
template <class... Children>
[[nodiscard]] ElementPtr Panel(PanelOptions options, Children&&... children) {
  const Tokens t = tokens();
  const float padding = options.padding >= 0.0f ? options.padding : t.spacing.md;
  const float gap = options.gap >= 0.0f ? options.gap : t.spacing.sm;
  auto content = Column(StackOptions{ .gap = gap, .padding = Insets{}.all(padding) },
    std::forward<Children>(children)...);
  ElementProps props;
  props.backgroundRole = options.raised ? ThemeColorRole::SurfaceRaised : ThemeColorRole::Surface;
  props.cornerRadius = options.radius >= 0.0f ? options.radius : t.radius.md;
  props.clip = true;
  return components::WithProps(std::move(content), props,
    options.key.empty() ? "dsl.panel" : std::move(options.key));
}

template <class... Children>
[[nodiscard]] ElementPtr Panel(Children&&... children) {
  return Panel(PanelOptions{}, std::forward<Children>(children)...);
}
template <class... Children>
[[nodiscard]] ElementPtr Card(std::string title, Children&&... children) {
  const Tokens t = tokens();
  auto content = Column(StackOptions{ .gap = t.spacing.sm, .padding = Insets{}.all(t.spacing.xs) },
    std::forward<Children>(children)...);
  return components::Card(std::move(title), theme().title, std::move(content), {}, {});
}

template <class... Children>
[[nodiscard]] ElementPtr Section(std::string title, Children&&... children) {
  const Tokens t = tokens();
  auto body = Column(StackOptions{ .gap = t.spacing.sm }, std::forward<Children>(children)...);
  auto header = components::SectionHeader(std::move(title), t.title, {}, {}, {});
  return Column(StackOptions{ .gap = t.spacing.sm }, std::move(header), std::move(body));
}

template <class... Children>
[[nodiscard]] ElementPtr Page(std::string title, Children&&... children) {
  const Tokens t = tokens();
  auto body = Column(StackOptions{ .gap = t.spacing.md }, std::forward<Children>(children)...);
  auto page = Column(StackOptions{ .gap = t.spacing.lg, .padding = Insets{}.all(t.spacing.xl) },
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
  props.clip = true;
  props.layout.minWidth = width;
  props.layout.minHeight = height;
  return components::WithProps(std::make_shared<ColorElement>(color, key + ".color"), props,
    std::move(key));
}
struct CanvasOptions {
  float minWidth = 0.0f;
  float minHeight = 0.0f;
  bool grow = true;
  bool shrink = true;
  std::string key;
};

/** Hosts a retained Layer in a DSL tree; use for custom drawing surfaces. */
[[nodiscard]] inline ElementPtr Canvas(std::shared_ptr<Layer> layer, CanvasOptions options) {
  ElementProps props;
  if (options.minWidth > 0.0f) props.layout.minWidth = options.minWidth;
  if (options.minHeight > 0.0f) props.layout.minHeight = options.minHeight;
  props.layout.grow = options.grow ? 1.0f : 0.0f;
  props.layout.shrink = options.shrink ? 1.0f : 0.0f;
  return components::WithProps(components::Layer(std::move(layer)), props,
    options.key.empty() ? "dsl.canvas" : std::move(options.key));
}

[[nodiscard]] inline ElementPtr Canvas(std::shared_ptr<Layer> layer, const float minHeight = 0.0f,
    std::string key = {}) {
  return Canvas(std::move(layer),
    CanvasOptions{ .minHeight = minHeight, .key = std::move(key) });
}

/** Hosts a raw Layer under a stable DSL-only name. */
[[nodiscard]] inline ElementPtr RawLayer(std::shared_ptr<Layer> layer, std::string key = {}) {
  return components::Layer(std::move(layer), std::move(key));
}
/**
 * A pure component: a build function that returns a DSL subtree.
 *
 * This is the stable custom-control entry point for reusable UI made of DSL nodes. A control that
 * needs onMount/onUpdate/onUnmount, providers or an error boundary should be a
 * coral::ui::Component mounted through ComponentHost or ChildComponent, where the owning context
 * supplies the invalidation route.
 */
[[nodiscard]] inline ElementPtr Component(std::function<ElementPtr()> build,
    std::string key = {}) {
  return components::Component(std::move(build), std::move(key));
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
