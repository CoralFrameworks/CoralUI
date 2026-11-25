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

#ifndef CORAL_UI_NATIVESLIDER_H
#define CORAL_UI_NATIVESLIDER_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <functional>
#include <memory>

namespace coral::ui::macos {

/** A native NSSlider hosted in the CoralUI layer tree; null on unsupported platforms. */
class NativeSlider final : public NativeViewLayer {
 public:
  using ChangeHandler = std::function<void(double)>;

  static std::shared_ptr<NativeSlider> create(double minimum, double maximum, double value,
    ChangeHandler onChange = {});
  ~NativeSlider() override;

  void setRange(double minimum, double maximum);
  void setValue(double value);
  [[nodiscard]] double value() const;
  void setContinuous(bool continuous);

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativeSlider(void* nativeView);
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVESLIDER_H
