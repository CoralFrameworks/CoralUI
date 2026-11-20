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

#ifndef CORAL_UI_RENDERERDEVICE_H
#define CORAL_UI_RENDERERDEVICE_H

#include <memory>

namespace backend {
class Device;
}

namespace coral::ui {

class Window;

/**
 * An opaque, shared GPU device handle.
 *
 * Applications that create several windows can share the same renderer device without naming a
 * backend type. The device is created and owned by the first window; `Application` keeps the
 * handle alive and passes it to later windows. Producers that genuinely need the RHI device use
 * the advanced `ui/WindowBackend.h` bridge, not this public handle.
 */
class RendererDevice {
 public:
  ~RendererDevice();
  RendererDevice(RendererDevice&&) noexcept;
  RendererDevice& operator=(RendererDevice&&) noexcept;

  RendererDevice(const RendererDevice&) = delete;
  RendererDevice& operator=(const RendererDevice&) = delete;

  [[nodiscard]] bool valid() const noexcept;

  /** Opaque native pointer for the advanced producer bridge; not a public backend type. */
  [[nodiscard]] void* nativeHandle() const noexcept;

  /** Advanced: wraps a borrowed RHI device; the caller keeps it alive. */
  static std::shared_ptr<RendererDevice> borrow(backend::Device* device);
  /** Advanced: takes ownership of an RHI device. */
  static std::shared_ptr<RendererDevice> own(backend::Device* device);

 private:
  friend class Window;
  friend class Application;
  struct Impl;
  explicit RendererDevice(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> mImpl;
};

} // namespace coral::ui

#endif // CORAL_UI_RENDERERDEVICE_H
