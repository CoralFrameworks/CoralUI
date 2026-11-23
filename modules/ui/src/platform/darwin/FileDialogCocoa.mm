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

/**
 * @file FileDialogCocoa.mm
 *
 * The native macOS open/save panels. `showPlatformFileDialog` is synchronous by contract, so this
 * runs `NSSavePanel`/`NSOpenPanel` modally and returns the accepted URLs as paths. The panel's own
 * modal loop pumps AppKit events while it is open; the framework's frame loop resumes afterwards.
 */

#include "../../FileDialogPlatform.h"

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

namespace coral::ui::detail {

namespace {

std::string pathFromURL(NSURL* url) {
  if (url == nil) return {};
  const char* path = url.path.UTF8String;
  return path != nullptr ? std::string(path) : std::string();
}

/** Applies the framework's extension filters to a panel. */
void applyFilters(NSSavePanel* panel, const FileDialogOptions& options) {
  if (options.filters.empty()) return;
  NSMutableArray<NSString*>* extensions = [NSMutableArray array];
  for (const FileDialogFilter& filter: options.filters) {
    for (const std::string& extension: filter.extensions) {
      if (extension.empty()) continue;
      NSString* value = [NSString stringWithUTF8String:extension.c_str()];
      if (value != nil) [extensions addObject:value];
    }
  }
  if (extensions.count > 0) {
    // `allowedFileTypes` is deprecated in favour of UTType on newer SDKs, but it is still the one
    // API that turns a filter's extensions into an NSSavePanel restriction without a UTType mapping
    // table per extension.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = extensions;
#pragma clang diagnostic pop
  }
}

/** Splits a default path into its parent directory and the file name, which the panels expect. */
void applyDefaultPath(NSSavePanel* panel, const std::string& path) {
  if (path.empty()) return;
  NSString* full = [NSString stringWithUTF8String:path.c_str()];
  if (full == nil) return;
  BOOL isDirectory = NO;
  if ([[NSFileManager defaultManager] fileExistsAtPath:full isDirectory:&isDirectory] && isDirectory) {
    panel.directoryURL = [NSURL fileURLWithPath:full isDirectory:YES];
    return;
  }
  panel.directoryURL = [NSURL fileURLWithPath:[full stringByDeletingLastPathComponent]
                                  isDirectory:YES];
  panel.nameFieldStringValue = [full lastPathComponent];
}

} // namespace

FileDialogResult showPlatformFileDialog(const FileDialogKind kind,
    const FileDialogOptions& options) {
  FileDialogResult result;
  @autoreleasepool {
    if (![NSThread isMainThread]) {
      // AppKit panels are main-thread objects; refusing is better than a crash from a worker.
      return result;
    }

    NSSavePanel* panel = nil;
    NSOpenPanel* openPanel = nil;
    if (kind == FileDialogKind::OpenFile) {
      openPanel = [NSOpenPanel openPanel];
      openPanel.canChooseFiles = YES;
      openPanel.canChooseDirectories = NO;
      openPanel.allowsMultipleSelection = options.allowMultiple;
      panel = openPanel;
    } else {
      panel = [NSSavePanel savePanel];
      panel.canCreateDirectories = YES;
    }
    if (panel == nil) return result;

    if (!options.title.empty()) {
      panel.title = [NSString stringWithUTF8String:options.title.c_str()];
    }
    if (!options.defaultPath.empty()) applyDefaultPath(panel, options.defaultPath);
    applyFilters(panel, options);

    const NSModalResponse response = [panel runModal];
    if (response != NSModalResponseOK) return result;

    result.accepted = true;
    if (openPanel != nil) {
      for (NSURL* url in openPanel.URLs) {
        const std::string path = pathFromURL(url);
        if (!path.empty()) result.paths.push_back(path);
      }
    } else {
      const std::string path = pathFromURL(panel.URL);
      if (!path.empty()) result.paths.push_back(path);
    }
  }
  return result;
}

} // namespace coral::ui::detail
