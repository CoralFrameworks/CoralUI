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
 * @file NativeAlertCocoa.mm
 *
 * `NSAlert` in a window sheet. The public API is synchronous, so the sheet path waits on the main
 * run loop for the completion handler, exactly like the file panels; an inactive app falls back to
 * the app-modal `runModal`.
 */

#include <ui/NativeAlert.h>

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <string>
#include <utility>

namespace coral::ui {

namespace {

NSWindow* alertParent(Window* parent) {
  if (parent == nullptr || !NSApp.isActive) return nil;
  NSWindow* window = [NSApp keyWindow] != nil ? [NSApp keyWindow] : [NSApp mainWindow];
  return window != nil && window.isVisible ? window : nil;
}

int runAlert(NativeAlertOptions options, Window* parent) {
  if (![NSThread isMainThread]) return -1;
  if (options.buttons.empty()) options.buttons.emplace_back("OK");
  @autoreleasepool {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = options.style == AlertStyle::Critical ? NSAlertStyleCritical
      : (options.style == AlertStyle::Warning ? NSAlertStyleWarning : NSAlertStyleInformational);
    if (!options.title.empty()) {
      alert.messageText = [NSString stringWithUTF8String:options.title.c_str()];
    }
    if (!options.message.empty()) {
      alert.informativeText = [NSString stringWithUTF8String:options.message.c_str()];
    }
    if (options.accessoryView != nullptr) {
      NSView* accessory = (__bridge NSView*)options.accessoryView;
      if ([accessory isKindOfClass:[NSView class]]) alert.accessoryView = accessory;
    }
    for (const std::string& title: options.buttons) {
      [alert addButtonWithTitle:title.empty()
        ? @"OK" : [NSString stringWithUTF8String:title.c_str()]];
    }
    const int defaultIndex = std::clamp(options.defaultButton, 0,
      static_cast<int>(alert.buttons.count) - 1);
    for (NSInteger index = 0; index < static_cast<NSInteger>(alert.buttons.count); ++index) {
      NSButton* button = alert.buttons[static_cast<NSUInteger>(index)];
      [button setKeyEquivalent:index == defaultIndex ? @"\r" : @""];
    }

    NSWindow* parentWindow = alertParent(parent);
    NSModalResponse response = NSModalResponseCancel;
    if (parentWindow != nil) {
      __block BOOL finished = NO;
      __block NSModalResponse sheetResponse = NSModalResponseCancel;
      [alert beginSheetModalForWindow:parentWindow completionHandler:^(NSModalResponse value) {
        sheetResponse = value;
        finished = YES;
      }];
      while (!finished) {
        @autoreleasepool {
          [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                   beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
        }
      }
      response = sheetResponse;
    } else {
      response = [alert runModal];
    }
    const NSInteger index = response - NSAlertFirstButtonReturn;
    return index >= 0 && index < static_cast<NSInteger>(alert.buttons.count)
      ? static_cast<int>(index) : -1;
  }
}

} // namespace

int showNativeAlert(const NativeAlertOptions& options, Window* parent) {
  NativeAlertOptions copy = options;
  // AppKit owns the actual window association; `parent` gates the sheet decision, while the active
  // key/main window is the one a sheet can attach to.
  return runAlert(std::move(copy), parent);
}

bool supportsNativeAlerts() noexcept {
  return true;
}

} // namespace coral::ui
