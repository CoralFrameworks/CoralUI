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

#ifndef CORAL_UI_CLIPBOARDMIME_H
#define CORAL_UI_CLIPBOARDMIME_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

/**
 * Maps an X11 selection target atom name to a clipboard MIME type, or "" when unsupported.
 *
 * The X11 clipboard speaks target names, not MIME types; keeping the translation in one tested place
 * stops the owner and the requester from drifting apart. A target that contains '/' is already a
 * MIME type (some applications use the MIME name as the target) and maps to itself, so a private
 * type such as "application/x-coral-clip" survives the round trip; legacy names without '/'
 * ("COMPOUND_TEXT", "SAVE_TARGETS") map to nothing.
 */
[[nodiscard]] std::string mimeForSelectionTarget(std::string_view target);

/**
 * The reverse mapping, used when advertising targets; "" means the MIME cannot be offered on X11.
 *
 * A MIME type without a dedicated X11 target name is offered under its own name.
 */
[[nodiscard]] std::string_view selectionTargetForMime(std::string_view mime);

/**
 * A multi-MIME clipboard payload: the shape X11/Wayland owners keep while they own the selection.
 *
 * `set` replaces the whole payload for that MIME; `formats` lists MIME types in a stable order so a
 * platform can advertise targets without sorting again.
 */
class ClipboardPayload {
 public:
  void set(std::string mime, std::string data);
  [[nodiscard]] std::string get(const std::string& mime) const;
  [[nodiscard]] bool hasFormat(const std::string& mime) const;
  [[nodiscard]] std::vector<std::string> formats() const;
  /** Removes one MIME type; returns whether it was present. */
  bool erase(const std::string& mime);
  void clear();

 private:
  std::map<std::string, std::string> mData;
};

} // namespace coral::ui

#endif // CORAL_UI_CLIPBOARDMIME_H
