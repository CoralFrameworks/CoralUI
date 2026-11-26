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

#ifndef CORAL_UI_WIN32_NATIVEPROGRESSINDICATOR_H
#define CORAL_UI_WIN32_NATIVEPROGRESSINDICATOR_H

#include <ui/NativeViewLayer.h>
#include <ui/win32/NativeControlSize.h>

#include <memory>

namespace coral::ui::win32 {

namespace detail {
class NativeControlHost;
}

enum class NativeProgressStyle {
  Bar,
  Spinning,
};

/**
 * A native Win32 progress bar (`msctls_progress32`) hosted in the CoralUI layer tree.
 *
 * Win32 has no spinning indicator class; `NativeProgressStyle::Spinning` is accepted for API parity
 * and still renders the bar. `PBS_MARQUEE` is fixed when the HWND is created, so pass
 * `indeterminate=true` to `create()` for a marquee; calling `setIndeterminate(true)` later starts the
 * animation only on such a control.
 */
class NativeProgressIndicator final : public NativeViewLayer {
 public:
  static std::shared_ptr<NativeProgressIndicator> create(bool indeterminate = true,
    NativeProgressStyle style = NativeProgressStyle::Bar);
  ~NativeProgressIndicator() override;

  void setIndeterminate(bool indeterminate);
  [[nodiscard]] bool indeterminate() const noexcept {
    return mIndeterminate;
  }
  void setValue(double value);
  [[nodiscard]] double value() const noexcept {
    return mValue;
  }

  void setStyle(NativeProgressStyle style);
  [[nodiscard]] NativeProgressStyle style() const noexcept {
    return mStyle;
  }

  /** Stored for API parity; Win32 shows the bar whenever the layer is visible. */
  void setDisplayedWhenStopped(bool displayed);
  [[nodiscard]] bool displayedWhenStopped() const noexcept {
    return mDisplayedWhenStopped;
  }

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const noexcept {
    return mControlSize;
  }

  void onWindowChanged(Window* previous, Window* current) override;

 private:
  NativeProgressIndicator(void* nativeView, int controlId, bool indeterminate,
    NativeProgressStyle style);

  std::shared_ptr<detail::NativeControlHost> mHost;
  double mValue = 0.0;
  bool mIndeterminate = false;
  bool mDisplayedWhenStopped = true;
  bool mEnabled = true;
  NativeProgressStyle mStyle = NativeProgressStyle::Bar;
  NativeControlSize mControlSize = NativeControlSize::Regular;
};

} // namespace coral::ui::win32

#endif // CORAL_UI_WIN32_NATIVEPROGRESSINDICATOR_H
