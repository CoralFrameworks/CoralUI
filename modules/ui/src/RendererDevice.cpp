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

#include <ui/RendererDevice.h>

#include <backend/Device.h>

#include <utility>

namespace coral::ui {

struct RendererDevice::Impl {
  backend::Device* borrowed = nullptr;
  std::shared_ptr<backend::Device> owned;
};

RendererDevice::RendererDevice(std::shared_ptr<Impl> impl) : mImpl(std::move(impl)) {}
RendererDevice::~RendererDevice() = default;
RendererDevice::RendererDevice(RendererDevice&&) noexcept = default;
RendererDevice& RendererDevice::operator=(RendererDevice&&) noexcept = default;

bool RendererDevice::valid() const noexcept {
  return mImpl != nullptr && (mImpl->borrowed != nullptr || mImpl->owned != nullptr);
}

void* RendererDevice::nativeHandle() const noexcept {
  if (mImpl == nullptr) return nullptr;
  return mImpl->owned != nullptr ? static_cast<void*>(mImpl->owned.get())
                                 : static_cast<void*>(mImpl->borrowed);
}

std::shared_ptr<RendererDevice> RendererDevice::borrow(backend::Device* device) {
  if (device == nullptr) return nullptr;
  auto impl = std::make_shared<RendererDevice::Impl>();
  impl->borrowed = device;
  return std::shared_ptr<RendererDevice>(new RendererDevice(std::move(impl)));
}

std::shared_ptr<RendererDevice> RendererDevice::own(backend::Device* device) {
  if (device == nullptr) return nullptr;
  auto impl = std::make_shared<RendererDevice::Impl>();
  impl->owned = std::shared_ptr<backend::Device>(device);
  return std::shared_ptr<RendererDevice>(new RendererDevice(std::move(impl)));
}

namespace detail {

backend::Device* rendererDeviceOf(const RendererDevice& device) {
  return static_cast<backend::Device*>(device.nativeHandle());
}

} // namespace detail

} // namespace coral::ui
