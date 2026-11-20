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

#include "../../FileDialogPlatform.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <thread>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace coral::ui::detail {

namespace {

bool hasExecutable(const char* name) {
  const char* path = std::getenv("PATH");
  if (path == nullptr) return false;
  const std::string paths(path);
  size_t start = 0;
  while (start <= paths.size()) {
    const size_t end = paths.find(':', start);
    const std::string directory = paths.substr(start, end == std::string::npos
      ? std::string::npos : end - start);
    const std::string candidate = (directory.empty() ? "." : directory) + "/" + name;
    if (access(candidate.c_str(), X_OK) == 0) return true;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return false;
}

struct Process {
  pid_t pid = -1;
  int output = -1;
};

Process spawn(const std::vector<std::string>& args) {
  Process process;
  int pipeFds[2] = { -1, -1 };
  if (pipe(pipeFds) != 0) return process;
  const pid_t child = fork();
  if (child < 0) {
    close(pipeFds[0]);
    close(pipeFds[1]);
    return process;
  }
  if (child == 0) {
    dup2(pipeFds[1], STDOUT_FILENO);
    const int null = open("/dev/null", O_WRONLY);
    if (null >= 0) dup2(null, STDERR_FILENO);
    close(pipeFds[0]);
    close(pipeFds[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg: args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(pipeFds[1]);
  process.pid = child;
  process.output = pipeFds[0];
  return process;
}

std::string readAll(const int fd, const int timeoutMs = 10000) {
  std::string result;
  char buffer[4096];
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  for (;;) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) break;
    pollfd descriptor{ fd, POLLIN, 0 };
    const int ready = poll(&descriptor, 1, static_cast<int>(remaining));
    if (ready <= 0) break;
    const ssize_t count = read(fd, buffer, sizeof(buffer));
    if (count <= 0) break;
    result.append(buffer, static_cast<size_t>(count));
  }
  return result;
}

int waitFor(const Process& process, const int timeoutMs = 5000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  int status = 0;
  for (;;) {
    const pid_t done = waitpid(process.pid, &status, WNOHANG);
    if (done == process.pid) return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (done < 0) return -1;
    if (std::chrono::steady_clock::now() >= deadline) {
      // A portal/helper that stopped answering must not hold the UI thread forever.
      kill(process.pid, SIGTERM);
      waitpid(process.pid, &status, 0);
      return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

std::string quote(const std::string& value) {
  std::string result = "'";
  for (const char character: value) {
    if (character == '\'' || character == '\\') result += '\\';
    result += character;
  }
  result += "'";
  return result;
}

std::string uriToPath(std::string uri) {
  if (uri.rfind("file://", 0) != 0) return {};
  uri.erase(0, 7);
  if (!uri.empty() && uri.front() != '/') {
    const size_t slash = uri.find('/');
    if (slash == std::string::npos) return {};
    uri.erase(0, slash);
  }
  std::string result;
  for (size_t index = 0; index < uri.size(); ++index) {
    if (uri[index] == '%' && index + 2 < uri.size()) {
      const auto hex = [](const char c) {
        return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10
          : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1));
      };
      const int high = hex(uri[index + 1]);
      const int low = hex(uri[index + 2]);
      if (high >= 0 && low >= 0) {
        result.push_back(static_cast<char>((high << 4) | low));
        index += 2;
        continue;
      }
    }
    result.push_back(uri[index]);
  }
  return result;
}

std::vector<std::string> urisIn(const std::string& text) {
  std::vector<std::string> paths;
  size_t offset = 0;
  while ((offset = text.find("file://", offset)) != std::string::npos) {
    const size_t end = text.find_first_of("'\"", offset);
    if (end == std::string::npos) break;
    const std::string path = uriToPath(text.substr(offset, end - offset));
    if (!path.empty()) paths.push_back(path);
    offset = end + 1;
  }
  return paths;
}

} // namespace

/** Portal-backed chooser; nullopt means "portal unavailable, try another backend". */
std::optional<FileDialogResult> showPortalFileDialog(FileDialogKind kind,
    const FileDialogOptions& options) {
  if (!hasExecutable("gdbus")) return std::nullopt;
  std::vector<std::string> monitorArgs = {
    "gdbus", "monitor", "--session", "--dest", "org.freedesktop.portal.Desktop",
  };
  const Process monitor = spawn(monitorArgs);
  if (monitor.pid < 0) return std::nullopt;

  std::vector<std::string> callArgs = {
    "gdbus", "call", "--session", "--dest", "org.freedesktop.portal.Desktop",
    "--object-path", "/org/freedesktop/portal/desktop", "--method",
    kind == FileDialogKind::SaveFile ? "org.freedesktop.portal.FileChooser.SaveFile"
                                     : "org.freedesktop.portal.FileChooser.OpenFile",
    options.parentWindow.empty() ? quote("") : quote(options.parentWindow),
    quote(options.title.empty() ? "Choose a file" : options.title),
  };
  std::string portalOptions = "{'multiple': <";
  portalOptions += options.allowMultiple && kind == FileDialogKind::OpenFile ? "true" : "false";
  portalOptions += ">";
  if (!options.filters.empty()) {
    portalOptions += ", 'filters': <[";
    for (size_t index = 0; index < options.filters.size(); ++index) {
      if (index > 0) portalOptions += ", ";
      portalOptions += "(";
      portalOptions += quote(options.filters[index].label);
      portalOptions += ", [";
      for (size_t ext = 0; ext < options.filters[index].extensions.size(); ++ext) {
        if (ext > 0) portalOptions += ", ";
        portalOptions += "(0, ";
        portalOptions += quote("*." + options.filters[index].extensions[ext]);
        portalOptions += ")";
      }
      portalOptions += "])";
    }
    portalOptions += "]>";
  }
  portalOptions += "}";
  callArgs.push_back(portalOptions);

  const Process call = spawn(callArgs);
  if (call.pid < 0) {
    kill(monitor.pid, SIGTERM);
    close(monitor.output);
    return std::nullopt;
  }
  const std::string callOutput = readAll(call.output);
  close(call.output);
  waitFor(call);

  const size_t handleStart = callOutput.find("objectpath '");
  if (handleStart == std::string::npos) {
    kill(monitor.pid, SIGTERM);
    close(monitor.output);
    return std::nullopt;
  }
  const size_t handleBegin = handleStart + 12;
  const size_t handleEnd = callOutput.find('\'', handleBegin);
  if (handleEnd == std::string::npos) {
    kill(monitor.pid, SIGTERM);
    close(monitor.output);
    return std::nullopt;
  }
  const std::string request = callOutput.substr(handleBegin, handleEnd - handleBegin);

  FileDialogResult result;
  std::string monitorOutput;
  bool matched = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  char buffer[4096];
  while (std::chrono::steady_clock::now() < deadline) {
    pollfd descriptor{ monitor.output, POLLIN, 0 };
    const int ready = poll(&descriptor, 1, 100);
    if (ready > 0 && (descriptor.revents & POLLIN) != 0) {
      const ssize_t count = read(monitor.output, buffer, sizeof(buffer));
      if (count > 0) {
        monitorOutput.append(buffer, static_cast<size_t>(count));
        const size_t response = monitorOutput.find(request + ":");
        if (response != std::string::npos &&
            monitorOutput.find("Response", response) != std::string::npos) {
          matched = true;
          break;
        }
      }
    }
  }
  close(monitor.output);
  kill(monitor.pid, SIGTERM);
  waitFor(monitor);
  if (!matched) return std::nullopt;
  const size_t response = monitorOutput.find(request + ":");
  result.paths = urisIn(monitorOutput.substr(response));
  result.accepted = !result.paths.empty();
  return result;
}

} // namespace coral::ui::detail
