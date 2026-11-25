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

#ifndef CORAL_UI_SYSTEMAPPEARANCE_H
#define CORAL_UI_SYSTEMAPPEARANCE_H

#include <ui/Theme.h>

#include <cstdint>

namespace coral::ui {

/**
 * The colours a platform reports for high-contrast mode, as ARGB.
 *
 * The roles are the ones Windows' `GetSysColor` exposes and every platform has an equivalent for;
 * they are deliberately fewer than `Theme`'s tokens, because the mapping from a system palette to a
 * theme belongs to the framework, not to each backend.
 */
struct SystemColors {
  bool valid = false;
  uint32_t window = 0;        /**< `COLOR_WINDOW`: the page background. */
  uint32_t windowText = 0;    /**< `COLOR_WINDOWTEXT`: the text on it. */
  uint32_t buttonFace = 0;    /**< `COLOR_BTNFACE`: control and raised-surface backgrounds. */
  uint32_t buttonText = 0;    /**< `COLOR_BTNTEXT`: control foreground and borders. */
  uint32_t highlight = 0;     /**< `COLOR_HIGHLIGHT`: accent, focus ring and selection. */
  uint32_t highlightText = 0; /**< `COLOR_HIGHLIGHTTEXT`: text on the highlight. */
  uint32_t grayText = 0;      /**< `COLOR_GRAYTEXT`: muted and disabled text. */
};

/** @return true when @p colors carries the roles a high-contrast theme needs. */
[[nodiscard]] bool isUsable(const SystemColors& colors) noexcept;

/**
 * @return the theme a window uses while the system is in high-contrast mode.
 *
 * The mapping is flat on purpose: system background and text colours, no decorative shadows, a
 * thicker focus ring, and the system highlight as the accent -- the point of the mode is unambiguous
 * edges and contrast, not the app's palette. Fonts and spacing come from the default dark theme, which
 * is what keeps a high-contrast switch from changing the layout.
 */
[[nodiscard]] Theme highContrastTheme(const SystemColors& colors);

/**
 * What the platform says the system looks like right now.
 *
 * A window reads this when it is created and again whenever the platform reports a settings change
 * (`WM_SETTINGCHANGE` on Windows), so the appearance policy lives in the framework and the backends
 * only answer the query.
 */
struct SystemAppearance {
  bool valid = false;
  bool highContrast = false;
  bool preferDark = false;
  bool reduceMotion = false;
  /** NSColor.controlAccentColor as ARGB, or 0 when the platform has no accent concept. */
  uint32_t accentColor = 0;
  SystemColors colors{};
};

} // namespace coral::ui

#endif // CORAL_UI_SYSTEMAPPEARANCE_H
