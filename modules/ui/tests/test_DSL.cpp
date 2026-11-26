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

#include <ui/DSLCore.h>
#include <ui/Element.h>
#include <ui/Theme.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using namespace coral::ui;

TEST(DslApi, ThemeScopeNestsAndRestores) {
  const Theme first = Theme::dark();
  const Theme second = Theme::light();
  {
    dsl::ThemeScope outer(first);
    EXPECT_EQ(&dsl::theme(), &first);
    {
      dsl::ThemeScope inner(second);
      EXPECT_EQ(&dsl::theme(), &second);
    }
    EXPECT_EQ(&dsl::theme(), &first);
  }
  EXPECT_NE(&dsl::theme(), &first);
}

TEST(DslApi, ForEachBuildsAColumnOfRows) {
  const std::vector<std::string> items{ "one", "two", "three" };
  const ElementPtr element = dsl::ForEach(items,
    [](const std::string& item, const size_t index) {
      return dsl::Text(std::to_string(index) + ":" + item, dsl::TextOptions{ {}, std::nullopt,
        "row." + std::to_string(index) });
    }, dsl::StackOptions{ .gap = 4.0f, .key = "rows" });

  const auto stack = std::dynamic_pointer_cast<const StackElement>(element);
  ASSERT_NE(stack, nullptr);
  EXPECT_EQ(stack->axis(), Axis::Column);
  EXPECT_FLOAT_EQ(stack->gap(), 4.0f);
  EXPECT_EQ(stack->key(), "rows");
  ASSERT_EQ(stack->children().size(), 3u);
  EXPECT_EQ(stack->children()[0]->key(), "row.0");
  EXPECT_EQ(stack->children()[2]->key(), "row.2");
}

TEST(DslApi, WhenKeepsItsBranchesAndKey) {
  const ElementPtr element = dsl::When(true, dsl::Text("yes"), dsl::Text("no"), "branch");
  const auto when = std::dynamic_pointer_cast<const WhenElement>(element);
  ASSERT_NE(when, nullptr);
  EXPECT_TRUE(when->condition());
  EXPECT_EQ(when->key(), "branch");
  ASSERT_NE(when->thenBranch(), nullptr);
  ASSERT_NE(when->elseBranch(), nullptr);
}

TEST(DslApi, StableCoreComposesWithoutComponentSignatures) {
  // This test is intentionally a compile-time contract as much as a runtime one: the calls below must
  // not need a font, colour, ElementProps, or a components:: factory.
  const Theme theme = Theme::dark();
  dsl::ThemeScope scope(theme);
  const ElementPtr page = dsl::Page("Settings",
    dsl::Caption("Theme defaults"),
    dsl::Row(dsl::StackOptions{ .gap = 8.0f },
      dsl::Button("Save", [] {}),
      dsl::Spacer()),
    dsl::Card("Profile",
      dsl::Text("Coral"),
      dsl::FormField("Name", dsl::Text("Coral"))),
    dsl::Grid(dsl::GridOptions{ .columns = 2, .gap = 8.0f },
      dsl::Text("left"),
      dsl::Text("right")));
  ASSERT_NE(page, nullptr);
  EXPECT_EQ(page->kind(), ElementKind::Props);
}

} // namespace

TEST(DslApi, DesignTokensFollowTheActiveTheme) {
  Theme theme = Theme::dark();
  theme.spacing.md = 17.0f;
  theme.radius.lg = 9.0f;
  dsl::ThemeScope scope(theme);
  const dsl::Tokens tokens = dsl::tokens();
  EXPECT_FLOAT_EQ(tokens.spacing.md, 17.0f);
  EXPECT_FLOAT_EQ(tokens.radius.lg, 9.0f);
  EXPECT_EQ(tokens.text, theme.text);
  EXPECT_EQ(tokens.body, theme.body);
}

TEST(DslApi, PanelUsesDesignTokensWithoutElementProps) {
  Theme theme = Theme::dark();
  theme.spacing.md = 15.0f;
  theme.spacing.sm = 5.0f;
  theme.radius.md = 7.0f;
  dsl::ThemeScope scope(theme);
  const ElementPtr panel = dsl::Panel(dsl::Text("inside"));
  auto props = std::dynamic_pointer_cast<const PropsElement>(panel);
  ASSERT_NE(props, nullptr);
  EXPECT_EQ(props->props().backgroundRole, ThemeColorRole::Surface);
  EXPECT_FLOAT_EQ(props->props().cornerRadius.value_or(-1.0f), 7.0f);
  ASSERT_NE(props->children().front(), nullptr);
  const auto stack = std::dynamic_pointer_cast<const StackElement>(props->children().front());
  ASSERT_NE(stack, nullptr);
  EXPECT_FLOAT_EQ(stack->gap(), 5.0f);
  EXPECT_FLOAT_EQ(stack->padding().left, 15.0f);
}

TEST(DslApi, TypographyRolesUseThemeFacesAndTextRoles) {
  Theme theme = Theme::dark();
  theme.body = nullptr;
  theme.title = nullptr;
  dsl::ThemeScope scope(theme);
  const auto display = std::dynamic_pointer_cast<const TextElement>(dsl::Display("Title"));
  const auto body = std::dynamic_pointer_cast<const TextElement>(dsl::Body("Body"));
  const auto label = std::dynamic_pointer_cast<const TextElement>(dsl::Label("Label"));
  ASSERT_NE(display, nullptr);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(display->font(), theme.title);
  EXPECT_EQ(body->font(), theme.body);
  EXPECT_EQ(display->color(), theme.text);
  EXPECT_EQ(body->color(), theme.text);
  EXPECT_EQ(label->color(), theme.controls.foreground);
}