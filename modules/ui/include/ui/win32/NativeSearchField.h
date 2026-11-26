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

#ifndef CORAL_UI_WIN32_NATIVESEARCHFIELD_H
#define CORAL_UI_WIN32_NATIVESEARCHFIELD_H

#include <ui/NativeViewLayer.h>
#include <ui/win32/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace coral::ui::win32 {

namespace detail {
class NativeControlHost;
}

/**
 * A native Win32 edit control with a cue banner, hosted in the CoralUI layer tree.
 *
 * Win32 has no search-field class; `EDIT` plus `EM_SETCUEBANNER` is the platform's native
 * search-style text input.
 */
class NativeSearchField final : public NativeViewLayer {
 public:
  using ChangeHandler = std::function<void(std::string_view)>;

  static std::shared_ptr<NativeSearchField> create(std::string placeholder = {},
    ChangeHandler onChange = {});
  ~NativeSearchField() override;

  void setText(std::string text);
  [[nodiscard]] std::string text() const;
  void setPlaceholder(std::string placeholder);

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const noexcept {
    return mControlSize;
  }

  void onWindowChanged(Window* previous, Window* current) override;

 private:
  NativeSearchField(void* nativeView, int controlId, std::string placeholder,
    ChangeHandler onChange);

  std::shared_ptr<detail::NativeControlHost> mHost;
  ChangeHandler mOnChange;
  std::string mPlaceholder;
  bool mSettingText = false;
  bool mEnabled = true;
  NativeControlSize mControlSize = NativeControlSize::Regular;
};

} // namespace coral::ui::win32

#endif // CORAL_UI_WIN32_NATIVESEARCHFIELD_H
