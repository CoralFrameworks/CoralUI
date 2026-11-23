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

#ifndef CORAL_UI_SECURITYSCOPEDBOOKMARK_H
#define CORAL_UI_SECURITYSCOPEDBOOKMARK_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace coral::ui {

/**
 * Creates serialized security-scoped bookmark data for @p path.
 *
 * This is what a sandboxed macOS app persists after an `NSOpenPanel`/`NSSavePanel` selection so it can
 * reopen the file on a later launch. When the app is not sandboxed AppKit may still resolve the
 * bookmark, but the security scope is meaningful only under the App Sandbox entitlement.
 *
 * @return the serialized bookmark, or null with @p error set.
 */
[[nodiscard]] std::optional<std::string> createSecurityScopedBookmark(std::string_view path,
  std::string* error = nullptr);

/**
 * A resolved bookmark and its scoped resource access.
 *
 * `startAccessing()` must succeed before opening the file; the destructor stops the access if it was
 * started. Moving is supported; copying is not.
 */
class SecurityScopedAccess {
 public:
  SecurityScopedAccess();
  ~SecurityScopedAccess();
  SecurityScopedAccess(SecurityScopedAccess&&) noexcept;
  SecurityScopedAccess& operator=(SecurityScopedAccess&&) noexcept;
  SecurityScopedAccess(const SecurityScopedAccess&) = delete;
  SecurityScopedAccess& operator=(const SecurityScopedAccess&) = delete;

  [[nodiscard]] static std::optional<SecurityScopedAccess> fromBookmark(
    std::string_view bookmark, std::string* error = nullptr);

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] bool stale() const noexcept;
  [[nodiscard]] std::string path() const;

  /** Starts the security scope; safe to call more than once. */
  bool startAccessing();
  void stopAccessing();
  [[nodiscard]] bool isAccessing() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};

[[nodiscard]] bool supportsSecurityScopedBookmarks() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_SECURITYSCOPEDBOOKMARK_H
