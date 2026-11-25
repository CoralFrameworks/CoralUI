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

#include <ui/FontPanel.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace {

std::mutex gHandlerMutex;
coral::ui::FontPanelHandler gHandler;

std::shared_ptr<const graphics::Font> fontFromNative(NSFont* font) {
  if (font == nil) return {};
  CTFontRef ctFont = (__bridge CTFontRef)font;
  CFURLRef url = (CFURLRef)CTFontCopyAttribute(ctFont, kCTFontURLAttribute);
  if (url == nullptr) return {};
  char path[4096]{};
  const bool ok = CFURLGetFileSystemRepresentation(url, true,
    reinterpret_cast<UInt8*>(path), sizeof(path));
  CFRelease(url);
  if (!ok) return {};
  std::shared_ptr<graphics::Typeface> typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) return {};
  return graphics::Font::create(std::move(typeface), static_cast<float>(font.pointSize));
}

void notifyFontPanel(const std::shared_ptr<const graphics::Font>& font) {
  coral::ui::FontPanelHandler handler;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    handler = gHandler;
  }
  if (handler) handler(font);
}

} // namespace

@interface CoralFontPanelTarget : NSObject
- (void)changeFont:(id)sender;
@end

@implementation CoralFontPanelTarget

- (void)changeFont:(id)sender {
  NSFontManager* manager = (NSFontManager*)sender;
  NSFont* selected = [manager selectedFont];
  if (selected == nil) return;
  const auto font = fontFromNative(selected);
  if (font != nullptr) notifyFontPanel(font);
}

@end

static CoralFontPanelTarget* __strong gTarget = nil;

namespace coral::ui {

bool showFontPanel(FontPanelHandler handler) {
  if (![NSThread isMainThread]) return false;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = std::move(handler);
  }
  if (gTarget == nil) gTarget = [[CoralFontPanelTarget alloc] init];
  NSFontManager* manager = [NSFontManager sharedFontManager];
  manager.target = gTarget;
  manager.action = @selector(changeFont:);
  NSFontPanel* panel = [NSFontPanel sharedFontPanel];
  [panel setPanelFont:[NSFont systemFontOfSize:14.0] isMultiple:NO];
  panel.hidesOnDeactivate = YES;
  [panel makeKeyAndOrderFront:nil];
  return true;
}

void hideFontPanel() {
  if (![NSThread isMainThread]) return;
  NSFontManager* manager = [NSFontManager sharedFontManager];
  manager.target = nil;
  manager.action = nil;
  [[NSFontPanel sharedFontPanel] orderOut:nil];
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = {};
  }
  gTarget = nil;
}

bool supportsFontPanel() noexcept {
  return true;
}

} // namespace coral::ui
