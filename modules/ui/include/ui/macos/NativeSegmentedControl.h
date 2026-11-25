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

#ifndef CORAL_UI_NATIVESEGMENTEDCONTROL_H
#define CORAL_UI_NATIVESEGMENTEDCONTROL_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::macos {

/** A native NSSegmentedControl hosted in the CoralUI layer tree. */
class NativeSegmentedControl final : public NativeViewLayer {
 public:
  using SelectionHandler = std::function<void(int)>;

  static std::shared_ptr<NativeSegmentedControl> create(std::vector<std::string> labels,
    int selected = 0, SelectionHandler onSelect = {});
  ~NativeSegmentedControl() override;

  void setLabels(std::vector<std::string> labels);
  [[nodiscard]] int segmentCount() const;
  [[nodiscard]] int selectedIndex() const;
  void setSelectedIndex(int index);

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativeSegmentedControl(void* nativeView);
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVESEGMENTEDCONTROL_H
