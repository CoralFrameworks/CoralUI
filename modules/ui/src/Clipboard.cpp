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

#include <ui/Clipboard.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "ClipboardPlatform.h"

namespace coral::ui {

namespace {

std::mutex& systemMutex() {
  static std::mutex mutex;
  return mutex;
}

std::shared_ptr<Clipboard>& systemStorage() {
  static std::shared_ptr<Clipboard> clipboard;
  return clipboard;
}

std::shared_ptr<Clipboard> createDefaultClipboard() {
  if (auto platform = detail::createPlatformClipboard(); platform != nullptr) {
    return platform;
  }
  return std::make_shared<MemoryClipboard>();
}

/** @return the bytes stored for @p mime, or an empty string when the format is absent. */
std::string findFormat(const std::vector<std::pair<std::string, std::string>>& formats,
    const std::string& mime) {
  for (const auto& entry: formats) {
    if (entry.first == mime) return entry.second;
  }
  return {};
}

/** Adds or replaces one format; @p mime must not be empty. */
void storeFormat(std::vector<std::pair<std::string, std::string>>& formats,
    const std::string& mime, std::string data) {
  for (auto& entry: formats) {
    if (entry.first == mime) {
      entry.second = std::move(data);
      return;
    }
  }
  formats.emplace_back(mime, std::move(data));
}

std::vector<std::string> formatNames(
    const std::vector<std::pair<std::string, std::string>>& formats) {
  std::vector<std::string> result;
  result.reserve(formats.size());
  for (const auto& entry: formats) result.push_back(entry.first);
  return result;
}

} // namespace anonymous

std::string MemoryClipboard::get(const std::string& mime) const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return findFormat(mFormats, mime);
}

bool MemoryClipboard::set(const std::string& mime, std::string data) {
  if (mime.empty()) return false;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    storeFormat(mFormats, mime, std::move(data));
  }
  notifyChanged();
  return true;
}

std::vector<std::string> MemoryClipboard::formats() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return formatNames(mFormats);
}

std::string MemoryClipboard::getPrimary(const std::string& mime) const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return findFormat(mPrimaryFormats, mime);
}

bool MemoryClipboard::setPrimary(const std::string& mime, std::string data) {
  if (mime.empty()) return false;
  const std::lock_guard<std::mutex> lock(mMutex);
  storeFormat(mPrimaryFormats, mime, std::move(data));
  // PRIMARY is not the clipboard: no change notification, exactly like the platform backends.
  return true;
}

std::vector<std::string> MemoryClipboard::primaryFormats() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return formatNames(mPrimaryFormats);
}

std::string Clipboard::getPrimary(const std::string& mime) const {
  (void)mime;
  return {};
}

bool Clipboard::setPrimary(const std::string& mime, std::string data) {
  (void)mime;
  (void)data;
  return false;
}

std::vector<std::string> Clipboard::primaryFormats() const {
  return {};
}

void Clipboard::setChangeHandler(ChangeHandler handler) {
  const std::lock_guard<std::mutex> lock(mHandlerMutex);
  mChangeHandler = std::move(handler);
}

bool Clipboard::hasChangeHandler() const {
  const std::lock_guard<std::mutex> lock(mHandlerMutex);
  return static_cast<bool>(mChangeHandler);
}

void Clipboard::notifyChanged() const {
  ChangeHandler handler;
  {
    const std::lock_guard<std::mutex> lock(mHandlerMutex);
    handler = mChangeHandler;
  }
  if (handler) handler();
}

std::vector<std::string> Clipboard::getFiles() const {
  const std::string list = get("text/uri-list");
  std::vector<std::string> paths;
  size_t offset = 0;
  while (offset < list.size()) {
    size_t end = list.find('\n', offset);
    if (end == std::string::npos) end = list.size();
    std::string line = list.substr(offset, end - offset);
    offset = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line.front() == '#') continue;
    paths.push_back(std::move(line));
  }
  return paths;
}

bool Clipboard::setFiles(const std::vector<std::string>& paths) {
  std::string list;
  for (const std::string& path: paths) {
    if (path.empty()) continue;
    if (!list.empty()) list += "\r\n";
    list += path;
  }
  if (list.empty()) return false;
  return set("text/uri-list", std::move(list));
}

std::shared_ptr<Clipboard> Clipboard::system() {
  const std::lock_guard<std::mutex> lock(systemMutex());
  auto& clipboard = systemStorage();
  if (clipboard == nullptr) {
    clipboard = createDefaultClipboard();
  }
  return clipboard;
}

void Clipboard::notifySystemChanged() {
  const std::shared_ptr<Clipboard> clipboard = system();
  if (clipboard != nullptr) clipboard->notifyChanged();
}

void Clipboard::setSystem(std::shared_ptr<Clipboard> clipboard) {
  const std::lock_guard<std::mutex> lock(systemMutex());
  systemStorage() = std::move(clipboard);
}


uint64_t Clipboard::systemChangeCount() {
  // Each platform answers from its own backend (Win32's sequence number, the X11/Wayland change
  // counter, or 0 where no notification exists), so this file stays platform-neutral.
  return detail::platformClipboardChangeCount();
}
} // namespace coral::ui
