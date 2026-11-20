/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_CONTENTFRAME_H
#define CORAL_UI_CONTENTFRAME_H

#include <cstdint>
#include <memory>

namespace coral::ui {

namespace detail {
struct ContentFrameAccess;
}

/**
 * One producer-owned frame, opaque to UI controls.
 *
 * The texture/view handle stays inside this value; a control only needs its size and validity.
 * Producers construct frames through the advanced `ui/ContentFrameBackend.h` bridge, which is the
 * only header that names the RHI texture type.
 */
class ContentFrame {
 public:
  ContentFrame();
  ~ContentFrame();
  ContentFrame(const ContentFrame&);
  ContentFrame& operator=(const ContentFrame&);
  ContentFrame(ContentFrame&&) noexcept;
  ContentFrame& operator=(ContentFrame&&) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] uint32_t width() const noexcept;
  [[nodiscard]] uint32_t height() const noexcept;

 private:
  friend class ContentHost;
  friend class RenderContext;
  friend struct detail::ContentFrameAccess;
  struct Impl;
  explicit ContentFrame(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> mImpl;
};

} // namespace coral::ui

#endif // CORAL_UI_CONTENTFRAME_H
