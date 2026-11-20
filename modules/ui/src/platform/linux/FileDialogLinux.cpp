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
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace coral::ui::detail {

std::optional<FileDialogResult> showPortalFileDialog(FileDialogKind kind,
  const FileDialogOptions& options);

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

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find('\n', start);
    std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(std::move(line));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}

std::string buildFilter(const FileDialogFilter& filter, const bool zenity) {
  std::string label = filter.label.empty() ? "Files" : filter.label;
  std::string patterns;
  for (const std::string& extension: filter.extensions) {
    if (!patterns.empty()) patterns += " ";
    patterns += "*." + extension;
  }
  if (patterns.empty()) patterns = "*";
  return zenity ? label + " | " + patterns : label + " (" + patterns + ")";
}

FileDialogResult run(const std::vector<std::string>& args) {
  FileDialogResult result;
  int output[2] = { -1, -1 };
  if (pipe(output) != 0) return result;
  const pid_t child = fork();
  if (child < 0) {
    close(output[0]);
    close(output[1]);
    return result;
  }
  if (child == 0) {
    dup2(output[1], STDOUT_FILENO);
    const int null = open("/dev/null", O_WRONLY);
    if (null >= 0) dup2(null, STDERR_FILENO);
    close(output[0]);
    close(output[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& argument: args) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  close(output[1]);
  std::string stdoutText;
  char buffer[4096];
  for (;;) {
    const ssize_t count = read(output[0], buffer, sizeof(buffer));
    if (count <= 0) break;
    stdoutText.append(buffer, static_cast<size_t>(count));
  }
  close(output[0]);
  int status = 0;
  waitpid(child, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return result;
  result.paths = splitLines(stdoutText);
  result.accepted = !result.paths.empty();
  return result;
}

} // namespace

FileDialogResult showPlatformFileDialog(const FileDialogKind kind,
    const FileDialogOptions& options) {
  if (auto portal = showPortalFileDialog(kind, options); portal.has_value()) {
    return *portal;
  }
  const bool save = kind == FileDialogKind::SaveFile;
  const bool zenity = hasExecutable("zenity");
  if (!zenity && !hasExecutable("kdialog")) return {};

  std::vector<std::string> args;
  if (zenity) {
    args.push_back("zenity");
    args.push_back("--file-selection");
    if (save) args.push_back("--save");
    if (!save && options.allowMultiple) args.push_back("--multiple");
  } else {
    args.push_back("kdialog");
    args.push_back(save ? "--getsavefilename" : "--getopenfilename");
  }
  if (!options.title.empty()) {
    args.push_back(zenity ? "--title=" + options.title : "--title");
    if (!zenity) args.push_back(options.title);
  }
  if (!options.defaultPath.empty()) {
    args.push_back(zenity ? "--filename=" + options.defaultPath : options.defaultPath);
  }
  if (!options.parentWindow.empty() && options.parentWindow.rfind("x11:", 0) == 0) {
    const std::string id = options.parentWindow.substr(4);
    args.push_back(zenity ? "--attach=" + id : "--attach");
    if (!zenity) args.push_back(id);
  }
  if (!save && options.allowMultiple && !zenity) {
    args.push_back("--multiple");
    args.push_back("--separate-output");
  }
  for (const FileDialogFilter& filter: options.filters) {
    const std::string value = buildFilter(filter, zenity);
    args.push_back(zenity ? "--file-filter=" + value : value);
  }
  return run(args);
}

} // namespace coral::ui::detail
