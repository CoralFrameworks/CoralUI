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

#ifndef CORAL_UI_CRASHREPORTER_H
#define CORAL_UI_CRASHREPORTER_H

#include <cstdint>
#include <string>
#include <vector>

namespace coral::ui {

struct CrashReporterOptions {
  /** Directory for `crash-<pid>-<time>.dmp`/`.txt`; empty means the current working directory. */
  std::string directory;
  /** Write the binary minidump. Windows only; ignored elsewhere. */
  bool writeMiniDump = true;
  /** Write the text report (context, process counters). Available on every platform. */
  bool writeTextReport = true;
};

/**
 * Installs process-wide crash reporting.
 *
 * Windows installs an unhandled-exception filter that writes a minidump and a text report into
 * `options.directory` and then terminates the process through the filter's handler.
 *
 * POSIX installs handlers for `SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGILL` and `SIGBUS` that write a text
 * report with a backtrace (`backtrace_symbols_fd`) and then re-raise the signal, so the process still
 * dies from the fault it actually hit and an attached debugger still stops there. Only
 * async-signal-safe calls are used, and the context is copied into a fixed buffer when it is set, which
 * is why a context longer than 4 KiB is truncated in the report.
 *
 * Other platforms keep their own signal handling: the call returns false there, and the text-report
 * writer still works. Installing twice replaces the previous configuration.
 */
[[nodiscard]] bool installCrashReporter(const CrashReporterOptions& options);
/** Removes the handler; safe when none was installed. */
void uninstallCrashReporter();
[[nodiscard]] bool crashReporterInstalled();

/**
 * Text attached to the next crash report: window size, GPU backend, the last frame statistics, whatever
 * the product considers useful. The handler copies it before the process dies, so keeping it current
 * costs one string assignment.
 */
void setCrashContext(std::string context);
[[nodiscard]] std::string crashContext();

/** Writes a minidump of the current process; false where the platform has no minidump format. */
bool writeMiniDump(const std::string& path);

/** Writes the text half of a report (context, process counters); also exposed for tests. */
bool writeCrashTextReport(const std::string& path);

/** One file a crash left behind. */
struct CrashReportFile {
  std::string path;
  std::uint64_t bytes = 0;
  bool miniDump = false;
  /** The file's write time; newer reports sort first. */
  std::int64_t modified = 0;
};

/**
 * @return the crash reports in @p directory, newest first.
 *
 * A report is any `crash-*.dmp` or `crash-*.txt` in the directory. Listing rather than parsing is what
 * lets a product say "3 reports are waiting to be sent" without knowing the file format, and the
 * support bundle uses the same list.
 */
[[nodiscard]] std::vector<CrashReportFile> listCrashReports(const std::string& directory);

/**
 * Deletes every report listCrashReports would find.
 *
 * @return false when a file could not be removed; the reports that were removed stay removed.
 */
bool purgeCrashReports(const std::string& directory);

/**
 * What a support bundle needs from the application.
 *
 * The framework knows the format and the crash files; the application knows what it is and what
 * hardware it is on. `details` is where the app puts the latter (GPU and driver, the tail of its log,
 * the last frame statistics) as one line each.
 */
struct SupportBundle {
  /** The directory the crash reports are in. */
  std::string directory;
  /** The file to write, e.g. `coral-support.txt`. */
  std::string outputPath;
  std::string application;
  std::string version;
  std::string commit;
  std::vector<std::string> details;
};

/**
 * Writes a text bundle for a bug report.
 *
 * It carries the application and build identity, the operating system, the reports found in
 * `directory` with their sizes and ages, and the application's own detail lines. It deliberately does
 * not embed dump bytes: a support engineer asks for the dump separately, and a bundle that silently
 * carries a 100 MB attachment is not something a user should be surprised by.
 *
 * @return false when the bundle could not be written.
 */
bool writeSupportBundle(const SupportBundle& bundle);

} // namespace coral::ui

#endif // CORAL_UI_CRASHREPORTER_H
