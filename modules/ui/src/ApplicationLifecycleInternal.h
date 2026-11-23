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

#ifndef CORAL_UI_APPLICATIONLIFECYCLEINTERNAL_H
#define CORAL_UI_APPLICATIONLIFECYCLEINTERNAL_H

#include <string>
#include <vector>

namespace coral::ui::detail {

/** @return true when a handler consumed the event. Called on the UI/AppKit main thread. */
bool dispatchOpenFiles(std::vector<std::string> files);
bool dispatchReopen();
/** @return false when application termination is vetoed. */
bool dispatchShouldTerminate();

} // namespace coral::ui::detail

#endif // CORAL_UI_APPLICATIONLIFECYCLEINTERNAL_H
