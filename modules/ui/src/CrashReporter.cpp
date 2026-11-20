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

#include <ui/CrashReporter.h>

#include <ui/ProcessStats.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/utsname.h>
#endif

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#else
// A POSIX crash handler has to write with async-signal-safe calls only: no allocation, no stdio, no
// locks. The report is therefore assembled into fixed buffers with `write`, and the backtrace comes
// from `backtrace_symbols_fd`, which writes straight to the descriptor.
#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace coral::ui {

namespace {

std::mutex gMutex;
CrashReporterOptions gOptions;
std::string gContext;
bool gInstalled = false;

#if defined(_WIN32)
LPTOP_LEVEL_EXCEPTION_FILTER gPreviousFilter = nullptr;
#else
constexpr int kCrashSignals[] = { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS };
constexpr size_t kCrashSignalCount = sizeof(kCrashSignals) / sizeof(kCrashSignals[0]);
struct sigaction gPreviousActions[kCrashSignalCount]{};
bool gPosixInstalled = false;
/** The report directory, copied at install time: the handler cannot lock the option mutex. */
char gReportDirectory[1024] = {};
/** The context, in a fixed buffer for the same reason; a torn read is the worst case. */
char gContextBuffer[4096] = {};
size_t gContextLength = 0;

void writeAll(const int descriptor, const char* text, const size_t length) {
  size_t written = 0;
  while (written < length) {
    const ssize_t count = ::write(descriptor, text + written, length - written);
    if (count <= 0) return;
    written += static_cast<size_t>(count);
  }
}

void writeLiteral(const int descriptor, const char* text) {
  writeAll(descriptor, text, std::strlen(text));
}

/**
 * The POSIX crash handler: report, backtrace, then die from the original signal.
 *
 * Only async-signal-safe calls: fixed buffers, `write`, `open`, `backtrace_symbols_fd`. The previous
 * action is restored and the signal re-raised rather than `_exit`, so a debugger attached to the
 * process still stops at the fault and the parent sees the signal that actually happened.
 */
void handlePosixSignal(const int signal, siginfo_t* info, void* context) {
  (void)context;
  (void)info;
  char path[1200];
  const char* directory = gReportDirectory;
  const size_t directoryLength = std::strlen(directory);
  const bool needsSeparator = directoryLength > 0 && directory[directoryLength - 1] != '/';
  const long pid = static_cast<long>(::getpid());
  const long seconds = static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());
  std::snprintf(path, sizeof(path), "%s%scrash-%ld-%ld.txt", directory, needsSeparator ? "/" : "",
    pid, seconds);

  const int descriptor = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (descriptor < 0) {
    // Nothing can be written; still die from the original signal.
    ::signal(signal, SIG_DFL);
    ::raise(signal);
    return;
  }

  writeLiteral(descriptor, "coral ui crash report\n");
  char line[64];
  std::snprintf(line, sizeof(line), "pid %ld\n", pid);
  writeLiteral(descriptor, line);
  std::snprintf(line, sizeof(line), "signal %d\n", signal);
  writeLiteral(descriptor, line);
  if (gContextLength > 0) {
    writeLiteral(descriptor, "context\n");
    writeAll(descriptor, gContextBuffer, gContextLength);
    writeLiteral(descriptor, "\n");
  }
  writeLiteral(descriptor, "backtrace\n");
  void* frames[64];
  const int frameCount = ::backtrace(frames, 64);
  if (frameCount > 0) {
    // Writes the symbols straight to the descriptor, one per line.
    ::backtrace_symbols_fd(frames, frameCount, descriptor);
  }
  ::close(descriptor);

  ::signal(signal, SIG_DFL);
  ::raise(signal);
}
#endif

std::string timestampSuffix() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
  return std::to_string(static_cast<long long>(seconds));
}

#if defined(_WIN32)
std::string reportPath(const std::string& extension) {
  std::string directory;
  {
    const std::lock_guard<std::mutex> lock(gMutex);
    directory = gOptions.directory;
  }
  if (!directory.empty() && directory.back() != '/' && directory.back() != '\\') {
    directory += '/';
  }
  const unsigned long pid =
#if defined(_WIN32)
    ::GetCurrentProcessId();
#else
    static_cast<unsigned long>(::getpid());
#endif
  return directory + "crash-" + std::to_string(pid) + "-" + timestampSuffix() + extension;
}
#endif

/** Opens @p path for writing with the platform's wide-character API on Windows. */
std::FILE* openReport(const std::string& path) {
#if defined(_WIN32)
  return _wfopen(std::filesystem::u8path(path).wstring().c_str(), L"wb");
#else
  return std::fopen(path.c_str(), "wb");
#endif
}

#if defined(_WIN32)
LONG WINAPI handleUnhandledException(EXCEPTION_POINTERS* info) {
  CrashReporterOptions options;
  std::string context;
  {
    const std::lock_guard<std::mutex> lock(gMutex);
    options = gOptions;
    context = gContext;
  }
  if (options.writeMiniDump) {
    const std::string path = reportPath(".dmp");
    const HANDLE file = ::CreateFileW(std::filesystem::u8path(path).wstring().c_str(),
      GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
      exceptionInfo.ThreadId = ::GetCurrentThreadId();
      exceptionInfo.ExceptionPointers = info;
      exceptionInfo.ClientPointers = FALSE;
      ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, MiniDumpNormal,
        info != nullptr ? &exceptionInfo : nullptr, nullptr, nullptr);
      ::CloseHandle(file);
    }
  }
  if (options.writeTextReport) {
    writeCrashTextReport(reportPath(".txt"));
  }
  // Terminate through the filter rather than returning to a state whose stack is already gone.
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

bool installCrashReporter(const CrashReporterOptions& options) {
  {
    const std::lock_guard<std::mutex> lock(gMutex);
    gOptions = options;
    gInstalled = true;
  }
#if defined(_WIN32)
  // Keep whatever was there before (a debugger, the runtime's abort filter) so it still runs.
  gPreviousFilter = ::SetUnhandledExceptionFilter(&handleUnhandledException);
  return true;
#else
  // The report directory and the context are copied here because the handler cannot take the mutex
  // the option accessors use.
  {
    const std::lock_guard<std::mutex> lock(gMutex);
    std::snprintf(gReportDirectory, sizeof(gReportDirectory), "%s", gOptions.directory.c_str());
    gContextLength = std::min(gContext.size(), sizeof(gContextBuffer) - 1);
    std::memcpy(gContextBuffer, gContext.data(), gContextLength);
    gContextBuffer[gContextLength] = '\0';
  }
  if (gPosixInstalled) return true;

  struct sigaction action {};
  action.sa_sigaction = &handlePosixSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO | SA_RESETHAND;
  bool installed = true;
  for (size_t index = 0; index < kCrashSignalCount; ++index) {
    if (::sigaction(kCrashSignals[index], &action, &gPreviousActions[index]) != 0) {
      installed = false;
    }
  }
  gPosixInstalled = installed;
  return installed;
#endif
}

void uninstallCrashReporter() {
#if defined(_WIN32)
  ::SetUnhandledExceptionFilter(gPreviousFilter);
  gPreviousFilter = nullptr;
#else
  if (gPosixInstalled) {
    for (size_t index = 0; index < kCrashSignalCount; ++index) {
      ::sigaction(kCrashSignals[index], &gPreviousActions[index], nullptr);
    }
    gPosixInstalled = false;
  }
#endif
  const std::lock_guard<std::mutex> lock(gMutex);
  gInstalled = false;
}

bool crashReporterInstalled() {
  const std::lock_guard<std::mutex> lock(gMutex);
  return gInstalled;
}

void setCrashContext(std::string context) {
  const std::lock_guard<std::mutex> lock(gMutex);
  gContext = std::move(context);
#if !defined(_WIN32)
  // Keep the signal-handler copy current: the handler cannot read the std::string safely.
  gContextLength = std::min(gContext.size(), sizeof(gContextBuffer) - 1);
  std::memcpy(gContextBuffer, gContext.data(), gContextLength);
  gContextBuffer[gContextLength] = '\0';
#endif
}

std::string crashContext() {
  const std::lock_guard<std::mutex> lock(gMutex);
  return gContext;
}

bool writeMiniDump(const std::string& path) {
#if defined(_WIN32)
  if (path.empty()) return false;
  const HANDLE file = ::CreateFileW(std::filesystem::u8path(path).wstring().c_str(),
    GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  const BOOL written = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file,
    MiniDumpNormal, nullptr, nullptr, nullptr);
  ::CloseHandle(file);
  return written != FALSE;
#else
  (void)path;
  return false;
#endif
}

bool writeCrashTextReport(const std::string& path) {
  if (path.empty()) return false;
  const ProcessStatistics process = processStatistics();
  const std::string context = crashContext();
  FILE* file = openReport(path);
  if (file == nullptr) return false;
  std::fprintf(file, "coral ui crash report\n");
  const unsigned long pid =
#if defined(_WIN32)
    ::GetCurrentProcessId();
#else
    static_cast<unsigned long>(::getpid());
#endif
  std::fprintf(file, "pid %lu\n", pid);
  std::fprintf(file, "%s\n", formatProcessStatistics(process).c_str());
  if (!context.empty()) {
    std::fprintf(file, "context\n%s\n", context.c_str());
  }
  std::fclose(file);
  return true;
}

namespace {

/** @return the operating system and its version, in the form a support engineer asks for. */
[[nodiscard]] std::string systemDescription() {
#if defined(_WIN32)
  const auto readValue = [](const wchar_t* name) -> std::string {
    wchar_t buffer[256] = {};
    DWORD size = sizeof(buffer);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", name,
          RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) {
      return {};
    }
    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 1) return {};
    std::string utf8(static_cast<std::size_t>(utf8Size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8.data(), utf8Size - 1, nullptr, nullptr);
    return utf8;
  };
  const std::string product = readValue(L"ProductName");
  const std::string display = readValue(L"DisplayVersion");
  const std::string build = readValue(L"CurrentBuild");
  std::string description = product.empty() ? "Windows" : product;
  if (!display.empty()) description += " " + display;
  if (!build.empty()) description += " (build " + build + ")";
  return description;
#else
  struct utsname info{};
  if (::uname(&info) == 0) return std::string(info.sysname) + " " + info.release;
  return "unknown";
#endif
}

/** @return the local time as `YYYY-MM-DD HH:MM:SS`, or an empty string when it cannot be formatted. */
[[nodiscard]] std::string timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  if (localtime_s(&local, &now) != 0) return {};
#else
  if (localtime_r(&now, &local) == nullptr) return {};
#endif
  char buffer[32] = {};
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local) == 0) return {};
  return buffer;
}

/** @return true when @p path names a file a crash reporter would have written. */
[[nodiscard]] bool isCrashReport(const std::filesystem::path& path) {
  const std::string name = path.filename().string();
  if (name.rfind("crash-", 0) != 0) return false;
  const std::string extension = path.extension().string();
  return extension == ".dmp" || extension == ".txt";
}

} // namespace

std::vector<CrashReportFile> listCrashReports(const std::string& directory) {
  std::vector<CrashReportFile> reports;
  if (directory.empty()) return reports;
  std::error_code error;
  const std::filesystem::directory_iterator entries(std::filesystem::u8path(directory), error);
  if (error) return reports;
  const auto fileTimeToSeconds = [](const std::filesystem::file_time_type time) {
    const auto system = std::chrono::time_point_cast<std::chrono::seconds>(
      std::chrono::system_clock::now() + (time - std::filesystem::file_time_type::clock::now()));
    return static_cast<std::int64_t>(system.time_since_epoch().count());
  };
  for (const std::filesystem::directory_entry& entry: entries) {
    std::error_code entryError;
    if (!entry.is_regular_file(entryError) || entryError) continue;
    const std::filesystem::path& path = entry.path();
    if (!isCrashReport(path)) continue;
    CrashReportFile report;
    report.path = path.string();
    report.bytes = static_cast<std::uint64_t>(entry.file_size(entryError));
    if (entryError) report.bytes = 0;
    report.miniDump = path.extension() == ".dmp";
    std::error_code timeError;
    report.modified = fileTimeToSeconds(entry.last_write_time(timeError));
    if (timeError) report.modified = 0;
    reports.push_back(std::move(report));
  }
  std::sort(reports.begin(), reports.end(), [](const CrashReportFile& a, const CrashReportFile& b) {
    if (a.modified != b.modified) return a.modified > b.modified;
    return a.path < b.path;
  });
  return reports;
}

bool purgeCrashReports(const std::string& directory) {
  bool removedAll = true;
  for (const CrashReportFile& report: listCrashReports(directory)) {
    std::error_code error;
    if (!std::filesystem::remove(std::filesystem::u8path(report.path), error) || error) {
      removedAll = false;
    }
  }
  return removedAll;
}

bool writeSupportBundle(const SupportBundle& bundle) {
  if (bundle.outputPath.empty()) return false;
  FILE* file = openReport(bundle.outputPath);
  if (file == nullptr) return false;
  std::fprintf(file, "coral ui support bundle\n");
  std::fprintf(file, "generated %s\n", timestamp().c_str());
  std::fprintf(file, "application %s\n", bundle.application.c_str());
  std::fprintf(file, "version %s\n", bundle.version.c_str());
  std::fprintf(file, "commit %s\n", bundle.commit.c_str());
  std::fprintf(file, "system %s\n", systemDescription().c_str());
  const std::vector<CrashReportFile> reports = listCrashReports(bundle.directory);
  std::fprintf(file, "crash reports %zu\n", reports.size());
  for (const CrashReportFile& report: reports) {
    std::fprintf(file, "  %s %llu bytes %s\n", report.path.c_str(),
      static_cast<unsigned long long>(report.bytes), report.miniDump ? "minidump" : "text");
  }
  if (!bundle.details.empty()) {
    std::fprintf(file, "details\n");
    for (const std::string& line: bundle.details) {
      std::fprintf(file, "  %s\n", line.c_str());
    }
  }
  std::fclose(file);
  return true;
}

} // namespace coral::ui
