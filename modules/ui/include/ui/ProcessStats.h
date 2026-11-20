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

#ifndef CORAL_UI_PROCESSSTATS_H
#define CORAL_UI_PROCESSSTATS_H

#include <cstdint>
#include <string>

namespace coral::ui {

/**
 * Cheap process-wide resource counters for a development overlay or a bug report.
 *
 * Reading them costs a handful of syscalls, so a diagnostics overlay samples them every so often
 * rather than every frame. A field is 0 when the platform cannot report it.
 */
struct ProcessStatistics {
  /** Resident set size, the number that matters for "why is the app using so much memory". */
  uint64_t workingSetBytes = 0;
  /** Committed private bytes, which grow with leaks even when pages are paged out. */
  uint64_t privateBytes = 0;
  /** Open kernel handles (Windows) / file descriptors (Linux). */
  uint32_t handles = 0;
};

/** @return the current process's counters; all zeros on a platform that cannot report them. */
[[nodiscard]] ProcessStatistics processStatistics();

/**
 * Formats the counters as one line, for the same reason `FrameStatsOverlay::format` exists: a product
 * can log the exact text the overlay shows.
 */
[[nodiscard]] std::string formatProcessStatistics(const ProcessStatistics& statistics);

} // namespace coral::ui

#endif // CORAL_UI_PROCESSSTATS_H
