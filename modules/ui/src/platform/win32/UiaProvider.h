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

#ifndef CORAL_UI_WIN32_UIAPROVIDER_H
#define CORAL_UI_WIN32_UIAPROVIDER_H

#include <ui/Accessibility.h>

namespace coral::ui::detail {

/**
 * Answers `WM_GETOBJECT` for the UI Automation root object.
 *
 * The snapshot is taken on the UI thread when a client asks for it, so the layer tree is only walked
 * while accessibility is actually in use. The returned provider owns an immutable copy of that
 * snapshot, which makes the fragment providers safe to use from whatever thread UI Automation calls
 * them on.
 *
 * @param hwnd     the window the client is asking about
 * @param snapshot the tree to publish, usually `Window::accessibilityTree()`
 * @return the provider to hand to `UiaReturnRawElementProvider`, or null when nothing is exposed
 */
[[nodiscard]] void* createUiaRootProvider(void* hwnd, AccessibilityNode snapshot,
  AccessibilityActionInvoker invoker);

/** A provider for one node addressed by @p path, used when raising a focus-changed event. */
[[nodiscard]] void* createUiaElementProvider(void* hwnd, AccessibilityNode snapshot,
  const std::string& path);

} // namespace coral::ui::detail

#endif // CORAL_UI_WIN32_UIAPROVIDER_H
