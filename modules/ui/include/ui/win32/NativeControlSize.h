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

#ifndef CORAL_UI_WIN32_NATIVECONTROLSIZE_H
#define CORAL_UI_WIN32_NATIVECONTROLSIZE_H

#include <cstdint>

namespace coral::ui::win32 {

/**
 * Win32 control size.
 *
 * Windows sizes common controls from the system font and DPI rather than from an NSControlSize-style
 * enum. The wrappers keep the requested value for API parity; `Small`/`Large` currently do not
 * override the system metrics.
 */
enum class NativeControlSize : uint8_t {
  Small,
  Regular,
  Large,
};

} // namespace coral::ui::win32

#endif // CORAL_UI_WIN32_NATIVECONTROLSIZE_H
