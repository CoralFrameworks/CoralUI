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

#include <ui/ContentFrame.h>
#include <ui/ContentFrameBackend.h>

#include <utility>

namespace coral::ui {

struct ContentFrame::Impl {
  graphics::ExternalImage image;
  std::shared_ptr<void> owner;
};

ContentFrame::ContentFrame() = default;
ContentFrame::~ContentFrame() = default;
ContentFrame::ContentFrame(const ContentFrame&) = default;
ContentFrame& ContentFrame::operator=(const ContentFrame&) = default;
ContentFrame::ContentFrame(ContentFrame&&) noexcept = default;
ContentFrame& ContentFrame::operator=(ContentFrame&&) noexcept = default;
ContentFrame::ContentFrame(std::shared_ptr<Impl> impl) : mImpl(std::move(impl)) {}

bool ContentFrame::valid() const noexcept {
  return mImpl != nullptr && mImpl->image.isValid();
}

uint32_t ContentFrame::width() const noexcept {
  return mImpl != nullptr ? mImpl->image.width : 0;
}

uint32_t ContentFrame::height() const noexcept {
  return mImpl != nullptr ? mImpl->image.height : 0;
}

namespace detail {

struct ContentFrameAccess {
  static ContentFrame make(const graphics::ExternalImage& image, std::shared_ptr<void> owner) {
    auto impl = std::make_shared<ContentFrame::Impl>();
    impl->image = image;
    impl->owner = std::move(owner);
    return ContentFrame(std::move(impl));
  }
  static const graphics::ExternalImage& image(const ContentFrame& frame) {
    static const graphics::ExternalImage empty;
    return frame.mImpl != nullptr ? frame.mImpl->image : empty;
  }
};

ContentFrame makeContentFrame(const graphics::ExternalImage& image, std::shared_ptr<void> owner) {
  return ContentFrameAccess::make(image, std::move(owner));
}

const graphics::ExternalImage& imageOf(const ContentFrame& frame) {
  return ContentFrameAccess::image(frame);
}

} // namespace detail

} // namespace coral::ui
