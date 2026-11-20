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

#include <ui/CheckBox.h>
#include <ui/ColorLayer.h>
#include <ui/ElementSpec.h>
#include <ui/Reconciler.h>

#include <gtest/gtest.h>

#include <memory>

namespace coral::ui {
namespace {

TEST(UiElementSpec, ParsesAFlatRectWithLayoutProperties) {
  const ElementPtr element = parseElementSpec("Rect#r background=#102030 radius=4 width=40 height=20");
  ASSERT_NE(element, nullptr);
  Reconciler reconciler;
  const auto layer = reconciler.mount(element);
  ASSERT_NE(layer, nullptr);
  EXPECT_FLOAT_EQ(layer->bounds().getWidth(), 40.0f);
  EXPECT_FLOAT_EQ(layer->bounds().getHeight(), 20.0f);
  EXPECT_FLOAT_EQ(layer->clipRadius(), 4.0f);
  const auto colour = std::dynamic_pointer_cast<ColorLayer>(layer);
  ASSERT_NE(colour, nullptr);
  EXPECT_EQ(colour->color().toR8G8B8A8()[0], 0x10);
  EXPECT_EQ(colour->color().toR8G8B8A8()[1], 0x20);
  EXPECT_EQ(colour->color().toR8G8B8A8()[2], 0x30);
}

TEST(UiElementSpec, ParsesACheckBoxWithItsLabelAndState) {
  const ElementPtr element = parseElementSpec("CheckBox text=\"Remember me\" checked=true");
  ASSERT_NE(element, nullptr);
  Reconciler reconciler;
  const auto layer = reconciler.mount(element);
  const auto checkBox = std::dynamic_pointer_cast<CheckBox>(layer);
  ASSERT_NE(checkBox, nullptr);
  EXPECT_TRUE(checkBox->checked());
  EXPECT_EQ(checkBox->label(), "Remember me");
}

TEST(UiElementSpec, FallsBackToTheElementRegistry) {
  ElementRegistry registry;
  EXPECT_TRUE(registry.add("Card", [](const ElementProps&, std::vector<ElementPtr>) {
    return std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "card");
  }));
  EXPECT_NE(parseElementSpec("Card", registry), nullptr);
  EXPECT_EQ(parseElementSpec("Nope", registry), nullptr)
    << "an unregistered type fails instead of guessing";
}

TEST(UiElementSpec, RejectsMalformedInput) {
  EXPECT_EQ(parseElementSpec(""), nullptr);
  EXPECT_EQ(parseElementSpec("Rect background=#GGGGGG"), nullptr);
  EXPECT_EQ(parseElementSpec("Rect opacity="), nullptr);
  EXPECT_EQ(parseElementSpec("Rect width=10"), nullptr) << "width without height is ambiguous";
  EXPECT_EQ(parseElementSpec("Rect unknown=1"), nullptr);
  EXPECT_EQ(parseElementSpec("#key"), nullptr) << "a spec needs a type";
}

TEST(UiElementSpec, ParsingIsStableForReconciliation) {
  const ElementPtr first = parseElementSpec("Rect#a background=#112233 width=30 height=30");
  const ElementPtr second = parseElementSpec("Rect#a background=#112233 width=30 height=30");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_TRUE(first->measureEquivalent(*second))
    << "the same spec must reconcile as the same element";
}

} // namespace
} // namespace coral::ui
