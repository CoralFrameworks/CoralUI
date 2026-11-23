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

#ifndef CORAL_UI_APPLICATIONLIFECYCLE_H
#define CORAL_UI_APPLICATIONLIFECYCLE_H

#include <functional>
#include <string>
#include <vector>

namespace coral::ui {

/**
 * Process-wide handlers for application lifecycle events the OS raises outside a window.
 *
 * The macOS AppKit delegate forwards open/reopen/quit here. They are process-wide rather than
 * per-Application because AppKit owns exactly one application object; a second Application in the
 * same process would share them.
 */

/** Called with file URLs from Finder/Dock open events, on the UI thread. */
void setOpenFilesHandler(std::function<void(std::vector<std::string>)> handler);

/** Called when the user reopens the app (Dock click with no visible window), on the UI thread. */
void setReopenHandler(std::function<void()> handler);

/**
 * Called before AppKit terminates on Cmd+Q. Return false to veto.
 *
 * A product that owns close interceptors should request its windows close here and return false,
 * then quit from the close completion; the framework does not guess how application state maps onto
 * the window set.
 */
void setShouldTerminateHandler(std::function<bool()> handler);

} // namespace coral::ui

#endif // CORAL_UI_APPLICATIONLIFECYCLE_H
