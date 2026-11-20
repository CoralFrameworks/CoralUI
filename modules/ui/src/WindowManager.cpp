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

#include <ui/WindowManager.h>

#include <ui/Window.h>

#include <algorithm>
#include <charconv>
#include <sstream>

namespace coral::ui {

namespace {

[[nodiscard]] bool sameWindow(const Window* left, const Window* right) {
  return left == right;
}

} // namespace

void WindowManager::add(std::shared_ptr<Window> window) {
  if (window == nullptr) return;
  if (std::find_if(mWindows.begin(), mWindows.end(), [&window](const auto& entry) {
        return entry.get() == window.get();
      }) != mWindows.end()) {
    return;
  }
  mWindows.push_back(std::move(window));
  if (mActive == nullptr) mActive = mWindows.back().get();
}

void WindowManager::remove(Window* window) {
  const auto position = std::find_if(mWindows.begin(), mWindows.end(),
    [window](const auto& entry) { return sameWindow(entry.get(), window); });
  if (position == mWindows.end()) return;
  if (mActive == (*position).get()) mActive = nullptr;
  mWindows.erase(position);
  if (mActive == nullptr && !mWindows.empty()) mActive = mWindows.front().get();
}

bool WindowManager::activate(Window* window) {
  if (window == nullptr) return false;
  const auto position = std::find_if(mWindows.begin(), mWindows.end(),
    [window](const auto& entry) { return sameWindow(entry.get(), window); });
  if (position == mWindows.end()) return false;
  if (isBlocked(*window)) return false;
  mActive = window;
  window->raise();
  return true;
}

bool WindowManager::isBlocked(const Window& window) const {
  for (const auto& candidate: mWindows) {
    if (candidate == nullptr || candidate.get() == &window) continue;
    if (candidate->isModal() && candidate->transientFor() == &window) return true;
  }
  return false;
}

std::vector<Rect> WindowManager::arrange(const WindowArrange mode, const Rect area,
    const float gap) {
  std::vector<Rect> rectangles;
  const auto count = static_cast<float>(mWindows.size());
  if (count <= 0.0f) return rectangles;
  rectangles.reserve(mWindows.size());

  switch (mode) {
    case WindowArrange::Cascade: {
      const float width = std::min(area.getWidth() * 0.7f, area.getWidth());
      const float height = std::min(area.getHeight() * 0.7f, area.getHeight());
      for (std::size_t index = 0; index < mWindows.size(); ++index) {
        const float offset = gap * static_cast<float>(index);
        const float x = std::min(area.getLeft() + offset, area.getRight() - width);
        const float y = std::min(area.getTop() + offset, area.getBottom() - height);
        rectangles.push_back(Rect::MakeXYWH(x, y, width, height));
      }
      break;
    }
    case WindowArrange::TileHorizontal: {
      const float width = (area.getWidth() - gap * (count - 1.0f)) / count;
      for (std::size_t index = 0; index < mWindows.size(); ++index) {
        rectangles.push_back(Rect::MakeXYWH(
          area.getLeft() + (width + gap) * static_cast<float>(index), area.getTop(), width,
          area.getHeight()));
      }
      break;
    }
    case WindowArrange::TileVertical: {
      const float height = (area.getHeight() - gap * (count - 1.0f)) / count;
      for (std::size_t index = 0; index < mWindows.size(); ++index) {
        rectangles.push_back(Rect::MakeXYWH(area.getLeft(),
          area.getTop() + (height + gap) * static_cast<float>(index), area.getWidth(), height));
      }
      break;
    }
  }

  for (std::size_t index = 0; index < mWindows.size() && index < rectangles.size(); ++index) {
    if (mWindows[index] != nullptr) mWindows[index]->restoreGeometry(rectangles[index]);
  }
  return rectangles;
}

void WindowManager::setGeometry(const std::string& id, const WindowGeometry& geometry) {
  if (id.empty()) return;
  mGeometry[id] = geometry;
}

std::optional<WindowGeometry> WindowManager::geometry(const std::string& id) const {
  const auto position = mGeometry.find(id);
  if (position == mGeometry.end()) return std::nullopt;
  return position->second;
}

std::string WindowManager::save() const {
  std::string output;
  for (const auto& entry: mGeometry) {
    output += entry.first + '|' + std::to_string(entry.second.x) + '|' +
      std::to_string(entry.second.y) + '|' + std::to_string(entry.second.width) + '|' +
      std::to_string(entry.second.height) + '|' + (entry.second.maximized ? "1" : "0") + '\n';
  }
  return output;
}

bool WindowManager::load(const std::string& data) {
  std::map<std::string, WindowGeometry> parsed;
  std::istringstream stream(data);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty()) continue;
    std::vector<std::string> fields;
    std::size_t offset = 0;
    while (offset <= line.size()) {
      const std::size_t separator = line.find('|', offset);
      if (separator == std::string::npos) {
        fields.push_back(line.substr(offset));
        break;
      }
      fields.push_back(line.substr(offset, separator - offset));
      offset = separator + 1;
    }
    if (fields.size() != 6 || fields[0].empty()) continue;
    int values[4] = { 0, 0, 0, 0 };
    bool valid = true;
    for (int index = 0; index < 4; ++index) {
      const std::string& field = fields[static_cast<std::size_t>(index) + 1];
      const auto result = std::from_chars(field.data(), field.data() + field.size(),
        values[index]);
      if (result.ec != std::errc{} || result.ptr != field.data() + field.size()) {
        valid = false;
        break;
      }
    }
    if (!valid || fields[5].size() != 1 || (fields[5][0] != '0' && fields[5][0] != '1')) {
      continue;
    }
    WindowGeometry geometry;
    geometry.x = values[0];
    geometry.y = values[1];
    geometry.width = values[2];
    geometry.height = values[3];
    geometry.maximized = fields[5][0] == '1';
    parsed[fields[0]] = geometry;
  }
  if (parsed.empty() && !data.empty()) return false;
  mGeometry = std::move(parsed);
  return true;
}

void WindowManager::clear() {
  mWindows.clear();
  mActive = nullptr;
  mGeometry.clear();
}


Window* WindowManager::windowAt(const Point screenPoint) const {
  // Later entries are on top (raises move to the back of the list is not modelled here), so walk
  // backwards to let the topmost window win.
  for (auto it = mWindows.rbegin(); it != mWindows.rend(); ++it) {
    const std::shared_ptr<Window>& window = *it;
    if (window == nullptr) continue;
    if (window->bounds().contains(screenPoint)) return window.get();
  }
  return nullptr;
}
bool WindowManager::beginDrag(std::shared_ptr<Window> source, DragPayload payload) {
  if (mDragPayload.has_value() || source == nullptr) return false;
  mDragPayload = std::move(payload);
  mDragSource = source.get();
  mDragTarget = nullptr;
  mDragEffect = DragEffect::None;
  return true;
}

bool WindowManager::dragTo(const Point screenPoint) {
  if (!mDragPayload.has_value()) return false;
  mDragPosition = screenPoint;
  Window* target = windowAt(screenPoint);
  if (target != mDragTarget && mDragTarget != nullptr) mDragTarget->dragLeave();
  mDragTarget = target;
  if (target == nullptr) {
    mDragEffect = DragEffect::None;
    return false;
  }
  const Rect bounds = target->bounds();
  const Point local{ screenPoint.x - bounds.getLeft(), screenPoint.y - bounds.getTop() };
  mDragEffect = target->dragHover(*mDragPayload, local);
  return mDragEffect != DragEffect::None;
}

bool WindowManager::drop() {
  if (!mDragPayload.has_value()) return false;
  bool accepted = false;
  if (mDragTarget != nullptr) {
    const Rect bounds = mDragTarget->bounds();
    const Point local{ mDragPosition.x - bounds.getLeft(), mDragPosition.y - bounds.getTop() };
    accepted = mDragTarget->dragDrop(*mDragPayload, local);
  }
  cancelDrag();
  return accepted;
}

void WindowManager::cancelDrag() {
  if (mDragTarget != nullptr) {
    mDragTarget->dragLeave();
    mDragTarget = nullptr;
  }
  mDragPayload.reset();
  mDragSource = nullptr;
  mDragEffect = DragEffect::None;
}
} // namespace coral::ui
