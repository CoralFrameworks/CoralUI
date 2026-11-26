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

#include <ui/OverlayHost.h>

#include <ui/Reconciler.h>
#include <ui/Window.h>

#include <utility>
#include <vector>

namespace coral::ui {

OverlayHost::OverlayHost(Window& window) : mWindow(&window) {}

OverlayHost::~OverlayHost() {
  dismissAll();
}

uint64_t OverlayHost::show(ElementPtr content, OverlayOptions options) {
  if (mWindow == nullptr || content == nullptr) return 0;
  auto reconciler = std::make_unique<Reconciler>();
  std::shared_ptr<Layer> layer = reconciler->mount(std::move(content));
  if (layer == nullptr) return 0;

  const uint64_t overlayId = mWindow->pushOverlay(layer, std::move(options));
  if (overlayId == 0) return 0;

  mEntries.emplace(overlayId, Entry{ overlayId, std::move(layer), std::move(reconciler) });
  return overlayId;
}

bool OverlayHost::update(const uint64_t id, ElementPtr content) {
  const auto found = mEntries.find(id);
  if (found == mEntries.end() || found->second.reconciler == nullptr || content == nullptr) {
    return false;
  }
  const std::shared_ptr<Layer> before = found->second.reconciler->root();
  found->second.reconciler->update(std::move(content));
  return found->second.reconciler->root() == before;
}

bool OverlayHost::dismiss(const uint64_t id) {
  const auto found = mEntries.find(id);
  if (found == mEntries.end()) return false;
  mWindow->dismissOverlay(id);
  mEntries.erase(found);
  return true;
}

void OverlayHost::dismissAll() {
  std::vector<uint64_t> ids;
  ids.reserve(mEntries.size());
  for (const auto& entry: mEntries) ids.push_back(entry.first);
  for (const uint64_t id: ids) dismiss(id);
}

bool OverlayHost::contains(const uint64_t id) const {
  return mEntries.find(id) != mEntries.end();
}

} // namespace coral::ui
