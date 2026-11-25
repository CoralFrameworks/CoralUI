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

#ifndef CORAL_UI_FONTPANEL_H
#define CORAL_UI_FONTPANEL_H

#include <functional>
#include <memory>

namespace graphics {
class Font;
}

namespace coral::ui {

using FontPanelHandler = std::function<void(std::shared_ptr<const graphics::Font>)>;

/**
 * Opens the platform font panel.
 *
 * macOS uses the shared `NSFontPanel` + `NSFontManager`; every selection is converted back into a
 * Coral `graphics::Font` through CoreText. Other platforms currently return false. The handler is
 * process-wide because the font panel is an application-level panel.
 */
bool showFontPanel(FontPanelHandler handler = {});

void hideFontPanel();

[[nodiscard]] bool supportsFontPanel() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_FONTPANEL_H
