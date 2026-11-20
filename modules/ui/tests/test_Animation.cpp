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

#include <ui/Animation.h>
#include <ui/ScrollView.h>
#include <ui/ColorLayer.h>
#include <ui/Motion.h>
#include <ui/Curve.h>
#include <ui/Layer.h>

#include <gtest/gtest.h>

#include <chrono>

namespace coral::ui {
namespace {

using namespace std::chrono_literals;

TEST(UiAnimation, CurvesMatchTheirFamilies) {
  const auto linear = Curve::linear();
  EXPECT_FLOAT_EQ(linear->transform(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(linear->transform(0.5f), 0.5f);
  EXPECT_FLOAT_EQ(linear->transform(1.0f), 1.0f);

  EXPECT_LT(Curve::easeIn()->transform(0.5f), 0.5f);
  EXPECT_GT(Curve::easeOut()->transform(0.5f), 0.5f);
  EXPECT_NEAR(Curve::easeInOut()->transform(0.5f), 0.5f, 1e-3);
  EXPECT_FLOAT_EQ(Curve::forEasing(Easing::Linear)->transform(0.25f), 0.25f);

  // A custom curve may overshoot; the engine must not clamp it away.
  const auto overshoot = Curve::cubicBezier(0.3f, 1.6f, 0.7f, 1.6f);
  EXPECT_GT(overshoot->transform(0.5f), 1.0f);
}

TEST(UiAnimation, SpringSettlesOnItsTarget) {
  Spring spring;
  Spring::State state;
  for (int step = 0; step < 240; ++step) {
    spring.advance(state, 1.0f, 1.0f / 60.0f);
  }
  EXPECT_NEAR(state.value, 1.0f, 0.01f);
  EXPECT_TRUE(spring.settled(state, 1.0f, 0.02f));
}

TEST(UiAnimation, ControllerEasesFinishesOnceAndCanBeStopped) {
  AnimationController controller;
  int finished = 0;
  controller.setOnFinished([&finished] { ++finished; });
  controller.start(0.0f, 10.0f, 100ms, Curve::linear());
  EXPECT_TRUE(controller.running());
  EXPECT_FLOAT_EQ(controller.advance(25ms), 2.5f);
  EXPECT_FLOAT_EQ(controller.advance(25ms), 5.0f);
  EXPECT_FLOAT_EQ(controller.advance(100ms), 10.0f);
  EXPECT_FALSE(controller.running());
  EXPECT_FLOAT_EQ(controller.progress(), 1.0f);
  EXPECT_EQ(finished, 1);

  controller.advance(20ms);
  EXPECT_EQ(finished, 1) << "the finished callback fires once";

  controller.start(10.0f, 0.0f, 100ms, Curve::linear());
  controller.stop();
  EXPECT_FALSE(controller.running());
  EXPECT_FLOAT_EQ(controller.advance(50ms), 10.0f) << "a stopped controller does not advance";
}

TEST(UiAnimation, AnimatedFloatRetargetsFromTheCurrentValue) {
  AnimatedFloat value(0.0f);
  value.animateTo(10.0f, 100ms, Curve::linear());
  EXPECT_TRUE(value.advance(50ms));
  EXPECT_FLOAT_EQ(value.value(), 5.0f);

  value.animateTo(0.0f, 100ms, Curve::linear());
  EXPECT_TRUE(value.advance(50ms));
  EXPECT_FLOAT_EQ(value.value(), 2.5f) << "retargeting continues from the current value";

  value.advance(100ms);
  EXPECT_FLOAT_EQ(value.value(), 0.0f);
  EXPECT_FALSE(value.running());
}

TEST(UiAnimation, LayerBoundsAnimationInterpolatesAndSnapsAtEnd) {
  Layer layer;
  layer.setBounds(Rect::MakeWH(10.0f, 10.0f));
  const Rect target = Rect::MakeLTRB(100.0f, 100.0f, 110.0f, 110.0f);
  layer.animateBounds(target, 200ms, Curve::linear());
  EXPECT_TRUE(layer.animatingBounds());
  EXPECT_TRUE(layer.hasRunningAnimation());

  layer.onFrame(std::chrono::steady_clock::now() + 250ms);
  EXPECT_FALSE(layer.animatingBounds());
  EXPECT_EQ(layer.bounds().getLeft(), 100.0f);
  EXPECT_EQ(layer.bounds().getBottom(), 110.0f);

  layer.animateBounds(Rect::MakeWH(20.0f, 20.0f), 0ms);
  EXPECT_FALSE(layer.animatingBounds()) << "a zero duration snaps";
  EXPECT_EQ(layer.bounds().getWidth(), 20.0f);
}

} // namespace

namespace {

class FixedSizeLayer final : public Layer {
 public:
  FixedSizeLayer(const float width, const float height) : mSize{ width, height } {}

  Size measure(const Size& available) override {
    (void)available;
    setMeasuredSize(mSize);
    return mSize;
  }

 private:
  Size mSize;
};

} // namespace

TEST(UiAnimation, ReduceMotionSnapsAnimations) {
  auto layer = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 20, 30, 255));
  layer->setBounds(Rect::MakeXYWH(0.0f, 0.0f, 10.0f, 10.0f));

  setReduceMotion(true);
  layer->animateBounds(Rect::MakeXYWH(100.0f, 0.0f, 10.0f, 10.0f), std::chrono::milliseconds(200));
  EXPECT_FALSE(layer->animatingBounds()) << "reduce motion skips the interpolation";
  EXPECT_FLOAT_EQ(layer->bounds().getLeft(), 100.0f) << "it lands on the target immediately";

  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<FixedSizeLayer>(100.0f, 1000.0f);
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();
  view->animateScrollTo(400.0f, std::chrono::milliseconds(300));
  EXPECT_FALSE(view->scrollAnimating());
  EXPECT_FLOAT_EQ(view->scrollY(), 400.0f);

  setReduceMotion(false);
  layer->animateBounds(Rect::MakeXYWH(0.0f, 0.0f, 10.0f, 10.0f), std::chrono::milliseconds(200));
  EXPECT_TRUE(layer->animatingBounds()) << "normal motion still animates";
  setReduceMotion(false);
}

} // namespace coral::ui
