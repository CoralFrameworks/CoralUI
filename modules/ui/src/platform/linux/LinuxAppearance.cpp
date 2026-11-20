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

#include "LinuxAppearance.h"

#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace coral::ui::detail {

namespace {

/**
 * Runs a short-lived helper and returns its stdout, or an empty string.
 *
 * The portal and GSettings interfaces are both D-Bus APIs. Talking to them through the desktop's own
 * `gdbus`/`gsettings` client avoids adding a glib dependency to the GUI framework, at the cost of one
 * short-lived process per query. Everything is bounded: a desktop whose helper hangs must not hang
 * window creation.
 */
std::string runHelper(const std::vector<std::string>& args) {
  int fds[2] = { -1, -1 };
  if (pipe(fds) != 0) return {};
  const pid_t child = fork();
  if (child < 0) {
    close(fds[0]);
    close(fds[1]);
    return {};
  }
  if (child == 0) {
    dup2(fds[1], STDOUT_FILENO);
    const int null = open("/dev/null", O_WRONLY);
    if (null >= 0) dup2(null, STDERR_FILENO);
    close(fds[0]);
    close(fds[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg: args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(fds[1]);
  std::string output;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
  bool timedOut = false;
  char buffer[512];
  for (;;) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) {
      timedOut = true;
      break;
    }
    pollfd descriptor{ fds[0], POLLIN, 0 };
    const int ready = poll(&descriptor, 1, static_cast<int>(remaining));
    if (ready <= 0) break;
    const ssize_t count = read(fds[0], buffer, sizeof(buffer));
    if (count <= 0) break;
    output.append(buffer, static_cast<size_t>(count));
  }
  close(fds[0]);
  if (timedOut) {
    // A helper that stopped answering must not hold window creation forever.
    kill(child, SIGTERM);
  }
  int status = 0;
  waitpid(child, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? output : std::string{};
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n'");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n'");
  return value.substr(first, last - first + 1);
}

/** Reads `org.freedesktop.appearance <key>` from the desktop portal. */
std::string portalSetting(const char* key) {
  const std::string output = runHelper({ "gdbus", "call", "--session",
    "--dest", "org.freedesktop.portal.Desktop",
    "--object-path", "/org/freedesktop/portal/desktop",
    "--method", "org.freedesktop.portal.Settings.Read",
    "org.freedesktop.appearance", key });
  // The reply is a variant such as `(<<uint32 1>>,)`; the value is the last integer in the text.
  const size_t at = output.find("uint32");
  if (at == std::string::npos) return {};
  size_t index = at + std::strlen("uint32");
  while (index < output.size() && (output[index] == ' ' || output[index] == '<')) ++index;
  const size_t begin = index;
  while (index < output.size() && std::isdigit(static_cast<unsigned char>(output[index])) != 0) ++index;
  return output.substr(begin, index - begin);
}

std::string gsettings(const char* schema, const char* key) {
  return trim(runHelper({ "gsettings", "get", schema, key }));
}

} // namespace

SystemAppearance linuxSystemAppearance() {
  static std::mutex mutex;
  static SystemAppearance cached;
  static auto readAt = std::chrono::steady_clock::time_point{};
  std::lock_guard<std::mutex> lock(mutex);
  const auto now = std::chrono::steady_clock::now();
  if (cached.valid && now - readAt < std::chrono::seconds(2)) return cached;

  SystemAppearance appearance;
  // color-scheme: 0 = no preference, 1 = prefer dark, 2 = prefer light.
  const std::string scheme = portalSetting("color-scheme");
  const std::string gnomeScheme = gsettings("org.gnome.desktop.interface", "color-scheme");
  if (!scheme.empty() || !gnomeScheme.empty()) {
    appearance.valid = true;
    appearance.preferDark = scheme == "1" || gnomeScheme == "prefer-dark";
  }
  const std::string contrast = portalSetting("contrast");
  if (!contrast.empty()) {
    appearance.valid = true;
    appearance.highContrast = contrast == "1";
  }
  const std::string highContrast = gsettings("org.gnome.desktop.a11y.interface", "high-contrast");
  if (highContrast == "true" || highContrast == "1") {
    appearance.valid = true;
    appearance.highContrast = true;
  }
  const std::string animations = gsettings("org.gnome.desktop.interface", "enable-animations");
  if (animations == "false" || animations == "0") {
    appearance.valid = true;
    appearance.reduceMotion = true;
  }
  // The portal does not publish a contrasting palette, so high contrast turns on only when a theme
  // supplied those colours; `applySystemAppearance` ignores an unusable palette instead of guessing.
  cached = appearance;
  readAt = now;
  return cached;
}

bool linuxPrefersDark() {
  return linuxSystemAppearance().preferDark;
}

} // namespace coral::ui::detail
