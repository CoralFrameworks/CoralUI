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

#ifndef CORAL_UI_ELEMENT_SPEC_H
#define CORAL_UI_ELEMENT_SPEC_H

#include <ui/Element.h>
#include <ui/ElementRegistry.h>

#include <memory>
#include <string_view>

namespace coral::ui {

/** Optional inputs the spec syntax cannot express, such as the font a Text/CheckBox needs. */
struct ElementSpecOptions {
  std::shared_ptr<const graphics::Font> font;
};

/**
 * Parses the v0 flat element spec documented in docs/DSL_SPEC.md.
 *
 * Grammar: `type ("#" key)? (" " name "=" value)*` with values `#RRGGBB`, numbers, quoted strings
 * and `true`/`false`. The result is an ordinary element — the reconciler never sees the text form.
 * Any syntax error, unknown property or unknown type yields a null element; the function does not
 * throw, so a caller can report the failure with its own wording.
 */
[[nodiscard]] ElementPtr parseElementSpec(std::string_view spec,
  const ElementRegistry& registry = ElementRegistry::shared(),
  const ElementSpecOptions& options = {});

} // namespace coral::ui

#endif // CORAL_UI_ELEMENT_SPEC_H
