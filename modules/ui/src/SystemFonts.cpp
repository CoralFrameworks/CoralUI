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

#include <ui/SystemFonts.h>

#include <graphics/Font.h>
#include <graphics/FontFallback.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui {

namespace {

/** Lowercase filename fragments, most useful faces first. */
constexpr const char* kFontKeywords[] = {
  "notocoloremoji", "noto color emoji", "apple color emoji", "seguiemj",
  "notosanscjk", "noto sans cjk", "notosanssc", "noto sans sc", "notosanstc",
  "msyh", "simsun", "simhei",
  "meiryo", "malgun", "pingfang", "hiragino sans gb",
  "arialuni", "arial unicode",
  "dejavusans", "liberationsans", "notosans", "segoeui",
};

constexpr size_t kMaxCandidates = 32;
constexpr size_t kMaxFallbackFaces = 12;

std::string lowerAscii(std::string value) {
  for (char& character: value) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
  }
  return value;
}

int priorityFor(const std::string& lowercaseName) {
  const int count = static_cast<int>(sizeof(kFontKeywords) / sizeof(kFontKeywords[0]));
  for (int index = 0; index < count; ++index) {
    if (lowercaseName.find(kFontKeywords[index]) != std::string::npos) return index;
  }
  return -1;
}

std::vector<std::string> platformFontDirectories() {
  std::vector<std::string> directories;
#if defined(_WIN32)
  if (const char* windows = std::getenv("WINDIR")) {
    directories.emplace_back(std::string(windows) + "\\Fonts");
  }
  if (const char* local = std::getenv("LOCALAPPDATA")) {
    directories.emplace_back(std::string(local) + "\\Microsoft\\Windows\\Fonts");
  }
#elif defined(__APPLE__)
  directories.emplace_back("/System/Library/Fonts");
  directories.emplace_back("/Library/Fonts");
  if (const char* home = std::getenv("HOME")) {
    directories.emplace_back(std::string(home) + "/Library/Fonts");
  }
#else
  directories.emplace_back("/usr/share/fonts");
  directories.emplace_back("/usr/local/share/fonts");
  if (const char* home = std::getenv("HOME")) {
    directories.emplace_back(std::string(home) + "/.local/share/fonts");
    directories.emplace_back(std::string(home) + "/.fonts");
  }
#endif
  return directories;
}

bool hasFontExtension(const std::string& lowercaseName) {
  static constexpr const char* kExtensions[] = { ".ttf", ".otf", ".ttc", ".otc" };
  for (const char* extension: kExtensions) {
    const size_t length = std::strlen(extension);
    if (lowercaseName.size() > length &&
        lowercaseName.compare(lowercaseName.size() - length, length, extension) == 0) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> discoverFontFiles() {
  struct Candidate {
    int priority = 0;
    std::string path;
  };
  std::vector<Candidate> candidates;
  std::error_code error;
  for (const std::string& directory: platformFontDirectories()) {
    if (candidates.size() >= kMaxCandidates) break;
    const std::filesystem::path root(directory);
    if (!std::filesystem::is_directory(root, error)) continue;
    std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    for (; !error && iterator != end; ++iterator) {
      std::error_code entryError;
      if (!iterator->is_regular_file(entryError)) continue;
      const std::string name = lowerAscii(iterator->path().filename().string());
      if (!hasFontExtension(name)) continue;
      const int priority = priorityFor(name);
      if (priority < 0) continue;
      candidates.push_back({ priority, iterator->path().string() });
      if (candidates.size() >= kMaxCandidates) break;
    }
    error.clear();
  }
  std::sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
    if (left.priority != right.priority) return left.priority < right.priority;
    return left.path < right.path;
  });
  std::vector<std::string> files;
  std::set<std::string> seen;
  for (const Candidate& candidate: candidates) {
    if (!seen.insert(candidate.path).second) continue;
    files.push_back(candidate.path);
  }
  return files;
}

struct DiscoveryState {
  std::mutex mutex;
  bool discovered = false;
  std::optional<std::vector<std::string>> overrideFiles;
  std::vector<std::string> files;
};

DiscoveryState& discoveryState() {
  static DiscoveryState state;
  return state;
}

std::vector<std::string> availableFontFiles() {
  DiscoveryState& state = discoveryState();
  std::lock_guard lock(state.mutex);
  if (state.overrideFiles.has_value()) return *state.overrideFiles;
  if (!state.discovered) {
    state.files = discoverFontFiles();
    state.discovered = true;
  }
  return state.files;
}

struct FallbackKey {
  const graphics::Typeface* typeface = nullptr;
  uint32_t sizeBits = 0;

  bool operator<(const FallbackKey& other) const noexcept {
    if (typeface != other.typeface) return typeface < other.typeface;
    return sizeBits < other.sizeBits;
  }
};

struct FallbackCache {
  std::mutex mutex;
  std::map<FallbackKey, std::weak_ptr<const graphics::FontFallback>> entries;
};

FallbackCache& fallbackCache() {
  static FallbackCache cache;
  return cache;
}

} // namespace

std::vector<std::string> systemFontFiles() {
  return availableFontFiles();
}

std::shared_ptr<const graphics::FontFallback> systemFontFallback(
    std::shared_ptr<const graphics::Font> primary) {
  if (primary == nullptr || primary->typeface() == nullptr) return nullptr;
  FallbackKey key;
  key.typeface = primary->typeface().get();
  const float size = primary->size();
  std::memcpy(&key.sizeBits, &size, sizeof(key.sizeBits));

  FallbackCache& cache = fallbackCache();
  {
    std::lock_guard lock(cache.mutex);
    const auto found = cache.entries.find(key);
    if (found != cache.entries.end()) {
      if (const auto existing = found->second.lock()) return existing;
      cache.entries.erase(found);
    }
  }

  const std::vector<std::string> files = availableFontFiles();
  if (files.empty()) return nullptr;

  auto fallback = std::make_shared<graphics::FontFallback>(primary);
  std::set<const graphics::Typeface*> loaded;
  loaded.insert(primary->typeface().get());
  size_t added = 0;
  for (const std::string& path: files) {
    if (added >= kMaxFallbackFaces) break;
    const auto typeface = graphics::Typeface::fromFile(path);
    if (typeface == nullptr) continue;
    if (!loaded.insert(typeface.get()).second) continue;
    const auto font = graphics::Font::create(typeface, size);
    if (font == nullptr) continue;
    fallback->addFallback(font);
    ++added;
  }
  if (added == 0) return nullptr;

  std::shared_ptr<const graphics::FontFallback> shared = std::move(fallback);
  {
    std::lock_guard lock(cache.mutex);
    cache.entries[key] = shared;
  }
  return shared;
}

void setSystemFontFilesForTesting(std::vector<std::string> files) {
  {
    DiscoveryState& state = discoveryState();
    std::lock_guard lock(state.mutex);
    if (files.empty()) {
      state.overrideFiles.reset();
      state.discovered = false;
      state.files.clear();
    } else {
      state.overrideFiles = std::move(files);
    }
  }
  FallbackCache& cache = fallbackCache();
  std::lock_guard lock(cache.mutex);
  cache.entries.clear();
}

} // namespace coral::ui
