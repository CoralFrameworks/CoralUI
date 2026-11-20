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

#include <ui/ClipboardMime.h>

namespace coral::ui {

std::string mimeForSelectionTarget(const std::string_view target) {
  if (target == "UTF8_STRING" || target == "TEXT" || target == "STRING") return "text/plain";
  if (target == "text/uri-list") return "text/uri-list";
  if (target == "text/html") return "text/html";
  if (target == "image/png") return "image/png";
  // Applications that do not know a named target convention simply use the MIME type as the target
  // atom, so a name that looks like a MIME type is its own mapping. This keeps a custom type
  // ("application/x-coral-clip") usable instead of silently dropping it.
  if (target.find('/') != std::string_view::npos) return std::string(target);
  return {};
}

std::string_view selectionTargetForMime(const std::string_view mime) {
  if (mime == "text/plain") return "UTF8_STRING";
  if (mime == "text/uri-list") return "text/uri-list";
  if (mime == "text/html") return "text/html";
  if (mime == "image/png") return "image/png";
  // Mirror of the identity rule above: a MIME type is a valid X11 target name by itself.
  if (mime.find('/') != std::string_view::npos) return mime;
  return {};
}


void ClipboardPayload::set(std::string mime, std::string data) {
  if (mime.empty()) return;
  mData[std::move(mime)] = std::move(data);
}

std::string ClipboardPayload::get(const std::string& mime) const {
  const auto found = mData.find(mime);
  return found == mData.end() ? std::string{} : found->second;
}

bool ClipboardPayload::hasFormat(const std::string& mime) const {
  return mData.find(mime) != mData.end();
}

std::vector<std::string> ClipboardPayload::formats() const {
  std::vector<std::string> result;
  result.reserve(mData.size());
  for (const auto& entry: mData) result.push_back(entry.first);
  return result;
}

bool ClipboardPayload::erase(const std::string& mime) {
  return mData.erase(mime) > 0;
}

void ClipboardPayload::clear() {
  mData.clear();
}
} // namespace coral::ui