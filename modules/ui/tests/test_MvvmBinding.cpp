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

#include <ui/Binding.h>
#include <ui/Command.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>
#include <ui/mvvm/Binding.h>
#include <ui/mvvm/Dispatcher.h>
#include <ui/mvvm/DataContext.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace coral::ui;
namespace mvvm = coral::mvvm;
namespace observer = coral::observer;

std::shared_ptr<const graphics::Font> testFont() {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, 14.0f) : nullptr;
}

struct PageModel {
  int selected = 0;
};

struct ItemModel {
  std::string title;
};

TEST(UiMvvm, DataContextResolvesTheNewestScopeAndPopsIt) {
  coral::ui::mvvm::DataContext context;
  auto page = std::make_shared<PageModel>();
  auto item = std::make_shared<ItemModel>();
  page->selected = 2;
  item->title = "clip";
  context.set(page);
  EXPECT_EQ(context.get<PageModel>(), page);
  EXPECT_EQ(context.get<ItemModel>(), nullptr);

  {
    coral::ui::mvvm::DataContext::ScopeGuard scope(context);
    context.set(item);
    EXPECT_EQ(context.get<ItemModel>(), item);
    EXPECT_EQ(context.get<PageModel>(), page);
  }
  EXPECT_EQ(context.get<ItemModel>(), nullptr);
  EXPECT_EQ(context.get<PageModel>(), page);
}

TEST(UiMvvm, PropertyBindingAppliesInitialValueAndChanges) {
  observer::Property<std::string> title("initial");
  auto label = std::make_shared<TextLayer>("", testFont());
  auto binding = bindProperty(title, *label,
    [label](const std::string& value) { label->setText(value); });
  EXPECT_EQ(label->text(), "initial");
  title.set("changed");
  EXPECT_EQ(label->text(), "changed");
}

TEST(UiMvvm, CommandBindingTracksCanExecuteAndInvokes) {
  int runs = 0;
  bool allowed = true;
  mvvm::Command command([&] { ++runs; }, [&] { return allowed; });
  std::vector<bool> availability;
  coral::ui::mvvm::CommandBinding binding(command, [&](const bool enabled) {
    availability.push_back(enabled);
  });
  ASSERT_EQ(availability, std::vector<bool>{ true });
  EXPECT_TRUE(binding.invoke());
  EXPECT_EQ(runs, 1);
  allowed = false;
  command.invalidateCanExecute();
  ASSERT_EQ(availability, (std::vector<bool>{ true, false }));
  EXPECT_FALSE(binding.canExecute());
  EXPECT_FALSE(binding.invoke());
  EXPECT_EQ(runs, 1);
}

TEST(UiMvvm, ObservableListBindingCoalescesStructuralChanges) {
  mvvm::ObservableList<int> items;
  int changes = 0;
  coral::ui::mvvm::ObservableListBinding<int> binding(items, [&] { ++changes; });
  EXPECT_EQ(changes, 1) << "the initial population callback";
  items.push_back(1);
  items.push_back(2);
  EXPECT_EQ(changes, 3);
  items.replace(0, 7);
  EXPECT_EQ(changes, 4);
  items.erase(0);
  EXPECT_EQ(changes, 5);
  items.clear();
  EXPECT_EQ(changes, 6);
}

TEST(UiMvvm, TextInputCanBeBoundToAProperty) {
  auto property = std::make_shared<observer::Property<std::string>>("hello");
  TextInput input("", testFont());
  auto binding = bindText(input, property);
  EXPECT_EQ(input.text(), "hello");
  property->set("world");
  EXPECT_EQ(input.text(), "world");
}

} // namespace

TEST(UiMvvm, ButtonCommandBindingTracksCanExecuteAndInvokes) {
  Button button("Run", testFont());
  int runs = 0;
  bool allow = true;
  coral::mvvm::Command command([&runs] { ++runs; }, [&allow] { return allow; });
  {
    coral::ui::mvvm::ButtonCommandBinding binding(button, command);
    EXPECT_TRUE(button.enabled());
    EXPECT_TRUE(binding.canExecute());

    allow = false;
    command.invalidateCanExecute();
    EXPECT_FALSE(button.enabled());
    EXPECT_FALSE(binding.invoke()) << "a disabled command must not execute";
    EXPECT_EQ(runs, 0);

    allow = true;
    command.invalidateCanExecute();
    EXPECT_TRUE(button.enabled());
    EXPECT_TRUE(binding.invoke());
    EXPECT_EQ(runs, 1);
  }
}

TEST(UiMvvm, WindowDispatcherMarshalsToTheUiThread) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 24;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  coral::ui::mvvm::WindowDispatcher dispatcher(*window);
  EXPECT_TRUE(dispatcher.isCurrentThread());

  int runs = 0;
  dispatcher.post([&runs] { ++runs; });
  EXPECT_EQ(runs, 0) << "post must queue instead of running inline";
  window->pump();
  EXPECT_EQ(runs, 1);

  bool workerSawOtherThread = false;
  std::thread worker([&] {
    workerSawOtherThread = !dispatcher.isCurrentThread();
    dispatcher.post([&runs] { ++runs; });
  });
  worker.join();
  EXPECT_TRUE(workerSawOtherThread);
  window->pump();
  EXPECT_EQ(runs, 2);

  dispatcher.detach();
}

TEST(UiMvvmBinding, RegistryCommandBindingRunsTheViewModelCommand) {
  CommandRegistry registry;
  bool allowed = false;
  int runs = 0;
  coral::mvvm::Command command([&runs] { ++runs; }, [&allowed] { return allowed; });
  coral::ui::mvvm::RegistryCommandBinding binding(registry, Shortcut::controlKey(Key::S),
    "doc.save", command);

  KeyEvent event;
  event.key = Key::S;
  event.control = true;
  EXPECT_FALSE(registry.isEnabled("doc.save"));
  EXPECT_FALSE(registry.dispatch(event)) << "a disabled command must not consume the shortcut";
  EXPECT_EQ(runs, 0);

  allowed = true;
  EXPECT_TRUE(registry.dispatch(event));
  EXPECT_EQ(runs, 1);
  EXPECT_TRUE(registry.invoke("doc.save"));
  EXPECT_EQ(runs, 2);
  EXPECT_EQ(registry.shortcutLabelFor("doc.save"), "Ctrl+S");
}

TEST(UiMvvmBinding, RegistryCommandBindingUnbindsOnDestruction) {
  CommandRegistry registry;
  coral::mvvm::Command command([] {});
  {
    coral::ui::mvvm::RegistryCommandBinding binding(registry, Shortcut{ Key::F5 }, "view.refresh",
      command);
    EXPECT_EQ(registry.size(), 1u);
  }
  EXPECT_TRUE(registry.empty());
}