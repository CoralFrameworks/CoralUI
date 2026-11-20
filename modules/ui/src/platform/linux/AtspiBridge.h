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

#ifndef CORAL_UI_PLATFORM_LINUX_ATSPIBRIDGE_H
#define CORAL_UI_PLATFORM_LINUX_ATSPIBRIDGE_H

#include <ui/Accessibility.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui::detail {

/**
 * Publishes one window's snapshot on the AT-SPI accessibility bus.
 *
 * The connection and the application root are process-wide; this object registers a window under
 * that root and unregisters it on destruction. `pump()` dispatches D-Bus method calls on the UI
 * thread, so `provider` and `invoker` never run on a client's thread.
 */
class AtspiBridge {
 public:
  virtual ~AtspiBridge() = default;
  virtual void pump() = 0;
  virtual void setTitle(std::string title) = 0;
  /** Announces that keyboard focus moved to a node in this window. */
  virtual void raiseFocusEvent() = 0;
  /** Announces one value/state/structure change the window already resolved to a node. */
  virtual void raisePropertyEvent(std::string_view relativePath, AccessibilityEvent event) = 0;
};

/**
 * Registers @p provider as an AT-SPI frame.
 *
 * Returns null when the session has no `org.a11y.Bus` (for example a bare Xvfb CI run), so callers
 * can keep the window fully functional without an accessibility bus.
 */
[[nodiscard]] std::unique_ptr<AtspiBridge> createAtspiBridge(
  std::function<AccessibilityNode()> provider, AccessibilityActionInvoker invoker,
  std::string title);

} // namespace coral::ui::detail

#endif // CORAL_UI_PLATFORM_LINUX_ATSPIBRIDGE_H
