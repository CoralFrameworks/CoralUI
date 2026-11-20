/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file crash_windows_smoke.cpp
 *
 * The one thing the unit tests cannot prove about crash reporting: that the installed filter actually
 * writes a minidump when the process dies. The test spawns itself as a child that installs the
 * reporter and raises an access violation, waits for it to die, and checks the directory the
 * reporter was pointed at.
 */

#include <ui/CrashReporter.h>

#include <cstdio>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <windows.h>

namespace {

/** @return this executable's path, so the child is spawned even when argv[0] is a bare name. */
std::string executablePath() {
  wchar_t buffer[1024] = {};
  const DWORD length = ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
  if (length == 0) return {};
  const int utf8Size = ::WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
  if (utf8Size <= 1) return {};
  std::string path(static_cast<std::size_t>(utf8Size - 1), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, buffer, -1, path.data(), utf8Size - 1, nullptr, nullptr);
  return path;
}

} // namespace
#endif

int main(const int argc, char** argv) {
#if !defined(_WIN32)
  (void)argc;
  (void)argv;
  std::fprintf(stderr, "crash_windows_smoke: the Windows crash filter is not part of this build\n");
  return 77;
#else
  if (argc >= 3 && std::string(argv[1]) == "--crash") {
    coral::ui::CrashReporterOptions options;
    options.directory = argv[2];
    options.writeMiniDump = true;
    options.writeTextReport = true;
    if (!coral::ui::installCrashReporter(options)) {
      std::fprintf(stderr, "crash_windows_smoke: the reporter could not be installed\n");
      return 3;
    }
    // A real exception, not a null dereference: this reaches the unhandled-exception filter even in a
    // build whose optimizer removed a store through a known-null pointer.
    ::RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    std::fprintf(stderr, "crash_windows_smoke: the exception did not terminate the process\n");
    return 4;
  }
  if (argc < 2) {
    std::fprintf(stderr, "usage: crash_windows_smoke <report-directory>\n");
    return 2;
  }
  const std::string directory = argv[1];
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::u8path(directory), error);
  if (error) {
    std::fprintf(stderr, "crash_windows_smoke: %s\n", error.message().c_str());
    return 5;
  }

  // Spawn the child directly rather than through `system`: the child must die from the exception, and
  // `CreateProcessW` with an explicit application name cannot be confused by a quoted path.
  const std::string self = executablePath().empty() ? std::string(argv[0]) : executablePath();
  std::wstring application = std::filesystem::u8path(self).wstring();
  std::wstring commandLine = L"\"" + application + L"\" --crash \"" +
    std::filesystem::u8path(directory).wstring() + L"\"";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (::CreateProcessW(application.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
        nullptr, &startup, &process) == FALSE) {
    std::fprintf(stderr, "crash_windows_smoke: CreateProcess failed (%lu)\n", ::GetLastError());
    return 9;
  }
  ::WaitForSingleObject(process.hProcess, 30000);
  DWORD childExit = 0;
  ::GetExitCodeProcess(process.hProcess, &childExit);
  ::CloseHandle(process.hThread);
  ::CloseHandle(process.hProcess);
  if (childExit == 0 || childExit == STILL_ACTIVE) {
    std::fprintf(stderr, "crash_windows_smoke: the child survived the access violation\n");
    return 6;
  }

  const std::vector<coral::ui::CrashReportFile> reports = coral::ui::listCrashReports(directory);
  bool dump = false;
  bool text = false;
  for (const coral::ui::CrashReportFile& report: reports) {
    if (report.miniDump && report.bytes > 0) dump = true;
    if (!report.miniDump && report.bytes > 0) text = true;
  }
  std::error_code ignored;
  std::filesystem::remove_all(std::filesystem::u8path(directory), ignored);
  if (!dump) {
    std::fprintf(stderr, "crash_windows_smoke: no minidump was written (%zu reports)\n", reports.size());
    return 7;
  }
  if (!text) {
    std::fprintf(stderr, "crash_windows_smoke: no text report was written\n");
    return 8;
  }
  std::printf("crash_windows_smoke: %zu reports, minidump and text written\n", reports.size());
  return 0;
#endif
}
