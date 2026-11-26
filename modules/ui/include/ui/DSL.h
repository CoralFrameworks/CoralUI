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

#ifndef CORAL_UI_DSL_H
#define CORAL_UI_DSL_H

/**
 * Umbrella header for the DSL.
 *
 * `DSLCore.h` is the stable API contract. The names below are convenience forwarders for the long
 * tail of components and remain experimental; see `docs/dsl-api.md`.
 */
#include <ui/DSLCore.h>
#include <ui/DSLOverlay.h>
#include <ui/DSLStyle.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace coral::ui::dsl {
// Thin forwarding names for the controls that already have a stable component factory. The DSL
// layout/theme helpers above are the new surface; these keep page code in one namespace without
// wrapping every control a second time.
using components::Avatar;
using components::AvatarGroup;
using components::Badge;
using components::Banner;
using components::Breadcrumb;
using components::CheckBox;
using components::CheckBoxGroup;
using components::Chip;
using components::ColorButton;
using components::ColorInput;
using components::ColorPicker;
using components::ComboBox;
using components::Component;
using components::Divider;
using components::Dock;
using components::EmptyState;
using components::Expander;
using components::IconButton;
using components::Kbd;
using components::Link;
using components::ListView;
using components::Meter;
using components::MultiSelect;
using components::NumberField;
using components::Pagination;
using components::PasswordField;
using components::ProgressBar;
using components::ProgressRing;
using components::PropertyRow;
using components::RadioButton;
using components::RadioGroup;
using components::RangeSlider;
using components::Rating;
using components::RecyclingList;
using components::Scroll;
using components::SearchField;
using components::SegmentedControl;
using components::Select;
using components::Skeleton;
using components::Slider;
using components::Spinner;
using components::SplitButton;
using components::SplitView;
using components::Stat;
using components::StatusBar;
using components::StatusIndicator;
using components::StepIndicator;
using components::Stepper;
using components::StyledText;
using components::TabBar;
using components::Table;
using components::TagInput;
using components::TagList;
using components::TextInput;
using components::Timeline;
using components::Toggle;
using components::ToggleButton;
using components::ToggleGroup;
using components::Toolbar;
using components::WithProps;


/** Component-signature Button for callers that still need an explicit font. */
[[nodiscard]] inline ElementPtr Button(std::string label,
    std::shared_ptr<const graphics::Font> font, std::function<void()> onClick = {},
    std::string key = {}, const bool enabled = true) {
  return components::Button(std::move(label), std::move(font), std::move(onClick),
    std::move(key), enabled);
}


/** Component-signature FormField. */
[[nodiscard]] inline ElementPtr FormField(std::string label,
    std::shared_ptr<const graphics::Font> font, ElementPtr content, std::string help = {},
    std::string error = {}, std::string key = {}) {
  return components::FormField(std::move(label), std::move(font), std::move(content),
    std::move(help), std::move(error), std::move(key));
}

/** Component-signature SectionHeader. */
[[nodiscard]] inline ElementPtr SectionHeader(std::string title,
    std::shared_ptr<const graphics::Font> font, std::string subtitle = {}, ElementPtr action = {},
    std::string key = {}) {
  return components::SectionHeader(std::move(title), std::move(font), std::move(subtitle),
    std::move(action), std::move(key));
}

/** Raw layer host, kept as `dsl::Layer` for escape-hatch code. */
using components::Layer;
} // namespace coral::ui::dsl

#endif // CORAL_UI_DSL_H