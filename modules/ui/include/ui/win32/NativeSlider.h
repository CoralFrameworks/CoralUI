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

#ifndef CORAL_UI_WIN32_NATIVESLIDER_H
#define CORAL_UI_WIN32_NATIVESLIDER_H

#include <ui/NativeViewLayer.h>
#include <ui/win32/NativeControlSize.h>

#include <functional>
#include <memory>

namespace coral::ui::win32 {

namespace detail {
class NativeControlHost;
}

/** A native Win32 trackbar (`msctls_trackbar32`) hosted in the CoralUI layer tree. */
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
  [[nodiscard]] bool continuous() const noexcept {
    return mContinuous;
  }

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const noexcept {
    return mControlSize;
  }

  void onWindowChanged(Window* previous, Window* current) override;

 private:
  NativeSlider(void* nativeView, int controlId, double minimum, double maximum, double value,
    ChangeHandler onChange);

  std::shared_ptr<detail::NativeControlHost> mHost;
  ChangeHandler mOnChange;
  double mMinimum = 0.0;
  double mMaximum = 1.0;
  bool mContinuous = true;
  bool mEnabled = true;
  NativeControlSize mControlSize = NativeControlSize::Regular;
};

} // namespace coral::ui::win32

#endif // CORAL_UI_WIN32_NATIVESLIDER_H
