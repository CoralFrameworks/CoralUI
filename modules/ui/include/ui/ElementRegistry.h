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

#ifndef CORAL_UI_ELEMENTREGISTRY_H
#define CORAL_UI_ELEMENTREGISTRY_H

#include <ui/Element.h>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui {

/**
 * Registers element builders under a type name.
 *
 * The built-in kinds are a closed enum because the reconciler has to know how to patch each one. A
 * registered builder is the escape hatch: an application registers `"Card"` once and a DSL or a
 * builder can then instantiate it by name, while the reconciler only ever sees the ordinary elements
 * the builder returns. That keeps the layer patch logic closed and the extension point open.
 *
 * Registration happens on the UI thread during startup, before the first build. Re-registering a name
 * replaces the previous builder, which is what a hot-reload path wants; `add` returns false when the
 * name is empty or the builder is null.
 */
class ElementRegistry {
 public:
  using Builder = std::function<ElementPtr(const ElementProps& props,
    std::vector<ElementPtr> children)>;

  /**
   * Registers @p type.
   *
   * @param defaults style/layout defaults applied to every instance; the caller's own props win, so a
   *                 DSL can inherit a card's padding/background while overriding one field.
   */
  bool add(std::string type, Builder builder, ElementProps defaults = {});
  [[nodiscard]] bool contains(std::string_view type) const;

  /**
   * Instantiates @p type.
   *
   * @param props passed to the builder (the wrapper itself carries no styling).
   * @param key when set, the result is wrapped so the keyed identity survives reconciliation.
   * @return null when @p type is not registered.
   */
  [[nodiscard]] ElementPtr build(std::string_view type, ElementProps props = {},
    std::vector<ElementPtr> children = {}, std::string key = {}) const;

  [[nodiscard]] std::vector<std::string> types() const;
  [[nodiscard]] std::size_t size() const noexcept {
    return mBuilders.size();
  }
  void clear();

  /** The process-wide registry a DSL resolves names against; an app may use its own instead. */
  [[nodiscard]] static ElementRegistry& shared();

 private:
  struct Registered {
    Builder builder;
    ElementProps defaults;
  };
  std::map<std::string, Registered> mBuilders;
};

} // namespace coral::ui

#endif // CORAL_UI_ELEMENTREGISTRY_H
