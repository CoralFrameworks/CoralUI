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
#include <ui/ColorPanel.h>
#include <ui/DatePickerPanel.h>
#include <ui/Command.h>
#include <ui/Flex.h>
#include <ui/FontPanel.h>
#include <array>
#include <filesystem>
#include <ui/InputSource.h>
#include <ui/NativeAlert.h>
#include <ui/macos/NativeButton.h>
#include <ui/macos/NativeColorWell.h>
#include <ui/macos/NativePopUpButton.h>
#include <ui/macos/NativeDatePicker.h>
#include <ui/macos/NativeSearchField.h>
#include <ui/macos/NativeSegmentedControl.h>
#include <ui/macos/NativeSlider.h>
#include <ui/macos/NativeProgressIndicator.h>
#include <ui/macos/NativeStepper.h>
#include <ui/macos/NativeSwitch.h>
#include <ui/NativeViewLayer.h>
#include <ui/Notification.h>
#include <ui/ScreenCapture.h>
#include <ui/SecurityScopedBookmark.h>
#include <ui/SpellCheck.h>
#include <ui/Table.h>
#include <ui/SystemImage.h>
#include <ui/DateFormat.h>
#include <ui/NumberFormat.h>
#include <ui/SystemFonts.h>
#include <ui/SystemSettings.h>
#include <ui/TextInput.h>
#include <ui/Window.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

NSView* findCoralContentView(NSView* root) {
  if (root == nil) return nil;
  if ([root respondsToSelector:@selector(setMarkedText:selectedRange:replacementRange:)]) {
    return root;
  }
  for (NSView* child in root.subviews) {
    if (NSView* found = findCoralContentView(child)) return found;
  }
  return nil;
}

NSMenuItem* findMenuItem(NSMenu* menu, NSString* title) {
  if (menu == nil) return nil;
  for (NSMenuItem* item in menu.itemArray) {
    if ([item.title isEqualToString:title]) return item;
    if (NSMenuItem* nested = findMenuItem(item.submenu, title)) return nested;
  }
  return nil;
}

NSVisualEffectView* findVisualEffectView(NSView* root) {
  if (root == nil) return nil;
  if ([root isKindOfClass:[NSVisualEffectView class]]) return (NSVisualEffectView*)root;
  for (NSView* child in root.subviews) {
    if (NSVisualEffectView* found = findVisualEffectView(child)) return found;
  }
  return nil;
}

NSSegmentedControl* findSegmentedControl(NSView* root) {
  if (root == nil) return nil;
  if ([root isKindOfClass:[NSSegmentedControl class]]) return (NSSegmentedControl*)root;
  for (NSView* child in root.subviews) {
    if (NSSegmentedControl* found = findSegmentedControl(child)) return found;
  }
  return nil;
}

NSTableView* findTableView(NSView* root) {
  if (root == nil) return nil;
  if ([root isKindOfClass:[NSTableView class]]) return (NSTableView*)root;
  for (NSView* child in root.subviews) {
    if (NSTableView* found = findTableView(child)) return found;
  }
  return nil;
}

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
    auto table = std::make_shared<coral::ui::Table>();
    table->setAccessibleName("Grid");
    table->setRows({ { nullptr, nullptr }, { nullptr, nullptr } });
    root->addChild(button);
    root->addChild(input);
    root->addChild(table);
    window->setRoot(root);
    window->focusLayer(input.get());
    bool clicked = false;
    button->setOnClick([&clicked] { clicked = true; });
    window->renderFrame();
    window->pump();

    NSWindow* native = [NSApp windows].firstObject;
    NSView* view = findCoralContentView(native.contentView);
    if (view == nil) {
      std::fprintf(stderr, "FAIL: could not find the CoralUI content view\n");
      return 1;
    }

    // Native NSToolbar: items resolve through the delegate and an item action reaches the window.
    std::string activated;
    window->setToolbarActionHandler([&activated](std::string_view identifier) {
      activated = std::string(identifier);
    });
    window->setToolbarItems({ coral::ui::Window::ToolbarItem{
      "com.coral.test", "Test", "gearshape", "Test item" } });
    NSToolbar* toolbar = native.toolbar;
    NSToolbarItem* toolbarItem = toolbar.items.firstObject;
    if (toolbar == nil || toolbar.items.count != 1 || toolbarItem == nil ||
        ![toolbarItem.label isEqualToString:@"Test"]) {
      std::fprintf(stderr, "FAIL: native toolbar did not expose its item\n");
      return 1;
    }
    [(id)toolbarItem.target performSelector:toolbarItem.action withObject:toolbarItem];
    if (activated != "com.coral.test") {
      std::fprintf(stderr, "FAIL: toolbar action produced '%s'\n", activated.c_str());
      return 1;
    }
    NSButton* customToolbarView = [NSButton buttonWithTitle:@"Custom" target:nil action:nil];
    window->setToolbarItems({ coral::ui::Window::ToolbarItem{
      "com.coral.custom", "Custom", "", "", (__bridge void*)customToolbarView } });
    toolbar = native.toolbar;
    toolbarItem = toolbar.items.firstObject;
    if (toolbarItem == nil || toolbarItem.view != customToolbarView) {
      std::fprintf(stderr, "FAIL: native toolbar custom view was not installed\n");
      return 1;
    }
    if (!window->supportsNativePopover()) {
      std::fprintf(stderr, "FAIL: native popover is not advertised\n");
      return 1;
    }
    NSButton* popoverContent = [NSButton buttonWithTitle:@"Popover" target:nil action:nil];
    window->showNativePopover((__bridge void*)popoverContent,
      coral::ui::Rect::MakeLTRB(20.0f, 40.0f, 120.0f, 60.0f));
    if (!window->isNativePopoverVisible()) {
      std::fprintf(stderr, "FAIL: native popover did not open\n");
      return 1;
    }
    window->hideNativePopover();
    if (window->isNativePopoverVisible()) {
      std::fprintf(stderr, "FAIL: native popover did not close\n");
      return 1;
    }

    int safeAreaCalls = 0;
    window->setSafeAreaChangedHandler([&safeAreaCalls](const coral::ui::Insets) {
      ++safeAreaCalls;
    });
    window->setTitlebarVisible(false);
    if (safeAreaCalls == 0) {
      std::fprintf(stderr, "FAIL: safe-area change handler was not called\n");
      return 1;
    }
    window->setToolbarStyle(coral::ui::Window::ToolbarStyle::UnifiedCompact);
    if (native.toolbarStyle != NSWindowToolbarStyleUnifiedCompact) {
      std::fprintf(stderr, "FAIL: toolbar style was not applied\n");
      return 1;
    }
    window->setTitlebarVisible(false);
    if (native.titleVisibility != NSWindowTitleHidden) {
      std::fprintf(stderr, "FAIL: titlebar visibility was not applied\n");
      return 1;
    }
    window->setTitlebarVisible(true);
    window->setWindowButtonsVisible(false);
    if (![native standardWindowButton:NSWindowCloseButton].hidden) {
      std::fprintf(stderr, "FAIL: window buttons were not hidden\n");
      return 1;
    }
    window->setWindowButtonsVisible(true);
    if (!window->toolbarAutoHideInFullScreen()) {
      std::fprintf(stderr, "FAIL: toolbar fullscreen auto-hide default is off\n");
      return 1;
    }
    window->setToolbarAutoHideInFullScreen(false);
    if (window->toolbarAutoHideInFullScreen()) {
      std::fprintf(stderr, "FAIL: toolbar fullscreen auto-hide did not disable\n");
      return 1;
    }
    window->setToolbarAutoHideInFullScreen(true);
    window->setToolbarItems({});
    window->setToolbarItems({
      coral::ui::Window::ToolbarItem{ "com.coral.a", "A", "gearshape", "" },
      coral::ui::Window::ToolbarItem{ "space", "", "", "", nullptr, true, false },
      coral::ui::Window::ToolbarItem{ "separator", "", "", "", nullptr, false, true },
    });
    toolbar = native.toolbar;
    if (toolbar.items.count != 3 ||
        ![toolbar.items[1].itemIdentifier isEqualToString:NSToolbarFlexibleSpaceItemIdentifier] ||
        toolbar.items[2].view == nil) {
      std::fprintf(stderr, "FAIL: toolbar flexible space/custom separator was not installed\n");
      return 1;
    }
    window->setToolbarItems({});

    bool commandRan = false;
    window->commands().bind("test.action", [&commandRan] { commandRan = true; });
    window->commands().setInfo("test.action", { "Do Thing", "Test", 0, -1, false, "gearshape" });
    NSMenuItem* commandItem = findMenuItem(NSApp.mainMenu, @"Do Thing");
    if (commandItem == nil || commandItem.target == nil || commandItem.image == nil) {
      std::fprintf(stderr, "FAIL: CommandRegistry did not publish a native menu item/icon\n");
      return 1;
    }
    [(id)commandItem.target performSelector:commandItem.action withObject:commandItem];
    if (!commandRan) {
      std::fprintf(stderr, "FAIL: native command menu did not invoke the registry\n");
      return 1;
    }
    NSMenu* dockMenu = [(id)NSApp.delegate applicationDockMenu:NSApp];
    NSMenuItem* dockCommand = findMenuItem(dockMenu, @"Do Thing");
    if (dockCommand == nil || dockCommand.target == nil) {
      std::fprintf(stderr, "FAIL: CommandRegistry did not publish a Dock menu item\n");
      return 1;
    }
    window->commands().unbind("test.action");

    bool fileCommandRan = false;
    window->commands().bind("test.file", [&fileCommandRan] { fileCommandRan = true; });
    window->commands().setInfo("test.file", { "File Action", "File", 0 });
    NSMenuItem* fileCommand = findMenuItem(NSApp.mainMenu, @"File Action");
    if (fileCommand == nil || ![fileCommand.menu.title isEqualToString:@"File"]) {
      std::fprintf(stderr, "FAIL: File-group command was not placed in the File menu\n");
      return 1;
    }
    [(id)fileCommand.target performSelector:fileCommand.action withObject:fileCommand];
    if (!fileCommandRan) {
      std::fprintf(stderr, "FAIL: File-group native command did not invoke\n");
      return 1;
    }
    window->commands().unbind("test.file");

    window->commands().bind("test.first", [] {});
    window->commands().setInfo("test.first", { "First Action", "File", 0, 0 });
    NSMenu* fileMenu = nil;
    for (NSMenuItem* item in NSApp.mainMenu.itemArray) {
      if ([item.title isEqualToString:@"File"]) fileMenu = item.submenu;
    }
    if (fileMenu == nil || fileMenu.itemArray.count == 0 ||
        ![fileMenu.itemArray.firstObject.title isEqualToString:@"First Action"]) {
      std::fprintf(stderr, "FAIL: command menuIndex was not honoured\n");
      return 1;
    }
    window->commands().unbind("test.first");

    int titlebarSelection = -1;
    window->setTitlebarAccessorySelectionHandler([&titlebarSelection](int index) {
      titlebarSelection = index;
    });
    window->setTitlebarAccessoryItems({ { "One", "" }, { "Two", "" } });
    NSSegmentedControl* accessory = nil;
    for (NSTitlebarAccessoryViewController* controller in native.titlebarAccessoryViewControllers) {
      accessory = findSegmentedControl(controller.view);
      if (accessory != nil) break;
    }
    if (native.titlebarAccessoryViewControllers.count != 1 || accessory == nil ||
        accessory.segmentCount != 2) {
      std::fprintf(stderr, "FAIL: native titlebar accessory did not expose its segments\n");
      return 1;
    }
    accessory.selectedSegment = 1;
    [(id)accessory.target performSelector:accessory.action withObject:accessory];
    if (titlebarSelection != 1) {
      std::fprintf(stderr, "FAIL: titlebar accessory selection produced %d\n", titlebarSelection);
      return 1;
    }

    NSSegmentedControl* customAccessory = [[NSSegmentedControl alloc] init];
    customAccessory.segmentCount = 1;
    [customAccessory setLabel:@"Custom" forSegment:0];
    window->setTitlebarAccessoryView((__bridge void*)customAccessory);
    if (native.titlebarAccessoryViewControllers.count != 1 ||
        native.titlebarAccessoryViewControllers.firstObject.view != customAccessory) {
      std::fprintf(stderr, "FAIL: arbitrary titlebar accessory view was not installed\n");
      return 1;
    }
    window->setTitlebarAccessoryView(nullptr);
    window->setTitlebarAccessoryItems({});

    std::string sidebarSelection;
    std::string editedId;
    std::string editedLabel;
    window->setSidebarSelectionHandler([&sidebarSelection](std::string_view identifier) {
      sidebarSelection = std::string(identifier);
    });
    window->setSidebarEditHandler(
      [&editedId, &editedLabel](std::string_view identifier, std::string_view label) {
        editedId = std::string(identifier);
        editedLabel = std::string(label);
      });
    std::string movedId;
    std::string movedParent;
    int movedIndex = -1;
    window->setSidebarMoveHandler(
      [&movedId, &movedParent, &movedIndex](std::string_view identifier,
        std::string_view parent, int index) {
        movedId = std::string(identifier);
        movedParent = std::string(parent);
        movedIndex = index;
      });
    std::string contextItem;
    std::string contextAction;
    window->setSidebarContextActionHandler(
      [&contextItem, &contextAction](std::string_view item, std::string_view action) {
        contextItem = std::string(item);
        contextAction = std::string(action);
      });
    window->setSidebarItems({
      { "section", "Section", "", true },
      { "one", "One", "folder", false, { { "child", "Child", "doc" } }, true },
      { "two", "Two", "gearshape" },
    });
    NSSplitViewController* split = (NSSplitViewController*)native.contentViewController;
    NSTableView* sidebar = findTableView(split.view);
    window->setSidebarContextActions({ { "rename", "Rename" } });
    if (sidebar.menu == nil || sidebar.menu.itemArray.count != 1 ||
        ![sidebar.menu.itemArray.firstObject.title isEqualToString:@"Rename"]) {
      std::fprintf(stderr, "FAIL: sidebar context menu was not published\n");
      return 1;
    }
    [sidebar selectRowIndexes:[NSIndexSet indexSetWithIndex:1] byExtendingSelection:NO];
    NSMenuItem* contextMenuItem = sidebar.menu.itemArray.firstObject;
    [(id)contextMenuItem.target performSelector:contextMenuItem.action
      withObject:contextMenuItem];
    if (contextItem != "one" || contextAction != "rename") {
      std::fprintf(stderr, "FAIL: sidebar context action produced '%s'/'%s'\n",
        contextItem.c_str(), contextAction.c_str());
      return 1;
    }
    window->setSidebarContextActions({});
    if (![split isKindOfClass:[NSSplitViewController class]] || split.splitViewItems.count != 2 ||
        sidebar == nil || sidebar.numberOfRows != 4 ||
        ![sidebar.delegate tableView:sidebar isGroupRow:0]) {
      std::fprintf(stderr, "FAIL: native sidebar did not expose its split item/rows\n");
      return 1;
    }
    id editableRow = [sidebar itemAtRow:1];
    id<NSOutlineViewDelegate> outlineDelegate = (id<NSOutlineViewDelegate>)sidebar.delegate;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-method-access"
    [outlineDelegate outlineView:sidebar setObjectValue:@"Renamed"
                  forTableColumn:sidebar.tableColumns.firstObject item:editableRow];
#pragma clang diagnostic pop
    if (editedId != "one" || editedLabel != "Renamed") {
      std::fprintf(stderr, "FAIL: sidebar edit produced '%s'/'%s'\n",
        editedId.c_str(), editedLabel.c_str());
      return 1;
    }
    [sidebar selectRowIndexes:[NSIndexSet indexSetWithIndex:3] byExtendingSelection:NO];
    if (sidebarSelection != "two") {
      std::fprintf(stderr, "FAIL: sidebar selection produced '%s'\n", sidebarSelection.c_str());
      return 1;
    }

    if (!window->moveSidebarItem("two", "", 0) || movedId != "two" ||
        !movedParent.empty() || movedIndex != 0 ||
        ![[[sidebar itemAtRow:0] valueForKey:@"label"] isEqualToString:@"Two"]) {
      std::fprintf(stderr, "FAIL: sidebar move did not reorder the outline\n");
      return 1;
    }
    window->setSidebarItems({});

    window->setSecureInputEnabled(true);
    if (!window->isSecureInputEnabled()) {
      std::fprintf(stderr, "FAIL: secure event input did not enable\n");
      return 1;
    }
    window->setSecureInputEnabled(false);

    std::string bookmarkError;
    const std::optional<std::string> bookmark =
      coral::ui::createSecurityScopedBookmark("/tmp", &bookmarkError);
    if (!bookmark.has_value()) {
      std::fprintf(stderr, "bookmark create failed: %s\n", bookmarkError.c_str());
    } else {
      auto access = coral::ui::SecurityScopedAccess::fromBookmark(*bookmark, &bookmarkError);
      if (!access.has_value() || access->path() != "/tmp" || !access->startAccessing()) {
        std::fprintf(stderr, "FAIL: security-scoped bookmark did not resolve/access: %s\n",
          bookmarkError.c_str());
        return 1;
      }
      access->stopAccessing();
    }

    const coral::ui::KeyboardInputSource inputSource = coral::ui::currentKeyboardInputSource();
    if (!coral::ui::supportsInputSourceMonitoring() || !inputSource.valid()) {
      std::fprintf(stderr, "FAIL: keyboard input source was not readable\n");
      return 1;
    }

    const coral::ui::SystemLocale locale = coral::ui::currentSystemLocale();
    if (!coral::ui::supportsSystemLocaleMonitoring() || locale.identifier.empty() ||
        locale.firstWeekday < 1 || locale.firstWeekday > 7) {
      std::fprintf(stderr, "FAIL: system locale was not readable\n");
      return 1;
    }

    window->setDockBadge("3");
    if (![NSApp.dockTile.badgeLabel isEqualToString:@"3"]) {
      std::fprintf(stderr, "FAIL: Dock badge was not applied\n");
      return 1;
    }
    window->setDockBadge("");

    const std::vector<coral::ui::SpellIssue> issues = coral::ui::spellCheck("helo wrold");
    const std::vector<std::string> suggestions = coral::ui::spellSuggestions("helo");
    if (!coral::ui::supportsSpellChecking() || issues.empty() || suggestions.empty()) {
      std::fprintf(stderr, "FAIL: spell checker/suggestions failed for 'helo wrold'\n");
      return 1;
    }

    input->setText("hello world");
    id axButton = findElementByLabel(view, "Press me");
    id axInput = findElementByLabel(view, "Search");
    id axGrid = findElementByLabel(view, "Grid");
    if (axButton == nil || axInput == nil || axGrid == nil) {
      std::fprintf(stderr, "FAIL: accessibility tree did not expose the button/input\n");
      return 1;
    }
    if ([axGrid accessibilityRowCount] != 2 || [axGrid accessibilityColumnCount] != 2 ||
        [axGrid accessibilityRowIndexRange].length != 0) {
      std::fprintf(stderr, "FAIL: AX grid dimensions were not exposed\n");
      return 1;
    }
    if (![[axButton accessibilityRole] isEqualToString:NSAccessibilityButtonRole] ||
        ![[axInput accessibilityRole] isEqualToString:NSAccessibilityTextFieldRole]) {
      std::fprintf(stderr, "FAIL: accessibility roles are %s / %s\n",
        [[axButton accessibilityRole] UTF8String], [[axInput accessibilityRole] UTF8String]);
      return 1;
    }
    const NSRect axButtonFrame = [axButton accessibilityFrame];
    id hit = [view accessibilityHitTest:NSMakePoint(NSMidX(axButtonFrame),
      NSMidY(axButtonFrame))];
    if (hit == nil || ![[hit accessibilityLabel] isEqualToString:@"Press me"]) {
      std::fprintf(stderr, "FAIL: AX hit test did not find the button\n");
      return 1;
    }

    if (![axInput accessibilityEditable] ||
        ![[axInput accessibilityValue] isEqualToString:@"hello world"]) {
      std::fprintf(stderr, "FAIL: AX text value/editability not exposed\n");
      return 1;
    }
    [view unmarkText];
    input->setText("");
    [axInput setAccessibilityValue:@"ax edited"];
    window->pump();
    window->pump();
    if (input->text() != "ax edited") {
      std::fprintf(stderr, "FAIL: AX setValue produced '%s'\n", input->text().c_str());
      return 1;
    }
    input->setText("");

    bool nativeButtonClicked = false;
    auto nativeButton = coral::ui::macos::NativeButton::create("Native Button",
      [&nativeButtonClicked] { nativeButtonClicked = true; });
    if (nativeButton == nullptr || nativeButton->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native button wrapper did not create a sized view\n");
      return 1;
    }
    nativeButton->setControlSize(coral::ui::macos::NativeControlSize::Small);
    if (nativeButton->controlSize() != coral::ui::macos::NativeControlSize::Small) {
      std::fprintf(stderr, "FAIL: native button size did not set\n");
      return 1;
    }
    nativeButton->setBounds(coral::ui::Rect::MakeLTRB(140.0f, 40.0f, 280.0f, 80.0f));
    root->addChild(nativeButton);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native button frame did not render\n");
      return 1;
    }
    NSButton* wrappedButton = (__bridge NSButton*)nativeButton->nativeView();
    nativeButton->setEnabled(false);
    if (nativeButton->enabled() || wrappedButton.enabled) {
      std::fprintf(stderr, "FAIL: native button enabled did not set\n");
      return 1;
    }
    nativeButton->setEnabled(true);
    [wrappedButton performClick:nil];
    if (!nativeButtonClicked) {
      std::fprintf(stderr, "FAIL: native button action did not reach the callback\n");
      return 1;
    }
    root->removeChild(nativeButton);
    nativeButton.reset();

    std::string searched;
    auto nativeSearch = coral::ui::macos::NativeSearchField::create("Search",
      [&searched](std::string_view value) { searched = std::string(value); });
    if (nativeSearch == nullptr || nativeSearch->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native search field wrapper did not create a sized view\n");
      return 1;
    }
    nativeSearch->setControlSize(coral::ui::macos::NativeControlSize::Small);
    if (nativeSearch->controlSize() != coral::ui::macos::NativeControlSize::Small) {
      std::fprintf(stderr, "FAIL: native search field size did not set\n");
      return 1;
    }
    nativeSearch->setBounds(coral::ui::Rect::MakeLTRB(140.0f, 100.0f, 380.0f, 140.0f));
    root->addChild(nativeSearch);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native search field frame did not render\n");
      return 1;
    }
    NSSearchField* wrappedSearch = (__bridge NSSearchField*)nativeSearch->nativeView();
    wrappedSearch.stringValue = @"query";
    [(id)wrappedSearch.target performSelector:wrappedSearch.action withObject:wrappedSearch];
    if (searched != "query" || nativeSearch->text() != "query") {
      std::fprintf(stderr, "FAIL: native search field callback/text did not work\n");
      return 1;
    }
    root->removeChild(nativeSearch);
    nativeSearch.reset();

    int selectedPopup = -1;
    std::string selectedPopupTitle;
    auto nativePopUp = coral::ui::macos::NativePopUpButton::create({ "One", "Two", "Three" }, 1,
      [&selectedPopup, &selectedPopupTitle](int index, std::string_view title) {
        selectedPopup = index;
        selectedPopupTitle = std::string(title);
      });
    if (nativePopUp == nullptr || nativePopUp->selectedIndex() != 1 ||
        nativePopUp->itemTitle(1) != "Two") {
      std::fprintf(stderr, "FAIL: native pop-up button did not expose its items\n");
      return 1;
    }
    nativePopUp->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 40.0f, 580.0f, 80.0f));
    root->addChild(nativePopUp);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native pop-up frame did not render\n");
      return 1;
    }
    NSPopUpButton* wrappedPopUp = (__bridge NSPopUpButton*)nativePopUp->nativeView();
    [wrappedPopUp selectItemAtIndex:2];
    [(id)wrappedPopUp.target performSelector:wrappedPopUp.action withObject:wrappedPopUp];
    if (selectedPopup != 2 || selectedPopupTitle != "Three") {
      std::fprintf(stderr, "FAIL: native pop-up selection did not reach the callback\n");
      return 1;
    }
    root->removeChild(nativePopUp);
    nativePopUp.reset();

    graphics::Color selectedWellColor;
    auto nativeColorWell = coral::ui::macos::NativeColorWell::create(
      graphics::Color::MakeRGBA8(10, 20, 30, 255),
      [&selectedWellColor](const graphics::Color color) { selectedWellColor = color; });
    if (nativeColorWell == nullptr || nativeColorWell->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native color well wrapper did not create a sized view\n");
      return 1;
    }
    nativeColorWell->setControlSize(coral::ui::macos::NativeControlSize::Small);
    nativeColorWell->setEnabled(false);
    if (nativeColorWell->controlSize() != coral::ui::macos::NativeControlSize::Small ||
        nativeColorWell->enabled()) {
      std::fprintf(stderr, "FAIL: native color well state did not set\n");
      return 1;
    }
    nativeColorWell->setEnabled(true);
    nativeColorWell->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 100.0f, 460.0f, 140.0f));
    root->addChild(nativeColorWell);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native color well frame did not render\n");
      return 1;
    }
    NSColorWell* wrappedWell = (__bridge NSColorWell*)nativeColorWell->nativeView();
    wrappedWell.color = [NSColor colorWithSRGBRed:0.2 green:0.4 blue:0.6 alpha:1.0];
    [(id)wrappedWell.target performSelector:wrappedWell.action withObject:wrappedWell];
    if (selectedWellColor.red < 0.19f || selectedWellColor.green < 0.39f ||
        selectedWellColor.blue < 0.59f) {
      std::fprintf(stderr, "FAIL: native color well callback did not work\n");
      return 1;
    }
    root->removeChild(nativeColorWell);
    nativeColorWell.reset();

    bool switchValue = true;
    auto nativeSwitch = coral::ui::macos::NativeSwitch::create("Enabled", false,
      [&switchValue](const bool on) { switchValue = on; });
    if (nativeSwitch == nullptr || nativeSwitch->isOn() ||
        nativeSwitch->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native switch wrapper did not create a sized view\n");
      return 1;
    }
    nativeSwitch->setControlSize(coral::ui::macos::NativeControlSize::Small);
    if (nativeSwitch->controlSize() != coral::ui::macos::NativeControlSize::Small) {
      std::fprintf(stderr, "FAIL: native switch size did not set\n");
      return 1;
    }
    nativeSwitch->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 160.0f, 560.0f, 200.0f));
    root->addChild(nativeSwitch);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native switch frame did not render\n");
      return 1;
    }
    NSButton* wrappedSwitch = (__bridge NSButton*)nativeSwitch->nativeView();
    nativeSwitch->setEnabled(false);
    if (nativeSwitch->enabled() || wrappedSwitch.enabled) {
      std::fprintf(stderr, "FAIL: native switch enabled did not set\n");
      return 1;
    }
    nativeSwitch->setEnabled(true);
    wrappedSwitch.state = NSControlStateValueOn;
    [(id)wrappedSwitch.target performSelector:wrappedSwitch.action withObject:wrappedSwitch];
    if (!switchValue || !nativeSwitch->isOn()) {
      std::fprintf(stderr, "FAIL: native switch callback did not work\n");
      return 1;
    }
    nativeSwitch->setOn(false);
    if (nativeSwitch->isOn()) {
      std::fprintf(stderr, "FAIL: native switch setOn did not work\n");
      return 1;
    }
    root->removeChild(nativeSwitch);
    nativeSwitch.reset();

    double sliderValue = 0.0;
    auto nativeSlider = coral::ui::macos::NativeSlider::create(0.0, 10.0, 5.0,
      [&sliderValue](const double value) { sliderValue = value; });
    if (nativeSlider == nullptr || nativeSlider->value() < 4.99 ||
        nativeSlider->value() > 5.01 || nativeSlider->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native slider wrapper did not initialize\n");
      return 1;
    }
    nativeSlider->setControlSize(coral::ui::macos::NativeControlSize::Regular);
    if (nativeSlider->controlSize() != coral::ui::macos::NativeControlSize::Regular) {
      std::fprintf(stderr, "FAIL: native slider size did not set\n");
      return 1;
    }
    nativeSlider->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 200.0f, 640.0f, 240.0f));
    root->addChild(nativeSlider);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native slider frame did not render\n");
      return 1;
    }
    NSSlider* wrappedSlider = (__bridge NSSlider*)nativeSlider->nativeView();
    wrappedSlider.doubleValue = 7.5;
    [(id)wrappedSlider.target performSelector:wrappedSlider.action withObject:wrappedSlider];
    if (sliderValue < 7.49 || sliderValue > 7.51) {
      std::fprintf(stderr, "FAIL: native slider callback did not work\n");
      return 1;
    }
    nativeSlider->setRange(0.0, 5.0);
    nativeSlider->setValue(9.0);
    if (nativeSlider->value() > 5.01) {
      std::fprintf(stderr, "FAIL: native slider range clamp did not work\n");
      return 1;
    }
    root->removeChild(nativeSlider);
    nativeSlider.reset();

    auto nativeProgress = coral::ui::macos::NativeProgressIndicator::create(false);
    if (nativeProgress == nullptr || nativeProgress->indeterminate() ||
        nativeProgress->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native progress indicator did not initialize\n");
      return 1;
    }
    nativeProgress->setValue(0.75);
    if (nativeProgress->style() != coral::ui::macos::NativeProgressStyle::Bar) {
      std::fprintf(stderr, "FAIL: native progress indicator style did not initialize\n");
      return 1;
    }
    nativeProgress->setStyle(coral::ui::macos::NativeProgressStyle::Spinning);
    if (nativeProgress->style() != coral::ui::macos::NativeProgressStyle::Spinning) {
      std::fprintf(stderr, "FAIL: native progress indicator style did not set\n");
      return 1;
    }
    nativeProgress->setStyle(coral::ui::macos::NativeProgressStyle::Bar);
    nativeProgress->setControlSize(coral::ui::macos::NativeControlSize::Small);
    if (nativeProgress->controlSize() != coral::ui::macos::NativeControlSize::Small) {
      std::fprintf(stderr, "FAIL: native progress indicator size did not set\n");
      return 1;
    }
    nativeProgress->setDisplayedWhenStopped(false);
    if (nativeProgress->displayedWhenStopped()) {
      std::fprintf(stderr, "FAIL: native progress indicator displayedWhenStopped did not set\n");
      return 1;
    }
    if (nativeProgress->value() < 0.74 || nativeProgress->value() > 0.76) {
      std::fprintf(stderr, "FAIL: native progress indicator value did not set\n");
      return 1;
    }
    nativeProgress->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 200.0f, 560.0f, 220.0f));
    root->addChild(nativeProgress);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native progress indicator frame did not render\n");
      return 1;
    }
    nativeProgress->setIndeterminate(true);
    if (!nativeProgress->indeterminate()) {
      std::fprintf(stderr, "FAIL: native progress indicator indeterminate did not set\n");
      return 1;
    }
    root->removeChild(nativeProgress);
    nativeProgress.reset();

    double stepperValue = 0.0;
    auto nativeStepper = coral::ui::macos::NativeStepper::create(0.0, 10.0, 5.0,
      [&stepperValue](const double value) { stepperValue = value; });
    if (nativeStepper == nullptr || nativeStepper->value() < 4.99 ||
        nativeStepper->value() > 5.01 || nativeStepper->preferredSize().height <= 0.0f) {
      std::fprintf(stderr, "FAIL: native stepper did not initialize\n");
      return 1;
    }
    nativeStepper->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 200.0f, 420.0f, 240.0f));
    root->addChild(nativeStepper);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native stepper frame did not render\n");
      return 1;
    }
    NSStepper* wrappedStepper = (__bridge NSStepper*)nativeStepper->nativeView();
    wrappedStepper.doubleValue = 7.0;
    [(id)wrappedStepper.target performSelector:wrappedStepper.action withObject:wrappedStepper];
    if (stepperValue < 6.99 || stepperValue > 7.01) {
      std::fprintf(stderr, "FAIL: native stepper callback did not work\n");
      return 1;
    }
    nativeStepper->setControlSize(coral::ui::macos::NativeControlSize::Large);
    if (nativeStepper->controlSize() != coral::ui::macos::NativeControlSize::Large) {
      std::fprintf(stderr, "FAIL: native stepper size did not set\n");
      return 1;
    }
    nativeStepper->setValueWraps(true);
    nativeStepper->setAutorepeat(false);
    if (!wrappedStepper.valueWraps || wrappedStepper.autorepeat) {
      std::fprintf(stderr, "FAIL: native stepper options did not set\n");
      return 1;
    }
    nativeStepper->setIncrement(2.5);
    if (nativeStepper->increment() < 2.49 || nativeStepper->increment() > 2.51) {
      std::fprintf(stderr, "FAIL: native stepper increment did not set\n");
      return 1;
    }
    nativeStepper->setRange(0.0, 5.0);
    nativeStepper->setValue(9.0);
    if (nativeStepper->value() > 5.01) {
      std::fprintf(stderr, "FAIL: native stepper range clamp did not work\n");
      return 1;
    }
    root->removeChild(nativeStepper);
    nativeStepper.reset();

    auto nativeDatePicker = coral::ui::macos::NativeDatePicker::create(
      std::chrono::year_month_day{ std::chrono::year{2026}, std::chrono::November,
        std::chrono::day{24} });
    if (nativeDatePicker == nullptr || nativeDatePicker->preferredSize().width <= 0.0f) {
      std::fprintf(stderr, "FAIL: native date picker wrapper did not create a sized view\n");
      return 1;
    }
    nativeDatePicker->setControlSize(coral::ui::macos::NativeControlSize::Small);
    if (nativeDatePicker->controlSize() != coral::ui::macos::NativeControlSize::Small) {
      std::fprintf(stderr, "FAIL: native date picker size did not set\n");
      return 1;
    }
    const auto initialNativeDate = nativeDatePicker->date();
    if (static_cast<int>(initialNativeDate.year()) != 2026 ||
        static_cast<unsigned>(initialNativeDate.month()) != 11u ||
        static_cast<unsigned>(initialNativeDate.day()) != 24u) {
      std::fprintf(stderr, "FAIL: native date picker initial date was wrong\n");
      return 1;
    }
    nativeDatePicker->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 240.0f, 660.0f, 280.0f));
    root->addChild(nativeDatePicker);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native date picker frame did not render\n");
      return 1;
    }
    NSDatePicker* wrappedDatePicker = (__bridge NSDatePicker*)nativeDatePicker->nativeView();
    NSDateComponents* changedComponents = [[NSDateComponents alloc] init];
    changedComponents.year = 2027;
    changedComponents.month = 1;
    changedComponents.day = 2;
    wrappedDatePicker.dateValue = [[NSCalendar currentCalendar]
      dateFromComponents:changedComponents];
    nativeDatePicker->setDate(nativeDatePicker->date());
    const auto changedNativeDate = nativeDatePicker->date();
    if (static_cast<int>(changedNativeDate.year()) != 2027 ||
        static_cast<unsigned>(changedNativeDate.month()) != 1u ||
        static_cast<unsigned>(changedNativeDate.day()) != 2u) {
      std::fprintf(stderr, "FAIL: native date picker setDate did not work\n");
      return 1;
    }
    root->removeChild(nativeDatePicker);
    nativeDatePicker.reset();

    int segmentedSelection = -1;
    auto nativeSegmented = coral::ui::macos::NativeSegmentedControl::create(
      { "One", "Two", "Three" }, 1,
      [&segmentedSelection](const int index) { segmentedSelection = index; });
    if (nativeSegmented == nullptr || nativeSegmented->segmentCount() != 3 ||
        nativeSegmented->selectedIndex() != 1) {
      std::fprintf(stderr, "FAIL: native segmented control did not initialize\n");
      return 1;
    }
    nativeSegmented->setBounds(coral::ui::Rect::MakeLTRB(400.0f, 280.0f, 640.0f, 320.0f));
    root->addChild(nativeSegmented);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native segmented control frame did not render\n");
      return 1;
    }
    NSSegmentedControl* wrappedSegmented =
      (__bridge NSSegmentedControl*)nativeSegmented->nativeView();
    wrappedSegmented.selectedSegment = 2;
    [(id)wrappedSegmented.target performSelector:wrappedSegmented.action
      withObject:wrappedSegmented];
    if (segmentedSelection != 2 || nativeSegmented->selectedIndex() != 2) {
      std::fprintf(stderr, "FAIL: native segmented control callback did not work\n");
      return 1;
    }
    root->removeChild(nativeSegmented);
    nativeSegmented.reset();

    NSButton* nativeContentButton = [NSButton buttonWithTitle:@"Native" target:nil action:nil];
    auto nativeContent = std::make_shared<coral::ui::NativeViewLayer>(
      (__bridge void*)nativeContentButton);
    nativeContent->setBounds(coral::ui::Rect::MakeLTRB(140.0f, 140.0f, 260.0f, 180.0f));
    root->addChild(nativeContent);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native view layer frame did not render\n");
      return 1;
    }
    if (nativeContentButton.superview == nil) {
      std::fprintf(stderr, "FAIL: native view layer was not attached\n");
      return 1;
    }
    nativeContent->setVisible(false);
    if (!nativeContentButton.hidden) {
      std::fprintf(stderr, "FAIL: native view layer visibility was not synced\n");
      return 1;
    }
    nativeContent->setVisible(true);
    if (nativeContentButton.hidden) {
      std::fprintf(stderr, "FAIL: native view layer did not become visible again\n");
      return 1;
    }
    root->removeChild(nativeContent);
    nativeContent.reset();
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: native view layer removal frame did not render\n");
      return 1;
    }
    if (nativeContentButton.superview != nil) {
      std::fprintf(stderr, "FAIL: native view layer was not removed\n");
      return 1;
    }

    [axButton accessibilityPerformAction:NSAccessibilityPressAction];
    window->pump();
    if (!clicked) {
      std::fprintf(stderr, "FAIL: AXPress did not invoke the button\n");
      return 1;
    }
    NSArray<NSString*>* actions = [axButton accessibilityActionNames];
    if (![actions containsObject:NSAccessibilityConfirmAction] ||
        ![actions containsObject:NSAccessibilityCancelAction]) {
      std::fprintf(stderr, "FAIL: AX confirm/cancel actions are missing\n");
      return 1;
    }
    clicked = false;
    [axButton accessibilityPerformAction:NSAccessibilityConfirmAction];
    window->pump();
    if (!clicked) {
      std::fprintf(stderr, "FAIL: AXConfirm did not invoke the button\n");
      return 1;
    }

    // The AppKit input context path: marked composition then a commit.
    [view unmarkText];
    input->setText("");
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
    const std::string correction = coral::ui::spellCorrection("helo");
    if (!correction.empty()) {
      input->setAutocorrectEnabled(true);
      input->setText("");
      for (const char32_t character: std::u32string(U"helo ")) input->onTextInput(character);
      if (input->text() != correction + " ") {
        std::fprintf(stderr, "FAIL: autocorrect produced '%s' instead of '%s '\n",
          input->text().c_str(), correction.c_str());
        return 1;
      }
      input->setAutocorrectEnabled(false);
      input->setText("");
    }

    input->setSpellCheckEnabled(true);
    input->setText("helo wrold");
    if (input->spellIssues().empty()) {
      std::fprintf(stderr, "FAIL: TextInput did not expose its spell issues\n");
      return 1;
    }
    input->setText("hello world");
    if (!input->spellIssues().empty()) {
      std::fprintf(stderr, "FAIL: corrected TextInput still reports spell issues\n");
      return 1;
    }
    input->setSpellCheckEnabled(false);

    const std::shared_ptr<const graphics::Font> systemFont = coral::ui::systemUIFont(14.0f);
    if (systemFont == nullptr || systemFont->measureWidth("Coral") <= 0.0f) {
      std::fprintf(stderr, "FAIL: system UI font did not resolve\n");
      return 1;
    }
    if (systemFont->hasVariations()) {
      const auto axes = systemFont->variationAxes();
      const auto axis = std::find_if(axes.begin(), axes.end(),
        [](const graphics::FontVariationAxis& a) { return a.tag == "wght"; });
      if (axis != axes.end()) {
        const float value = axis->defaultValue == axis->maxValue
          ? axis->minValue : axis->defaultValue;
        std::string variationError;
        const auto varied = systemFont->withVariations({ { "wght", value } }, &variationError);
        if (varied == nullptr || !varied->hasVariations()) {
          std::fprintf(stderr, "FAIL: variable font axis was not applied: %s\n",
            variationError.c_str());
          return 1;
        }
      }
    }
    const coral::ui::NumberFormatOptions systemNumber = coral::ui::systemNumberFormat();
    const std::string formattedNumber = coral::ui::formatNumber(1234.5, systemNumber);
    const coral::ui::DateFormatOptions systemDate = coral::ui::systemDateFormat();
    const std::string formattedDate = coral::ui::formatDate(
      std::chrono::year_month_day{ std::chrono::year{2026}, std::chrono::November,
        std::chrono::day{24} }, systemDate);
    if (formattedNumber.empty() || formattedDate.empty()) {
      std::fprintf(stderr, "FAIL: system locale formatting produced an empty string\n");
      return 1;
    }

    const std::array<graphics::FontFeature, 1> features = {
      graphics::FontFeature{ .tag = "liga", .value = 0 },
    };
    if (systemFont->shape("fi", graphics::Font::Direction::Auto, features).glyphs().empty()) {
      std::fprintf(stderr, "FAIL: font feature shaping returned no glyphs\n");
      return 1;
    }

    const graphics::Image tintedSymbol = coral::ui::systemSymbolImage("gearshape", {
      .pointSize = 16.0f,
      .tint = graphics::Color::MakeRGBA8(255, 0, 0, 255),
    });
    if (tintedSymbol.isEmpty()) {
      std::fprintf(stderr, "FAIL: tinted SF Symbol did not resolve\n");
      return 1;
    }

    const graphics::Image symbol = coral::ui::systemSymbolImage("gearshape", { .pointSize = 16.0f });
    if (!coral::ui::supportsSystemSymbols() || symbol.isEmpty()) {
      std::fprintf(stderr, "FAIL: SF Symbol did not resolve to an image\n");
      return 1;
    }

    if (!coral::ui::supportsUserNotifications()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise notifications\n");
      return 1;
    }
    if (coral::ui::screenCaptureAccess() == coral::ui::ScreenCaptureAccess::Unavailable) {
      std::fprintf(stderr, "FAIL: screen capture permission API is unavailable\n");
      return 1;
    }
    const coral::ui::SystemAppearance appearance = window->systemAppearance();
    if (!appearance.valid || appearance.accentColor == 0) {
      std::fprintf(stderr, "FAIL: system accent colour was not reported\n");
      return 1;
    }
    coral::ui::SystemAppearance synthetic = appearance;
    synthetic.accentColor = 0xFFFF0000u;
    window->applySystemAppearance(synthetic);
    if (window->theme().accent.red < 0.99f || window->theme().accent.green > 0.01f ||
        window->theme().accent.blue > 0.01f) {
      std::fprintf(stderr, "FAIL: system accent was not applied to the theme\n");
      return 1;
    }
    window->applySystemAppearance(appearance);

    window->setContentFocusRingEnabled(true);
    NSView* focusView = findCoralContentView(native.contentView);
    if (!window->contentFocusRingEnabled() || focusView == nil ||
        focusView.focusRingType != NSFocusRingTypeExterior) {
      std::fprintf(stderr, "FAIL: content focus ring was not enabled\n");
      return 1;
    }
    window->setContentFocusRingEnabled(false);

    if (!window->supportsWindowCapture()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise ScreenCaptureKit capture\n");
      return 1;
    }
    const std::string printPath = "/tmp/coral-ui-print.pdf";
    std::error_code printError;
    std::filesystem::remove(printPath, printError);
    if (!window->printToFile(printPath) || !std::filesystem::exists(printPath) ||
        std::filesystem::file_size(printPath, printError) == 0) {
      std::fprintf(stderr, "FAIL: printToFile did not produce a PDF\n");
      return 1;
    }
    if (!window->supportsPrinting()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise printing\n");
      return 1;
    }
    const coral::ui::FrameStatistics frameStats = window->frameStatistics();
    if (window->outputColorSpace() != frameStats.outputColorSpace) {
      std::fprintf(stderr, "FAIL: output color space query diverged from frame stats\n");
      return 1;
    }
    if (!window->supportsShareSheet()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise share sheets\n");
      return 1;
    }
    window->showShareSheet("Coral UI smoke");
    if (!coral::ui::supportsNativeDatePicker()) {
      std::fprintf(stderr, "FAIL: native date picker is not advertised\n");
      return 1;
    }
    if (!coral::ui::showNativeDatePicker(std::chrono::year_month_day{
          std::chrono::year{2026}, std::chrono::November, std::chrono::day{24} })) {
      std::fprintf(stderr, "FAIL: native date picker did not open\n");
      return 1;
    }
    if (!coral::ui::isNativeDatePickerVisible()) {
      std::fprintf(stderr, "FAIL: native date picker did not become visible\n");
      return 1;
    }
    coral::ui::hideNativeDatePicker();

    if (!coral::ui::supportsColorPanel()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise a color panel\n");
      return 1;
    }
    graphics::Color chosenColor;
    if (!coral::ui::showColorPanel(graphics::Color::MakeRGBA8(10, 20, 30, 255),
          [&chosenColor](const graphics::Color color) { chosenColor = color; })) {
      std::fprintf(stderr, "FAIL: the Cocoa color panel did not open\n");
      return 1;
    }
    NSColorPanel* colorPanel = [NSColorPanel sharedColorPanel];
    colorPanel.color = [NSColor colorWithSRGBRed:0.25 green:0.5 blue:0.75 alpha:1.0];
    [(id)[colorPanel valueForKey:@"target"] performSelector:@selector(changeColor:)
      withObject:colorPanel];
    if (chosenColor.red < 0.24f || chosenColor.green < 0.49f || chosenColor.blue < 0.74f) {
      std::fprintf(stderr, "FAIL: color panel selection did not reach the handler\n");
      return 1;
    }
    coral::ui::hideColorPanel();

    if (!coral::ui::supportsFontPanel()) {
      std::fprintf(stderr, "FAIL: the Cocoa backend does not advertise a font panel\n");
      return 1;
    }
    std::shared_ptr<const graphics::Font> chosenFont;
    if (!coral::ui::showFontPanel([&chosenFont](std::shared_ptr<const graphics::Font> font) {
          chosenFont = std::move(font);
        })) {
      std::fprintf(stderr, "FAIL: the Cocoa font panel did not open\n");
      return 1;
    }
    NSFontManager* fontManager = [NSFontManager sharedFontManager];
    [fontManager setSelectedFont:[NSFont systemFontOfSize:18.0] isMultiple:NO];
    [(id)fontManager.target performSelector:@selector(changeFont:) withObject:fontManager];
    if (chosenFont == nullptr) {
      std::fprintf(stderr, "FAIL: font panel selection did not reach the handler\n");
      return 1;
    }
    coral::ui::hideFontPanel();
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
    // With no app-level veto, a window close interceptor must still be able to veto quitting.
    coral::ui::setShouldTerminateHandler({});
    window->setCloseRequestedCallback([] { return false; });
    const NSApplicationTerminateReply windowVeto = [delegate applicationShouldTerminate:NSApp];
    window->setCloseRequestedCallback({});
    const bool lifecycleOk = opened.size() == 1 && opened[0] == "/tmp/coral-open-test" &&
      reopened && reply == NSTerminateCancel && windowVeto == NSTerminateCancel;
    coral::ui::setOpenFilesHandler({});
    coral::ui::setReopenHandler({});
    coral::ui::setShouldTerminateHandler({});
    if (!lifecycleOk) {
      std::fprintf(stderr, "FAIL: lifecycle callbacks opened=%zu reopened=%d terminate=%ld\n",
        opened.size(), reopened ? 1 : 0, static_cast<long>(windowVeto));
      return 1;
    }

    std::printf("native AX/IME/symbol/alert/lifecycle/spell smoke: roles, AXPress, IME commit, "
                "%dx%d SF Symbol, native alerts, lifecycle, %zu spell issues, %zu suggestions, "
                "input=%s locale=%s h24=%d weekStart=%d\n", symbol.width(), symbol.height(),
                issues.size(), suggestions.size(), inputSource.id.c_str(), locale.identifier.c_str(),
                locale.uses24HourClock ? 1 : 0, locale.firstWeekday);
    return 0;
  }
}
