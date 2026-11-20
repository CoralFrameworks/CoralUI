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

#ifndef CORAL_UI_THEME_H
#define CORAL_UI_THEME_H

#include <ui/Types.h>
#include <ui/LineBreakPolicy.h>
#include <ui/Translation.h>


#include <memory>

namespace coral::ui {

/** The spacing scale a control uses for its padding and internal gaps. */
struct Spacing {
  float xxs = 2.0f;
  float xs = 4.0f;
  float sm = 8.0f;
  float md = 12.0f;
  float lg = 16.0f;
  float xl = 24.0f;
};

/** The semantic type scale; a control picks a role and reads its size from here. */
struct FontSizes {
  float caption = 11.0f;
  float body = 13.0f;
  float title = 16.0f;
  float heading = 20.0f;
  float display = 26.0f;
};

/** Corner radii; `pill` is intentionally larger than any control height. */
struct Radius {
  float sm = 2.0f;
  float md = 4.0f;
  float lg = 8.0f;
  float pill = 999.0f;
};

/** The state colours shared by interactive controls. */
struct ControlColors {
  Color background = Color::MakeRGBA8(35, 38, 46, 255);
  Color backgroundHover = Color::MakeRGBA8(45, 49, 59, 255);
  Color backgroundPressed = Color::MakeRGBA8(62, 67, 78, 255);
  Color border = Color::MakeRGBA8(62, 67, 78, 255);
  Color borderFocused = Color::MakeRGBA8(86, 156, 255, 255);
  Color foreground = Color::MakeRGBA8(226, 232, 240, 255);
  Color muted = Color::MakeRGBA8(148, 163, 184, 255);
  Color accent = Color::MakeRGBA8(86, 156, 255, 255);
  Color disabled = Color::MakeRGBA8(90, 96, 108, 255);
  Color track = Color::MakeRGBA8(62, 68, 80, 255);
  Color knob = Color::MakeRGBA8(245, 247, 250, 255);
};

/** A soft rounded-rectangle shadow; the raster/GPU canvases have no blur primitive, so it is layered. */
struct Shadow {
  float blurRadius = 12.0f;
  float offsetX = 0.0f;
  float offsetY = 4.0f;
  Color color = Color::MakeRGBA8(0, 0, 0, 110);
};

/** The elevation scale used by floating surfaces. */
struct ShadowTokens {
  Shadow sm{ 8.0f, 0.0f, 2.0f, Color::MakeRGBA8(0, 0, 0, 90) };
  Shadow md{ 16.0f, 0.0f, 6.0f, Color::MakeRGBA8(0, 0, 0, 120) };
  Shadow lg{ 28.0f, 0.0f, 10.0f, Color::MakeRGBA8(0, 0, 0, 140) };
};

/**
 * The colours, spacing and fonts the built-in layers draw with.
 *
 * Controls copy the tokens they need in `Layer::applyTheme`, so changing the window theme updates an
 * already-built tree instead of requiring every control to be recreated.
 */
struct Theme;

/** The token a themed layer paints with, resolved again whenever the window theme changes. */
enum class ThemeColorRole : uint8_t {
  None,
  Background,
  Surface,
  SurfaceRaised,
  Text,
  TextMuted,
  Accent,
  Scrim,
  /** Control tokens: the same values the built-in controls copy in `applyTheme`. */
  Foreground,
  Border,
  BorderFocused,
  ControlBackground,
  ControlHover,
  ControlPressed,
  Disabled,
  Track,
  Knob,
};

[[nodiscard]] Color themeColor(const Theme& theme, ThemeColorRole role) noexcept;

struct Theme {
  Color background = Color::MakeRGBA8(24, 26, 32, 255);
  Color surface = Color::MakeRGBA8(35, 38, 46, 255);
  Color surfaceRaised = Color::MakeRGBA8(52, 56, 66, 255);
  Color text = Color::MakeRGBA8(226, 232, 240, 255);
  Color textMuted = Color::MakeRGBA8(148, 163, 184, 255);
  Color accent = Color::MakeRGBA8(86, 156, 255, 255);
  Color scrim = Color::MakeRGBA8(0, 0, 0, 150);

  Spacing spacing;
  FontSizes fonts;
  Radius radius;
  ControlColors controls;
  ShadowTokens shadow;
  float focusRingWidth = 1.5f;

  std::shared_ptr<const Font> body;
  std::shared_ptr<const Font> title;

  /**
   * The catalog a control asks for its labels with; null keeps the built-in English strings.
   *
   * Carrying it on the theme is what lets a widget translate what it composes without a second
   * parameter through every Element builder: a page hands its theme down, and `tr` falls back to the
   * literal the control was written with when the catalog has no entry.
   */
  std::shared_ptr<const Translator> translator;

  /** The translation for @p key, or @p fallback when no translator is attached or the key is missing. */
  [[nodiscard]] std::string tr(const std::string_view key, const std::string_view fallback) const {
    if (translator != nullptr && translator->has(key)) return translator->translate(key);
    return std::string(fallback);
  }

  /** Default line-break/hyphenation policy inherited by text controls that have no explicit one. */
  LineBreakPolicy lineBreakPolicy;

  [[nodiscard]] static Theme dark() {
    return Theme{};
  }

  /** A light skin with the same layout tokens; controls are repainted through `Window::setTheme`. */
  [[nodiscard]] static Theme light() {
    Theme theme;
    theme.background = Color::MakeRGBA8(247, 248, 250, 255);
    theme.surface = Color::MakeRGBA8(255, 255, 255, 255);
    theme.surfaceRaised = Color::MakeRGBA8(237, 240, 245, 255);
    theme.text = Color::MakeRGBA8(30, 34, 42, 255);
    theme.textMuted = Color::MakeRGBA8(95, 105, 120, 255);
    theme.accent = Color::MakeRGBA8(30, 100, 220, 255);
    theme.scrim = Color::MakeRGBA8(0, 0, 0, 60);
    theme.controls.background = Color::MakeRGBA8(255, 255, 255, 255);
    theme.controls.backgroundHover = Color::MakeRGBA8(244, 246, 249, 255);
    theme.controls.backgroundPressed = Color::MakeRGBA8(225, 229, 235, 255);
    theme.controls.border = Color::MakeRGBA8(205, 210, 218, 255);
    theme.controls.borderFocused = theme.accent;
    theme.controls.foreground = theme.text;
    theme.controls.muted = theme.textMuted;
    theme.controls.accent = theme.accent;
    theme.controls.disabled = Color::MakeRGBA8(165, 170, 180, 255);
    theme.controls.track = Color::MakeRGBA8(215, 220, 228, 255);
    theme.controls.knob = Color::MakeRGBA8(255, 255, 255, 255);
    return theme;
  }
};

inline Color themeColor(const Theme& theme, const ThemeColorRole role) noexcept {
  switch (role) {
    case ThemeColorRole::Background: return theme.background;
    case ThemeColorRole::Surface: return theme.surface;
    case ThemeColorRole::SurfaceRaised: return theme.surfaceRaised;
    case ThemeColorRole::Text: return theme.text;
    case ThemeColorRole::TextMuted: return theme.textMuted;
    case ThemeColorRole::Accent: return theme.accent;
    case ThemeColorRole::Scrim: return theme.scrim;
    case ThemeColorRole::Foreground: return theme.controls.foreground;
    case ThemeColorRole::Border: return theme.controls.border;
    case ThemeColorRole::BorderFocused: return theme.controls.borderFocused;
    case ThemeColorRole::ControlBackground: return theme.controls.background;
    case ThemeColorRole::ControlHover: return theme.controls.backgroundHover;
    case ThemeColorRole::ControlPressed: return theme.controls.backgroundPressed;
    case ThemeColorRole::Disabled: return theme.controls.disabled;
    case ThemeColorRole::Track: return theme.controls.track;
    case ThemeColorRole::Knob: return theme.controls.knob;
    case ThemeColorRole::None: break;
  }
  return Color{};
}

} // namespace coral::ui

#endif // CORAL_UI_THEME_H
