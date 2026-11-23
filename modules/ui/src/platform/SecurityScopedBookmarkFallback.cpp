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

namespace coral::ui {

struct SecurityScopedAccess::Impl {};

std::optional<std::string> createSecurityScopedBookmark(std::string_view, std::string* error) {
  if (error != nullptr) *error = "security-scoped bookmarks are macOS-only";
  return std::nullopt;
}

SecurityScopedAccess::SecurityScopedAccess() = default;
SecurityScopedAccess::~SecurityScopedAccess() = default;
SecurityScopedAccess::SecurityScopedAccess(SecurityScopedAccess&&) noexcept = default;
SecurityScopedAccess& SecurityScopedAccess::operator=(SecurityScopedAccess&&) noexcept = default;

std::optional<SecurityScopedAccess> SecurityScopedAccess::fromBookmark(std::string_view,
    std::string* error) {
  if (error != nullptr) *error = "security-scoped bookmarks are macOS-only";
  return std::nullopt;
}

bool SecurityScopedAccess::valid() const noexcept { return false; }
bool SecurityScopedAccess::stale() const noexcept { return false; }
std::string SecurityScopedAccess::path() const { return {}; }
bool SecurityScopedAccess::startAccessing() { return false; }
void SecurityScopedAccess::stopAccessing() {}
bool SecurityScopedAccess::isAccessing() const noexcept { return false; }
bool supportsSecurityScopedBookmarks() noexcept { return false; }

} // namespace coral::ui
