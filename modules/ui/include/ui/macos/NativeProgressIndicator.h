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

#ifndef CORAL_UI_NATIVEPROGRESSINDICATOR_H
#define CORAL_UI_NATIVEPROGRESSINDICATOR_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <memory>

namespace coral::ui::macos {

enum class NativeProgressStyle {
  Bar,
  Spinning,
};

/** A native NSProgressIndicator hosted in the CoralUI layer tree; null on unsupported platforms. */
class NativeProgressIndicator final : public NativeViewLayer {
 public:
  static std::shared_ptr<NativeProgressIndicator> create(bool indeterminate = true,
    NativeProgressStyle style = NativeProgressStyle::Bar);
  ~NativeProgressIndicator() override;

  void setIndeterminate(bool indeterminate);
  [[nodiscard]] bool indeterminate() const;
  void setValue(double value);
  [[nodiscard]] double value() const;
  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;
  void setDisplayedWhenStopped(bool displayed);
  [[nodiscard]] bool displayedWhenStopped() const;
  void setStyle(NativeProgressStyle style);
  [[nodiscard]] NativeProgressStyle style() const;

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativeProgressIndicator(void* nativeView);
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVEPROGRESSINDICATOR_H
