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

#include <ui/DSL.h>

namespace coral::ui::dsl {
namespace {

thread_local const Theme* gTheme = nullptr;

const Theme& fallbackTheme() {
  static const Theme theme = Theme::dark();
  return theme;
}

} // namespace

ThemeScope::ThemeScope(const Theme& theme) : mPrevious(gTheme) {
  gTheme = &theme;
}

ThemeScope::ThemeScope(const Theme& theme, const Density density) : mPrevious(gTheme) {
  if (density == Density::Compact) {
    mOwned = theme;
    mOwned->spacing.xxs *= 0.75f;
    mOwned->spacing.xs *= 0.75f;
    mOwned->spacing.sm *= 0.75f;
    mOwned->spacing.md *= 0.75f;
    mOwned->spacing.lg *= 0.75f;
    mOwned->spacing.xl *= 0.75f;
    gTheme = &*mOwned;
    return;
  }
  gTheme = &theme;
}

ThemeScope::~ThemeScope() {
  gTheme = mPrevious;
}

const Theme& theme() {
  return gTheme != nullptr ? *gTheme : fallbackTheme();
}

} // namespace coral::ui::dsl
