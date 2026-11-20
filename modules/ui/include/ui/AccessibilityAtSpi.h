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

#ifndef CORAL_UI_ACCESSIBILITY_ATSPI_H
#define CORAL_UI_ACCESSIBILITY_ATSPI_H

#include <ui/Accessibility.h>

#include <string>
#include <vector>

namespace coral::ui {

/**
 * The AT-SPI vocabulary a Linux bridge needs, kept platform-neutral so it can be tested everywhere.
 *
 * AT-SPI identifies a role by name ("push button", "check box", ...) and a feature by interface name
 * ("org.a11y.atspi.Action", ...). This file owns both mappings; the D-Bus transport only serialises the
 * result, which keeps the protocol code free of policy and lets the tests run on any host.
 */
[[nodiscard]] const char* atspiRoleName(AccessibilityRole role);

/** The interfaces @p node exposes, in the order a client should probe them. */
[[nodiscard]] std::vector<std::string> atspiInterfaces(const AccessibilityNode& node);

/** The name a client should announce; falls back to the role when the node has no name. */
[[nodiscard]] std::string atspiName(const AccessibilityNode& node);

} // namespace coral::ui

#endif // CORAL_UI_ACCESSIBILITY_ATSPI_H
