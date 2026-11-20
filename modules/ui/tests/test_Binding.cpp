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
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/UiExecutor.h>
#include <ui/Window.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>

namespace {

using namespace coral::ui;
namespace observer = coral::observer;

std::shared_ptr<const graphics::Font> testFont(const float size) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, size) : nullptr;
}

TEST(UiBinding, APropertyUpdatesALayer) {
  observer::Property<std::string> title("initial");
  auto layer = std::make_shared<TextLayer>("", testFont(14.0f));
  auto observation = coral::ui::bindProperty(title, *layer,
    [layer](const std::string& value) { layer->setText(value); });
  EXPECT_EQ(layer->text(), "initial");
  title.set("changed");
  EXPECT_EQ(layer->text(), "changed");
}

TEST(UiBinding, TextBindingIsTwoWayAndDetaches) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 60;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto input = std::make_shared<TextInput>("", font);
  window->setRoot(input);
  window->focusLayer(input.get());

  auto model = std::make_shared<observer::Property<std::string>>("model");
  auto binding = bindText(*input, model);
  EXPECT_EQ(input->text(), "model");

  window->textInput(U'!');
  EXPECT_EQ(model->get(), "model!");

  model->set("changed");
  EXPECT_EQ(input->text(), "changed");

  binding.reset();
  model->set("after reset");
  EXPECT_EQ(input->text(), "changed");
  window->textInput(U'?');
  EXPECT_EQ(model->get(), "after reset");
}

TEST(UiBinding, AUiExecutorMarshalsModelChangesToThePump) {
  const auto font = testFont(14.0f);
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 60;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto input = std::make_shared<TextInput>("", font);
  window->setRoot(input);
  window->focusLayer(input.get());

  auto model = std::make_shared<observer::Property<std::string>>("before");
  auto binding = bindText(*input, model, std::make_shared<UiExecutor>(*window));

  std::thread producer([model] { model->set("from worker"); });
  producer.join();
  EXPECT_EQ(input->text(), "before"); // the executor has not pumped yet
  ASSERT_EQ(window->run(), 1);
  EXPECT_EQ(input->text(), "from worker");
}

} // namespace
