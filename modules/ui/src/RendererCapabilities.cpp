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

#include <ui/RendererCapabilities.h>

#include <backend/Capabilities.h>
#include <backend/Device.h>
#include <backend/Types.h>

namespace coral::ui {

RendererCapabilities RendererCapabilities::raster() {
  RendererCapabilities capabilities;
  capabilities.backend = "software";
  capabilities.recording = false;
  capabilities.sampleCount = 1;
  capabilities.externalImages = { false,
    "a software canvas has no device texture to sample" };
  capabilities.layerMasks = { true, "" };
  // `Canvas::clipPath` is rasterised into a coverage mask on the CPU, so a shaped clip works.
  capabilities.shapedClips = { true, "" };
  capabilities.groupOpacity = { true, "" };
  capabilities.blendModes = { true, "" };
  capabilities.damageRects = { true, "partial updates are a hint; a platform without a preserved target repaints the window" };
  capabilities.readback = { true, "" };
  return capabilities;
}

RendererCapabilities queryRendererCapabilities(backend::Device* const device, const bool recording,
    const uint32_t sampleCount) {
  if (device == nullptr || !recording) {
    RendererCapabilities capabilities = RendererCapabilities::raster();
    if (device != nullptr) {
      capabilities.backend = "recording (no device probe)";
    }
    capabilities.sampleCount = sampleCount;
    return capabilities;
  }

  const backend::DeviceInfo& info = device->getInfo();
  const backend::Capabilities& limits = device->getCapabilities();

  RendererCapabilities capabilities;
  capabilities.backend = toString(info.backend);
  if (info.apiVersion != nullptr && info.apiVersion[0] != '\0') {
    if (!capabilities.backend.empty()) capabilities.backend += ' ';
    capabilities.backend += info.apiVersion;
  }
  if (info.deviceName != nullptr) capabilities.device = info.deviceName;
  capabilities.recording = true;
  capabilities.sampleCount = sampleCount == 0 ? 1 : sampleCount;
  capabilities.maxTextureDimension = limits.limits.maxTextureDimension2D;
  // The RHI's Limits has no separate viewport field. A viewport is bounded by the target size, which
  // is bounded by the texture dimension, so this is the conservative answer rather than a guess.
  capabilities.maxViewport = limits.limits.maxTextureDimension2D;

  capabilities.externalImages = { true, "" };
  capabilities.layerMasks = { true, "" };
  // A shaped clip is honoured: the renderer turns a masked run into a masked layer, which is the
  // same composite path `Canvas::maskLayer` uses. It is not a per-command texture sample, and the
  // distinction is documented here rather than left for a reader to infer.
  capabilities.shapedClips = { true, "" };
  capabilities.groupOpacity = { true, "" };
  capabilities.blendModes = { true, "" };
  capabilities.damageRects = { true, "partial updates are a hint; a platform without a preserved target repaints the window" };
  capabilities.readback = { true, "" };
  return capabilities;
}

} // namespace coral::ui

