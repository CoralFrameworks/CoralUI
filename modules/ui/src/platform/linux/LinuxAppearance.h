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

#ifndef CORAL_UI_LINUXAPPEARANCE_H
#define CORAL_UI_LINUXAPPEARANCE_H

#include <ui/SystemAppearance.h>

namespace coral::ui::detail {

/**
 * The Linux desktop's appearance preferences.
 *
 * Read through the XDG desktop portal when it is available (the cross-desktop interface) and through
 * the GNOME GSettings schema as a fallback, because a minimal session may run without a portal and
 * because the portal does not expose the animation policy. The query is cached briefly: a window
 * constructor asks for it once and the answer does not change between two windows created together.
 */
[[nodiscard]] SystemAppearance linuxSystemAppearance();

/** True when the desktop prefers a dark application theme. */
[[nodiscard]] bool linuxPrefersDark();

} // namespace coral::ui::detail

#endif // CORAL_UI_LINUXAPPEARANCE_H
