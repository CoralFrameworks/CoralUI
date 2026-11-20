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

#include <ui/IconTheme.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

void IconTheme::add(std::string name, std::shared_ptr<const Image> image, const float scale) {
  if (name.empty() || image == nullptr || image->isEmpty() || image->width() <= 0) return;
  std::vector<IconVariant> variants;
  variants.push_back(IconVariant{ std::move(image), std::max(0.01f, scale) });
  add(std::move(name), std::move(variants));
}

void IconTheme::add(std::string name, std::vector<IconVariant> variants) {
  if (name.empty() || variants.empty()) return;
  variants.erase(std::remove_if(variants.begin(), variants.end(),
    [](const IconVariant& variant) {
      return variant.image == nullptr || variant.image->isEmpty() || variant.image->width() <= 0;
    }), variants.end());
  if (variants.empty()) return;
  auto found = mEntries.find(name);
  if (found == mEntries.end()) {
    mNames.push_back(name);
    mEntries.emplace(std::move(name), Entry{ std::move(variants) });
    return;
  }
  auto& existing = found->second.variants;
  existing.insert(existing.end(), variants.begin(), variants.end());
}

bool IconTheme::contains(const std::string_view name) const {
  return mEntries.find(std::string(name)) != mEntries.end();
}

const Image* IconTheme::resolve(const std::string_view name, const float logicalSize) const {
  const auto found = mEntries.find(std::string(name));
  if (found == mEntries.end()) return nullptr;
  const Image* best = nullptr;
  float bestScore = 0.0f;
  for (const IconVariant& variant: found->second.variants) {
    const float intrinsic = static_cast<float>(variant.image->width()) / variant.scale;
    // Prefer the smallest sufficient asset; if none is sufficient, use the closest asset.
    const float score = intrinsic >= logicalSize
      ? intrinsic
      : 100000.0f - intrinsic;
    if (best == nullptr || score < bestScore) {
      best = variant.image.get();
      bestScore = score;
    }
  }
  return best;
}

} // namespace coral::ui
