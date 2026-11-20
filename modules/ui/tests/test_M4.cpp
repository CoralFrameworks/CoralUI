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

#include <ui/ColorLayer.h>
#include <ui/Window.h>

#include <gtest/gtest.h>

namespace {

using namespace coral::ui;

TEST(UiWindow, AHeadlessWindowCanBeResizedBeforeItsFirstFrame) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  window->setRoot(root);

  ASSERT_TRUE(window->resize(32, 48));
  ASSERT_EQ(window->width(), 32);
  ASSERT_EQ(window->height(), 48);
  EXPECT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_EQ(bitmap.info().width(), 32u);
  EXPECT_EQ(bitmap.info().height(), 48u);
}

TEST(UiWindow, AHeadlessWindowRecoversItsSurface) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 255, 0, 255)));
  ASSERT_EQ(window->run(), 1);

  ASSERT_TRUE(window->recoverDevice());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_EQ(bitmap.info().width(), 16u);
  EXPECT_GT(window->lastFrameMilliseconds(), 0.0);
  EXPECT_EQ(window->frameCount(), 1);
}

TEST(UiWindow, HeadlessDpiScaleIsUnity) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  EXPECT_FLOAT_EQ(window->dpiScale(), 1.0f);
}

} // namespace
