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

#include <ui/Button.h>
#include <ui/CheckBox.h>
#include <ui/RadioButton.h>
#include <ui/SegmentedControl.h>
#include <ui/Slider.h>
#include <ui/Toggle.h>
#include <ui/Style.h>
#include <ui/UI.h>
#include <ui/Window.h>

#include <gtest/gtest.h>

#include <memory>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace coral::ui {
namespace {

[[nodiscard]] std::array<uint8_t, 4> bytes(const Color& color) {
  return color.toR8G8B8A8();
}

[[nodiscard]] uint8_t red(const Color& color) {
  return color.toR8G8B8A8()[0];
}

TEST(UiStyle, UnstyledControlsFollowTheThemeStateTokens) {
  const Theme theme = Theme::dark();
  const Style empty;

  const ResolvedStyle normal = resolveStyle(empty, 0, theme);
  EXPECT_EQ(bytes(normal.background), bytes(theme.controls.background));
  EXPECT_EQ(bytes(normal.foreground), bytes(theme.controls.foreground));
  EXPECT_EQ(bytes(normal.border), bytes(theme.controls.border));
  EXPECT_EQ(normal.font, FontRole::Body);

  const ResolvedStyle hovered = resolveStyle(empty, stateFlag(ControlState::Hovered), theme);
  EXPECT_EQ(bytes(hovered.background), bytes(theme.controls.backgroundHover));

  const ResolvedStyle pressed = resolveStyle(empty,
    states({ ControlState::Hovered, ControlState::Pressed }), theme);
  EXPECT_EQ(bytes(pressed.background), bytes(theme.controls.backgroundPressed))
    << "pressed wins over hovered";

  const ResolvedStyle focused = resolveStyle(empty, stateFlag(ControlState::Focused), theme);
  EXPECT_EQ(bytes(focused.border), bytes(theme.controls.borderFocused));

  const ResolvedStyle disabled = resolveStyle(empty,
    states({ ControlState::Hovered, ControlState::Pressed, ControlState::Disabled }), theme);
  EXPECT_EQ(bytes(disabled.background), bytes(theme.controls.disabled))
    << "disabled wins over the interaction states";
  EXPECT_EQ(bytes(disabled.foreground), bytes(theme.controls.muted));
}

TEST(UiStyle, VariantsOverrideTheBaseInDeclarationOrder) {
  const Theme theme = Theme::dark();
  Style style;
  style.base.background = Color::MakeRGBA8(10, 10, 10, 255);
  style.variant(stateFlag(ControlState::Hovered),
    StylePatch{ .background = Color::MakeRGBA8(20, 20, 20, 255) });
  style.variant(states({ ControlState::Hovered, ControlState::Focused }),
    StylePatch{ .background = Color::MakeRGBA8(30, 30, 30, 255),
      .border = Color::MakeRGBA8(40, 40, 40, 255) });

  EXPECT_EQ(red(resolveStyle(style, 0, theme).background), 10);
  EXPECT_EQ(red(resolveStyle(style, stateFlag(ControlState::Hovered), theme).background), 20);
  const ResolvedStyle both = resolveStyle(style,
    states({ ControlState::Hovered, ControlState::Focused }), theme);
  EXPECT_EQ(red(both.background), 30) << "the later, more specific variant wins";
  EXPECT_EQ(red(both.border), 40);
}

TEST(UiStyle, LayoutAndTypographyValuesResolve) {
  const Theme theme = Theme::dark();
  Style style;
  style.base.radius = 12.0f;
  style.base.borderWidth = 2.0f;
  style.base.padding = Insets{ 4.0f, 6.0f, 8.0f, 10.0f };
  style.base.font = FontRole::Heading;

  const ResolvedStyle resolved = resolveStyle(style, 0, theme);
  EXPECT_FLOAT_EQ(resolved.radius, 12.0f);
  EXPECT_FLOAT_EQ(resolved.borderWidth, 2.0f);
  EXPECT_FLOAT_EQ(resolved.padding.left, 4.0f);
  EXPECT_FLOAT_EQ(resolved.padding.bottom, 10.0f);
  EXPECT_EQ(resolved.font, FontRole::Heading);
}

TEST(UiStyle, FontLadderIsOrderedAndReadsTheTheme) {
  const Theme theme = Theme::dark();
  EXPECT_FLOAT_EQ(fontSize(theme, FontRole::Caption), theme.fonts.caption);
  EXPECT_FLOAT_EQ(fontSize(theme, FontRole::Body), theme.fonts.body);
  EXPECT_LT(fontSize(theme, FontRole::Caption), fontSize(theme, FontRole::Body));
  EXPECT_LT(fontSize(theme, FontRole::Body), fontSize(theme, FontRole::Title));
  EXPECT_LT(fontSize(theme, FontRole::Title), fontSize(theme, FontRole::Heading));
  EXPECT_LT(fontSize(theme, FontRole::Heading), fontSize(theme, FontRole::Display));
}

TEST(UiStyle, StyleSheetResolvesNamedStylesAndReportsUnknownNames) {
  const Theme theme = Theme::dark();
  StyleSheet sheet;
  EXPECT_TRUE(sheet.names().empty());
  EXPECT_FALSE(sheet.contains("primary"));

  Style primary;
  primary.base.background = Color::MakeRGBA8(1, 2, 3, 255);
  primary.variant(stateFlag(ControlState::Disabled),
    StylePatch{ .background = Color::MakeRGBA8(9, 9, 9, 255) });
  sheet.set("primary", primary);
  sheet.set("secondary", Style{});

  EXPECT_EQ(sheet.size(), 2u);
  EXPECT_TRUE(sheet.contains("primary"));
  const std::vector<std::string> names = sheet.names();
  ASSERT_EQ(names.size(), 2u);
  EXPECT_EQ(names[0], "primary") << "names are sorted for deterministic output";
  EXPECT_EQ(names[1], "secondary");

  const auto resolved = sheet.resolve("primary", 0, theme);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(red(resolved->background), 1);
  const auto disabled = sheet.resolve("primary", stateFlag(ControlState::Disabled), theme);
  ASSERT_TRUE(disabled.has_value());
  EXPECT_EQ(red(disabled->background), 9);

  EXPECT_FALSE(sheet.resolve("missing", 0, theme).has_value());
  sheet.clear();
  EXPECT_TRUE(sheet.names().empty());
}

TEST(UiStyle, ThemeColorRoleExposesTheControlTokens) {
  const Theme theme = Theme::dark();
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::Foreground)), bytes(theme.controls.foreground));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::Border)), bytes(theme.controls.border));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::BorderFocused)),
    bytes(theme.controls.borderFocused));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::ControlBackground)),
    bytes(theme.controls.background));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::ControlHover)),
    bytes(theme.controls.backgroundHover));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::ControlPressed)),
    bytes(theme.controls.backgroundPressed));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::Disabled)), bytes(theme.controls.disabled));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::Track)), bytes(theme.controls.track));
  EXPECT_EQ(bytes(themeColor(theme, ThemeColorRole::Knob)), bytes(theme.controls.knob));
}


TEST(UiStyle, ButtonResolvesNamedStylesForItsInteractionStates) {
  const Theme theme = Theme::dark();
  Button button("OK", nullptr);
  button.setBounds(Rect::MakeXYWH(0.0f, 0.0f, 120.0f, 40.0f));

  Style primary;
  primary.base.background = Color::MakeRGBA8(10, 200, 10, 255);
  primary.base.foreground = Color::MakeRGBA8(250, 250, 250, 255);
  primary.variant(stateFlag(ControlState::Hovered),
    StylePatch{ .background = Color::MakeRGBA8(20, 220, 20, 255) });
  primary.variant(stateFlag(ControlState::Pressed),
    StylePatch{ .background = Color::MakeRGBA8(0, 150, 0, 255) });
  primary.variant(stateFlag(ControlState::Disabled),
    StylePatch{ .background = Color::MakeRGBA8(60, 60, 60, 255),
      .foreground = Color::MakeRGBA8(120, 120, 120, 255) });
  auto sheet = std::make_shared<StyleSheet>();
  sheet->set("primary", primary);
  button.setStyleSheet(sheet);
  button.setStyleName("primary");

  const auto normal = button.resolvedStyle();
  ASSERT_TRUE(normal.has_value());
  EXPECT_EQ(red(normal->background), 10);

  PointerEvent move;
  move.position = { 10.0f, 10.0f };
  button.onPointerMove(move);
  const auto hovered = button.resolvedStyle();
  ASSERT_TRUE(hovered.has_value());
  EXPECT_EQ(red(hovered->background), 20) << "hover comes from the style variant";

  PointerEvent down;
  down.position = { 10.0f, 10.0f };
  button.onPointerDown(down);
  const auto pressed = button.resolvedStyle();
  ASSERT_TRUE(pressed.has_value());
  EXPECT_EQ(red(pressed->background), 0);

  PointerEvent up;
  up.position = { 10.0f, 10.0f };
  button.onPointerUp(up);
  button.setEnabled(false);
  const auto disabled = button.resolvedStyle();
  ASSERT_TRUE(disabled.has_value());
  EXPECT_EQ(red(disabled->background), 60);
  EXPECT_EQ(red(disabled->foreground), 120);
  static_cast<void>(theme);
}

TEST(UiStyle, LayersWithoutAStyleNameKeepTheThemePath) {
  Layer layer;
  EXPECT_FALSE(layer.styleSheet() != nullptr);
  EXPECT_FALSE(layer.resolvedStyle().has_value());

  layer.setStyleSheet(std::make_shared<StyleSheet>());
  EXPECT_FALSE(layer.resolvedStyle().has_value()) << "a sheet without a name resolves nothing";
  layer.setStyleName("missing");
  EXPECT_FALSE(layer.resolvedStyle().has_value()) << "an unknown name keeps the theme path";
  EXPECT_EQ(layer.styleStates(), 0u);
}


TEST(UiStyle, ControlsReportInteractionStatesToTheResolver) {
  auto sheet = std::make_shared<StyleSheet>();
  Style style;
  style.variant(stateFlag(ControlState::Checked),
    StylePatch{ .background = Color::MakeRGBA8(7, 7, 7, 255) });
  style.variant(stateFlag(ControlState::Focused),
    StylePatch{ .border = Color::MakeRGBA8(9, 9, 9, 255) });
  sheet->set("choice", style);

  CheckBox box("Sync", nullptr, true);
  box.onFocusChanged(true);
  EXPECT_NE(box.styleStates() & stateFlag(ControlState::Checked), 0u)
    << "a checked checkbox reports Checked";
  EXPECT_NE(box.styleStates() & stateFlag(ControlState::Focused), 0u);
  box.setStyleSheet(sheet);
  box.setStyleName("choice");
  const auto boxStyle = box.resolvedStyle();
  ASSERT_TRUE(boxStyle.has_value());
  EXPECT_EQ(red(boxStyle->background), 7);
  EXPECT_EQ(red(boxStyle->border), 9);

  Toggle toggle("Live", nullptr, true);
  EXPECT_NE(toggle.styleStates() & stateFlag(ControlState::Checked), 0u);
  RadioButton radio("One", nullptr, true);
  EXPECT_NE(radio.styleStates() & stateFlag(ControlState::Checked), 0u);
  Slider slider("Opacity", nullptr, 0.0, 1.0, 0.5);
  slider.onFocusChanged(true);
  EXPECT_NE(slider.styleStates() & stateFlag(ControlState::Focused), 0u);
  SegmentedControl segments({ "A", "B" }, nullptr);
  EXPECT_NE(segments.styleStates() & stateFlag(ControlState::Selected), 0u);
}


namespace {

class ThemeProbeLayer final : public Layer {
 public:
  void applyTheme(const Theme& theme) override {
    background = theme.background;
    ++applications;
  }

  Color background{};
  int applications = 0;
};

} // namespace

TEST(UiStyle, WindowThemeModeSwitchesTheBuiltInSkins) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 90;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  EXPECT_EQ(window->themeMode(), Window::ThemeMode::Dark);
  EXPECT_EQ(bytes(window->theme().background), bytes(Theme::dark().background));

  auto probe = std::make_shared<ThemeProbeLayer>();
  window->setRoot(probe);
  EXPECT_EQ(bytes(probe->background), bytes(Theme::dark().background));

  window->setThemeMode(Window::ThemeMode::Light);
  EXPECT_EQ(window->themeMode(), Window::ThemeMode::Light);
  EXPECT_EQ(bytes(window->theme().background), bytes(Theme::light().background));
  EXPECT_EQ(bytes(probe->background), bytes(Theme::light().background))
    << "controls repaint through the theme propagation";

  window->setThemeMode(Window::ThemeMode::System);
  EXPECT_EQ(window->themeMode(), Window::ThemeMode::System);
  EXPECT_EQ(bytes(window->theme().background), bytes(Theme::light().background))
    << "the headless platform reports a light preference";

  window->setThemeMode(Window::ThemeMode::Dark);
  EXPECT_EQ(bytes(probe->background), bytes(Theme::dark().background));
  window->refreshThemeMode();
  EXPECT_EQ(bytes(window->theme().background), bytes(Theme::dark().background))
    << "refresh is a no-op outside System mode";
}

} // namespace

TEST(UiStyle, WindowHotSwapsTheStyleSheet) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 90;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  auto checkBox = std::make_shared<CheckBox>("Accent", nullptr);
  checkBox->setStyleName("accent");
  window->setRoot(checkBox);
  EXPECT_FALSE(checkBox->resolvedStyle().has_value()) << "no sheet means nothing to resolve";

  auto first = std::make_shared<StyleSheet>();
  Style firstStyle;
  firstStyle.base.background = Color::MakeRGBA8(200, 10, 10, 255);
  first->set("accent", firstStyle);
  window->setStyleSheet(first);
  ASSERT_TRUE(checkBox->resolvedStyle().has_value());
  EXPECT_EQ(red(checkBox->resolvedStyle()->background), 200);

  auto second = std::make_shared<StyleSheet>();
  Style secondStyle;
  secondStyle.base.background = Color::MakeRGBA8(10, 200, 10, 255);
  second->set("accent", secondStyle);
  window->setStyleSheet(second);
  EXPECT_EQ(red(checkBox->resolvedStyle()->background), 10)
    << "the swapped sheet applies without rebuilding the tree";
  EXPECT_EQ(window->styleSheet(), second);
}

TEST(UiStyle, SdkUmbrellaHeaderCompilesAndMountsATree) {
  // Including <ui/UI.h> above proves the SDK umbrella is self-contained; this case uses the types it
  // exposes so the header cannot rot silently.
  auto root = std::make_shared<Flex>(Axis::Column);
  root->addChild(std::make_shared<Button>("OK", nullptr));
  root->addChild(std::make_shared<CheckBox>("Remember", nullptr));
  EXPECT_EQ(root->children().size(), 2u);
}
} // namespace coral::ui
