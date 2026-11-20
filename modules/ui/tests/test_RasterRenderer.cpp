/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ui/ColorLayer.h>
#include <ui/RasterRenderer.h>

#include <graphics/Bitmap.h>
#include <gtest/gtest.h>

#include <memory>

using namespace coral;

namespace {

TEST(UiRasterRenderer, RendersARootLayerWithoutADevice) {
  coral::ui::RasterRenderer renderer(8, 6);
  renderer.setRoot(std::make_shared<coral::ui::ColorLayer>(
    coral::ui::Color::MakeRGBA8(200, 30, 40, 255)));

  EXPECT_EQ(renderer.rendererKind(), coral::ui::RendererKind::Raster);
  EXPECT_TRUE(renderer.capabilities().externalImages.supported == false);
  EXPECT_TRUE(renderer.renderFrame());
  EXPECT_TRUE(renderer.hasFrame());

  coral::ui::Bitmap bitmap;
  ASSERT_TRUE(renderer.snapshot(&bitmap));
  ASSERT_EQ(bitmap.width(), 8);
  ASSERT_EQ(bitmap.height(), 6);
  const auto pixels = bitmap.makePixmap();
  const uint32_t pixel = pixels.getPixelARGB32(4, 3);
  EXPECT_EQ((pixel >> 16) & 0xFFu, 200u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 30u);
  EXPECT_EQ(pixel & 0xFFu, 40u);

  EXPECT_TRUE(renderer.recover());
  EXPECT_FALSE(renderer.hasFrame()) << "recovery invalidates the cached frame";
}

} // namespace
