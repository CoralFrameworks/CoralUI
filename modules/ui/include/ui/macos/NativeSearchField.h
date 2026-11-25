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

#ifndef CORAL_UI_NATIVESEARCHFIELD_H
#define CORAL_UI_NATIVESEARCHFIELD_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui::macos {

/** A native NSSearchField hosted in the CoralUI layer tree; null on platforms without one. */
class NativeSearchField final : public NativeViewLayer {
 public:
  using ChangeHandler = std::function<void(std::string_view)>;

  static std::shared_ptr<NativeSearchField> create(std::string placeholder = {},
    ChangeHandler onChange = {});
  ~NativeSearchField() override;

  void setText(std::string text);
  [[nodiscard]] std::string text() const;
  void setPlaceholder(std::string placeholder);

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativeSearchField(void* nativeView, std::string placeholder);
  std::string mPlaceholder;
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVESEARCHFIELD_H
