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

#include <ui/ColorPanel.h>

#import <Cocoa/Cocoa.h>

#include <mutex>
#include <utility>

namespace {

std::mutex gHandlerMutex;
coral::ui::ColorPanelHandler gHandler;

graphics::Color colorFromNative(NSColor* color) {
  NSColor* srgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
  if (srgb == nil) srgb = color;
  return graphics::Color(static_cast<float>(srgb.redComponent),
    static_cast<float>(srgb.greenComponent), static_cast<float>(srgb.blueComponent),
    static_cast<float>(srgb.alphaComponent));
}

} // namespace

@interface CoralColorPanelTarget : NSObject
- (void)changeColor:(id)sender;
@end

@implementation CoralColorPanelTarget

- (void)changeColor:(id)sender {
  (void)sender;
  NSColorPanel* panel = [NSColorPanel sharedColorPanel];
  const graphics::Color color = colorFromNative(panel.color);
  coral::ui::ColorPanelHandler handler;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    handler = gHandler;
  }
  if (handler) handler(color);
}

@end

static CoralColorPanelTarget* __strong gTarget = nil;

namespace coral::ui {

bool showColorPanel(const graphics::Color initial, ColorPanelHandler handler) {
  if (![NSThread isMainThread]) return false;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = std::move(handler);
  }
  if (gTarget == nil) gTarget = [[CoralColorPanelTarget alloc] init];
  NSColorPanel* panel = [NSColorPanel sharedColorPanel];
  panel.target = gTarget;
  panel.action = @selector(changeColor:);
  panel.color = [NSColor colorWithSRGBRed:initial.red green:initial.green
    blue:initial.blue alpha:initial.alpha];
  [panel makeKeyAndOrderFront:nil];
  return true;
}

void hideColorPanel() {
  if (![NSThread isMainThread]) return;
  NSColorPanel* panel = [NSColorPanel sharedColorPanel];
  panel.target = nil;
  panel.action = nil;
  [panel orderOut:nil];
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = {};
  }
  gTarget = nil;
}

bool supportsColorPanel() noexcept {
  return true;
}

} // namespace coral::ui
