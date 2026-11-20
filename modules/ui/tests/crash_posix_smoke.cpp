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

// POSIX crash-handler smoke test.
//
// The handler has to run inside a dying process, so the test forks: the child installs the reporter,
// sets a context line and raises SIGSEGV; the parent waits for it and inspects the report the handler
// wrote. That is the only way to prove the handler actually fires and writes async-signal-safely --
// calling it in-process would take the test binary down with it.

#include <ui/CrashReporter.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int failures = 0;

void check(const bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

/** @return the first crash report in @p directory, or an empty string. */
std::string firstReport(const std::string& directory) {
  DIR* handle = ::opendir(directory.c_str());
  if (handle == nullptr) return {};
  std::string found;
  while (const dirent* entry = ::readdir(handle)) {
    const std::string name = entry->d_name;
    if (name.rfind("crash-", 0) == 0 && name.size() > 4 && name.substr(name.size() - 4) == ".txt") {
      found = directory + "/" + name;
      break;
    }
  }
  ::closedir(handle);
  return found;
}

} // namespace

int main() {
#if defined(__EMSCRIPTEN__)
  std::printf("SKIP: no process model to crash\n");
  return 77;
#else
  char directoryTemplate[] = "/tmp/coral-crash-smoke-XXXXXX";
  if (::mkdtemp(directoryTemplate) == nullptr) {
    std::printf("SKIP: mkdtemp failed\n");
    return 77;
  }
  const std::string directory = directoryTemplate;

  const pid_t child = ::fork();
  if (child < 0) {
    std::printf("SKIP: fork failed\n");
    return 77;
  }
  if (child == 0) {
    coral::ui::CrashReporterOptions options;
    options.directory = directory;
    options.writeMiniDump = false; // no minidump format on POSIX
    options.writeTextReport = true;
    if (!coral::ui::installCrashReporter(options)) {
      ::_exit(90);
    }
    coral::ui::setCrashContext("coral-crash-smoke context line");
    ::raise(SIGSEGV);
    ::_exit(91); // the handler re-raises, so this is unreachable
  }

  int status = 0;
  if (::waitpid(child, &status, 0) < 0) {
    std::printf("FAIL  waitpid\n");
    return 1;
  }
  check(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV,
    "the child dies from the signal it raised, not from a handler exit");
  check(WEXITSTATUS(status) != 90 || !WIFEXITED(status),
    "the reporter installed in the child");

  const std::string report = firstReport(directory);
  check(!report.empty(), "the handler wrote a report");
  std::string text;
  if (!report.empty()) {
    std::ifstream file(report);
    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  }
  check(text.find("coral ui crash report") != std::string::npos, "the report names the reporter");
  check(text.find("signal 11") != std::string::npos, "the report names the signal");
  check(text.find("coral-crash-smoke context line") != std::string::npos,
    "the report carries the context that was current at the crash");
  const size_t backtrace = text.find("backtrace\n");
  check(backtrace != std::string::npos, "the report has a backtrace section");
  if (backtrace != std::string::npos) {
    // At least the frame that called raise has to appear; the exact symbol names are toolchain
    // specific, so the assertion is that the section is not empty.
    check(text.size() > backtrace + std::strlen("backtrace\n") + 1,
      "the backtrace has at least one frame");
  }

  if (!report.empty()) {
    std::remove(report.c_str());
  }
  ::rmdir(directory.c_str());
  std::printf("%s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
  return failures == 0 ? 0 : 1;
#endif
}
