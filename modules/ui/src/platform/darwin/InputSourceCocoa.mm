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

#include <ui/InputSource.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include <mutex>
#include <string>
#include <utility>

namespace coral::ui {

namespace {

std::mutex gHandlerMutex;
std::function<void()> gHandler;
bool gObserverInstalled = false;

std::string stringProperty(TISInputSourceRef source, CFStringRef key) {
  if (source == nullptr) return {};
  CFTypeRef value = TISGetInputSourceProperty(source, key);
  if (value == nullptr || CFGetTypeID(value) != CFStringGetTypeID()) return {};
  NSString* string = (__bridge NSString*)value;
  const char* utf8 = string.UTF8String;
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

void inputSourceChanged(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef) {
  dispatch_async(dispatch_get_main_queue(), ^{
    std::function<void()> handler;
    {
      const std::lock_guard<std::mutex> lock(gHandlerMutex);
      handler = gHandler;
    }
    if (handler) handler();
  });
}

void installObserver() {
  if (gObserverInstalled) return;
  gObserverInstalled = true;
  CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), nullptr,
    &inputSourceChanged, kTISNotifySelectedKeyboardInputSourceChanged, nullptr,
    CFNotificationSuspensionBehaviorDeliverImmediately);
}

} // namespace

KeyboardInputSource currentKeyboardInputSource() {
  KeyboardInputSource info;
  TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
  if (source == nullptr) return info;
  info.id = stringProperty(source, kTISPropertyInputSourceID);
  info.name = stringProperty(source, kTISPropertyLocalizedName);
  CFRelease(source);
  return info;
}

void setInputSourceChangedHandler(std::function<void()> handler) {
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = std::move(handler);
  }
  if (gHandler) installObserver();
}

bool supportsInputSourceMonitoring() noexcept {
  return true;
}

} // namespace coral::ui
