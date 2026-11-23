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

// A macOS native-window smoke for the two pieces that only exist in the AppKit backend:
// the NSAccessibility element tree/actions and the NSTextInputClient text path. A session with no
// window server reports 77 so CTest shows a skip instead of a false pass.

#import <Cocoa/Cocoa.h>

#include <ui/ApplicationLifecycle.h>
#include <ui/Button.h>
#include <ui/Flex.h>
#include <ui/NativeAlert.h>
#include <ui/SystemImage.h>
#include <ui/TextInput.h>
#include <ui/Window.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

id findElementByLabel(id root, const std::string& wanted) {
  NSMutableArray* pending = [NSMutableArray arrayWithObject:root];
  while (pending.count > 0) {
    id element = pending.firstObject;
    [pending removeObjectAtIndex:0];
    const char* label = [[element accessibilityLabel] UTF8String];
    if (label != nullptr && wanted == label) return element;
    [pending addObjectsFromArray:[element accessibilityChildren]];
  }
  return nil;
}

} // namespace

int main() {
  @autoreleasepool {
    coral::ui::Window::Options options;
    options.headless = false;
    options.width = 400;
    options.height = 300;
    auto window = coral::ui::Window::create(options);
    if (window == nullptr) {
      std::fprintf(stderr, "SKIP: no native Cocoa window in this session\n");
      return 77;
    }

    auto root = std::make_shared<coral::ui::Flex>(coral::ui::Axis::Column);
    auto button = std::make_shared<coral::ui::Button>("Press me", nullptr);
    auto input = std::make_shared<coral::ui::TextInput>("", nullptr);
    input->setAccessibleName("Search");
    root->addChild(button);
    root->addChild(input);
    window->setRoot(root);
    window->focusLayer(input.get());
    bool clicked = false;
    button->setOnClick([&clicked] { clicked = true; });
    window->renderFrame();
    window->pump();

    NSWindow* native = [NSApp windows].firstObject;
    NSView* view = native.contentView;
    id axButton = findElementByLabel(view, "Press me");
    id axInput = findElementByLabel(view, "Search");
    if (axButton == nil || axInput == nil) {
      std::fprintf(stderr, "FAIL: accessibility tree did not expose the button/input\n");
      return 1;
    }
    if (![[axButton accessibilityRole] isEqualToString:NSAccessibilityButtonRole] ||
        ![[axInput accessibilityRole] isEqualToString:NSAccessibilityTextFieldRole]) {
      std::fprintf(stderr, "FAIL: accessibility roles are %s / %s\n",
        [[axButton accessibilityRole] UTF8String], [[axInput accessibilityRole] UTF8String]);
      return 1;
    }
    [axButton accessibilityPerformAction:NSAccessibilityPressAction];
    window->pump();
    if (!clicked) {
      std::fprintf(stderr, "FAIL: AXPress did not invoke the button\n");
      return 1;
    }

    // The AppKit input context path: marked composition then a commit.
    [view setMarkedText:@"に" selectedRange:NSMakeRange(1, 0)
        replacementRange:NSMakeRange(NSNotFound, 0)];
    if (![view hasMarkedText]) {
      std::fprintf(stderr, "FAIL: marked text was not registered\n");
      return 1;
    }
    [view insertText:@"日本" replacementRange:NSMakeRange(NSNotFound, 0)];
    if (input->text() != "日本" || [view hasMarkedText]) {
      std::fprintf(stderr, "FAIL: IME commit produced '%s' (marked=%d)\n",
        input->text().c_str(), [view hasMarkedText] ? 1 : 0);
      return 1;
    }
    const graphics::Image symbol = coral::ui::systemSymbolImage("gearshape", { .pointSize = 16.0f });
    if (!coral::ui::supportsSystemSymbols() || symbol.isEmpty()) {
      std::fprintf(stderr, "FAIL: SF Symbol did not resolve to an image\n");
      return 1;
    }

    if (!coral::ui::supportsNativeAlerts()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise native alerts\n");
      return 1;
    }

    // AppKit lifecycle callbacks: open URLs, Dock reopen, and a vetoed termination must reach the
    // process-wide handlers rather than being swallowed by the delegate.
    std::vector<std::string> opened;
    bool reopened = false;
    coral::ui::setOpenFilesHandler([&opened](std::vector<std::string> paths) {
      opened = std::move(paths);
    });
    coral::ui::setReopenHandler([&reopened] { reopened = true; });
    coral::ui::setShouldTerminateHandler([] { return false; });
    id delegate = NSApp.delegate;
    [delegate application:NSApp openURLs:@[ [NSURL fileURLWithPath:@"/tmp/coral-open-test"] ]];
    [delegate applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
    const NSApplicationTerminateReply reply = [delegate applicationShouldTerminate:NSApp];
    const bool lifecycleOk = opened.size() == 1 && opened[0] == "/tmp/coral-open-test" &&
      reopened && reply == NSTerminateCancel;
    coral::ui::setOpenFilesHandler({});
    coral::ui::setReopenHandler({});
    coral::ui::setShouldTerminateHandler({});
    if (!lifecycleOk) {
      std::fprintf(stderr, "FAIL: lifecycle callbacks opened=%zu reopened=%d terminate=%ld\n",
        opened.size(), reopened ? 1 : 0, static_cast<long>(reply));
      return 1;
    }

    std::printf("native AX/IME/symbol/alert/lifecycle smoke: roles, AXPress, IME commit, %dx%d "
                "SF Symbol, native alerts, lifecycle\n", symbol.width(), symbol.height());
    return 0;
  }
}
