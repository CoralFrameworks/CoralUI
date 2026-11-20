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

#include <ui/DragDrop.h>

#include <utility>

namespace coral::ui {

DragPayload DragPayload::text(std::string text) {
  DragPayload payload;
  payload.mType = "text/plain";
  payload.mData = std::move(text);
  return payload;
}

DragPayload DragPayload::files(std::vector<std::string> paths) {
  DragPayload payload;
  payload.mType = "text/uri-list";
  payload.mPaths = std::move(paths);
  std::string joined;
  for (const std::string& path: payload.mPaths) {
    if (!joined.empty()) joined += "\r\n";
    joined += path;
  }
  payload.mData = std::move(joined);
  return payload;
}

DragPayload DragPayload::custom(std::string mime, std::string data) {
  DragPayload payload;
  payload.mType = std::move(mime);
  payload.mData = std::move(data);
  return payload;
}

void DragPayload::setSecondary(std::string mime, std::string data) {
  mSecondaryType = std::move(mime);
  mSecondaryData = std::move(data);
}

} // namespace coral::ui
