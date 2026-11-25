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

#ifndef CORAL_UI_ACCESSIBILITYPROXY_H
#define CORAL_UI_ACCESSIBILITYPROXY_H

#include <ui/Layer.h>

#include <functional>
#include <string>
#include <utility>

namespace coral::ui {

/**
 * A paintless, hit-testless child that gives a composite control a ListItem in the accessibility
 * tree. Selection stays in sync with the owning control.
 */
class AccessibilityProxy final : public Layer {
 public:
  explicit AccessibilityProxy(std::string label) : mLabel(std::move(label)) {
    setAccessibleRole(AccessibilityRole::ListItem);
    setAccessibleName(mLabel);
    setHitTestable(false);
  }

  void setSelected(const bool selected) {
    setAccessibleSelected(selected);
  }

  void setOnSelect(std::function<void()> callback) {
    mOnSelect = std::move(callback);
  }

  bool performAccessibilityAction(AccessibilityAction action, std::string_view) override {
    if (action != AccessibilityAction::Select && action != AccessibilityAction::Invoke) {
      return false;
    }
    if (mOnSelect) mOnSelect();
    return true;
  }

  void drawSelf(RenderContext&) override {}

 private:
  std::string mLabel;
  std::function<void()> mOnSelect;
};

} // namespace coral::ui

#endif // CORAL_UI_ACCESSIBILITYPROXY_H
