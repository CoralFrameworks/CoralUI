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

#include <ui/SecurityScopedBookmark.h>

#import <Cocoa/Cocoa.h>

#include <string>

namespace coral::ui {

struct SecurityScopedAccess::Impl {
  NSURL* url = nil;
  bool stale = false;
  bool accessing = false;
};

namespace {

std::string errorMessage(NSError* error) {
  if (error == nil) return "bookmark operation failed";
  NSString* text = error.localizedDescription;
  return text != nil ? std::string(text.UTF8String) : std::string("bookmark operation failed");
}

} // namespace

std::optional<std::string> createSecurityScopedBookmark(const std::string_view path,
    std::string* error) {
  if (path.empty()) {
    if (error != nullptr) *error = "empty path";
    return std::nullopt;
  }
  @autoreleasepool {
    NSString* file = [NSString stringWithUTF8String:std::string(path).c_str()];
    if (file.length == 0) {
      if (error != nullptr) *error = "invalid UTF-8 path";
      return std::nullopt;
    }
    NSURL* url = [NSURL fileURLWithPath:file];
    NSError* nativeError = nil;
    NSData* data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                includingResourceValuesForKeys:nil relativeToURL:nil error:&nativeError];
    if (data == nil || data.length == 0) {
      if (error != nullptr) *error = errorMessage(nativeError);
      return std::nullopt;
    }
    return std::string(static_cast<const char*>(data.bytes), data.length);
  }
}

SecurityScopedAccess::SecurityScopedAccess() : mImpl(std::make_unique<Impl>()) {}
SecurityScopedAccess::~SecurityScopedAccess() {
  stopAccessing();
}
SecurityScopedAccess::SecurityScopedAccess(SecurityScopedAccess&&) noexcept = default;
SecurityScopedAccess& SecurityScopedAccess::operator=(SecurityScopedAccess&&) noexcept = default;

std::optional<SecurityScopedAccess> SecurityScopedAccess::fromBookmark(
    const std::string_view bookmark, std::string* error) {
  if (bookmark.empty()) {
    if (error != nullptr) *error = "empty bookmark";
    return std::nullopt;
  }
  @autoreleasepool {
    NSData* data = [NSData dataWithBytes:bookmark.data() length:bookmark.size()];
    NSError* nativeError = nil;
    BOOL stale = NO;
    NSURL* url = [NSURL URLByResolvingBookmarkData:data
                                          options:NSURLBookmarkResolutionWithSecurityScope
                                    relativeToURL:nil
                              bookmarkDataIsStale:&stale
                                            error:&nativeError];
    if (url == nil) {
      if (error != nullptr) *error = errorMessage(nativeError);
      return std::nullopt;
    }
    SecurityScopedAccess access;
    access.mImpl->url = url;
    access.mImpl->stale = stale != NO;
    return access;
  }
}

bool SecurityScopedAccess::valid() const noexcept {
  return mImpl != nullptr && mImpl->url != nil;
}

bool SecurityScopedAccess::stale() const noexcept {
  return mImpl != nullptr && mImpl->stale;
}

std::string SecurityScopedAccess::path() const {
  if (!valid()) return {};
  const char* value = mImpl->url.path.UTF8String;
  return value != nullptr ? std::string(value) : std::string();
}

bool SecurityScopedAccess::startAccessing() {
  if (!valid()) return false;
  if (mImpl->accessing) return true;
  mImpl->accessing = [mImpl->url startAccessingSecurityScopedResource] != NO;
  return mImpl->accessing;
}

void SecurityScopedAccess::stopAccessing() {
  if (mImpl == nullptr || !mImpl->accessing) return;
  [mImpl->url stopAccessingSecurityScopedResource];
  mImpl->accessing = false;
}

bool SecurityScopedAccess::isAccessing() const noexcept {
  return mImpl != nullptr && mImpl->accessing;
}

bool supportsSecurityScopedBookmarks() noexcept {
  return true;
}

} // namespace coral::ui
