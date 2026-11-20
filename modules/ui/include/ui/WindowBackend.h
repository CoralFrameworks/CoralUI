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

#ifndef CORAL_UI_WINDOWBACKEND_H
#define CORAL_UI_WINDOWBACKEND_H

#include <ui/RendererDevice.h>
#include <ui/Window.h>

#include <backend/Device.h>

namespace coral::ui::detail {

/**
 * Advanced bridge for content producers and conformance tests that need the RHI device behind an
 * opaque `RendererDevice`. Ordinary UI code includes `ui/Window.h` and never sees this header.
 */
inline backend::Device* deviceOf(const RendererDevice& device) {
  return static_cast<backend::Device*>(device.nativeHandle());
}

inline backend::Device* deviceOf(const Window& window) {
  const std::shared_ptr<RendererDevice> device = window.rendererDevice();
  return device != nullptr ? deviceOf(*device) : nullptr;
}

} // namespace coral::ui::detail

#endif // CORAL_UI_WINDOWBACKEND_H
