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

#ifndef CORAL_EXAMPLES_GALLERY_SECTIONS_H
#define CORAL_EXAMPLES_GALLERY_SECTIONS_H

#include <memory>
#include <string>

namespace coral::ui {
class Layer;
class TextInput;
struct Theme;
} // namespace coral::ui

/**
 * Builds the sectioned control gallery: a scrollable section list on the left and a page of
 * examples on the right. The returned layer owns its child pages for the lifetime of the window.
 */
std::shared_ptr<coral::ui::Layer> buildSectionedGallery(
  const std::shared_ptr<coral::ui::Layer>& page, const coral::ui::Theme& theme,
  bool nativeDisabled, std::shared_ptr<coral::ui::TextInput>& inputField,
  int initialSection = 0, std::string initialControl = {});

#endif // CORAL_EXAMPLES_GALLERY_SECTIONS_H
