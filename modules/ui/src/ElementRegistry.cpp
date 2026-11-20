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

#include <ui/ElementRegistry.h>

#include <utility>

namespace coral::ui {

bool ElementRegistry::add(std::string type, Builder builder, ElementProps defaults) {
  if (type.empty() || !builder) return false;
  mBuilders[std::move(type)] = Registered{ std::move(builder), std::move(defaults) };
  return true;
}

namespace {

/** Merges the registered defaults under the caller's props (the caller wins every set field). */
[[nodiscard]] ElementProps mergeDefaults(ElementProps defaults, const ElementProps& caller) {
  if (caller.bounds) defaults.bounds = caller.bounds;
  if (caller.background) defaults.background = caller.background;
  if (caller.backgroundRole) defaults.backgroundRole = caller.backgroundRole;
  if (caller.cornerRadius) defaults.cornerRadius = caller.cornerRadius;
  if (caller.shadow) defaults.shadow = caller.shadow;
  if (caller.layoutTransition) defaults.layoutTransition = caller.layoutTransition;
  if (caller.ref) defaults.ref = caller.ref;
  defaults.layout = caller.layout;
  defaults.visible = caller.visible;
  // 1.0 is the ElementProps default and indistinguishable from "unset"; a caller that wants full
  // opacity over a default of 0.5 explicitly passes 1.0.
  defaults.opacity = caller.opacity != 1.0f ? caller.opacity : defaults.opacity;
  defaults.clip = caller.clip;
  return defaults;
}

} // namespace

bool ElementRegistry::contains(const std::string_view type) const {
  return mBuilders.find(std::string(type)) != mBuilders.end();
}

ElementPtr ElementRegistry::build(const std::string_view type, ElementProps props,
    std::vector<ElementPtr> children, std::string key) const {
  const auto position = mBuilders.find(std::string(type));
  if (position == mBuilders.end()) return nullptr;
  const ElementProps merged = mergeDefaults(position->second.defaults, props);
  ElementPtr built = position->second.builder(merged, std::move(children));
  if (built == nullptr) return nullptr;
  // The wrapper carries the merged props (defaults included) and the optional key.
  return components::WithProps(std::move(built), merged, std::move(key));
}

std::vector<std::string> ElementRegistry::types() const {
  std::vector<std::string> names;
  names.reserve(mBuilders.size());
  for (const auto& entry: mBuilders) names.push_back(entry.first);
  return names;
}

void ElementRegistry::clear() {
  mBuilders.clear();
}

ElementRegistry& ElementRegistry::shared() {
  static ElementRegistry registry;
  return registry;
}

} // namespace coral::ui
