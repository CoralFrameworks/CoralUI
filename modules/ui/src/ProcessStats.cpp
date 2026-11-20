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

#include <ui/ProcessStats.h>

#include <cstdio>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <dirent.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#endif

namespace coral::ui {

ProcessStatistics processStatistics() {
  ProcessStatistics stats;
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  counters.cb = sizeof(counters);
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &counters, sizeof(counters)) != FALSE) {
    stats.workingSetBytes = static_cast<uint64_t>(counters.WorkingSetSize);
    stats.privateBytes = static_cast<uint64_t>(counters.PagefileUsage);
  }
  DWORD handles = 0;
  if (::GetProcessHandleCount(::GetCurrentProcess(), &handles) != FALSE) {
    stats.handles = static_cast<uint32_t>(handles);
  }
#elif defined(__linux__)
  long pages = 0;
  long resident = 0;
  if (FILE* file = std::fopen("/proc/self/statm", "r")) {
    if (std::fscanf(file, "%ld %ld", &pages, &resident) == 2) {
      const long pageSize = ::sysconf(_SC_PAGESIZE);
      if (pageSize > 0 && resident > 0) {
        stats.workingSetBytes = static_cast<uint64_t>(resident) * static_cast<uint64_t>(pageSize);
      }
    }
    std::fclose(file);
  }
  // RssAnon is the anonymous resident part, the closest cheap equivalent of private committed bytes.
  if (FILE* status = std::fopen("/proc/self/status", "r")) {
    char line[256];
    while (std::fgets(line, sizeof(line), status) != nullptr) {
      unsigned long long kilobytes = 0;
      if (std::sscanf(line, "RssAnon: %llu kB", &kilobytes) == 1) {
        stats.privateBytes = static_cast<uint64_t>(kilobytes) * 1024u;
      }
    }
    std::fclose(status);
  }
  // Each entry in /proc/self/fd is one open descriptor.
  if (DIR* directory = ::opendir("/proc/self/fd")) {
    uint32_t count = 0;
    while (::readdir(directory) != nullptr) {
      ++count;
    }
    ::closedir(directory);
    // readdir also yields "." and "..", and opendir itself holds one descriptor.
    stats.handles = count >= 3 ? count - 3 : 0;
  }
#endif
  return stats;
}

std::string formatProcessStatistics(const ProcessStatistics& statistics) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "mem %.1f MiB  private %.1f MiB  handles %u",
    static_cast<double>(statistics.workingSetBytes) / (1024.0 * 1024.0),
    static_cast<double>(statistics.privateBytes) / (1024.0 * 1024.0), statistics.handles);
  return buffer;
}

} // namespace coral::ui
