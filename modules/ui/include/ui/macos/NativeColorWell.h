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

#ifndef CORAL_UI_NATIVECOLORWELL_H
#define CORAL_UI_NATIVECOLORWELL_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <geometry/Color.h>

#include <functional>
#include <memory>

namespace coral::ui::macos {

/** A native NSColorWell hosted in the CoralUI layer tree; null on unsupported platforms. */
class NativeColorWell final : public NativeViewLayer {
 public:
  using ChangeHandler = std::function<void(graphics::Color)>;

  static std::shared_ptr<NativeColorWell> create(graphics::Color initial = {},
    ChangeHandler onChange = {});
  ~NativeColorWell() override;

  void setColor(graphics::Color color);
  [[nodiscard]] graphics::Color color() const;
  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;
  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;

 private:
  NativeColorWell(void* nativeView);
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVECOLORWELL_H
