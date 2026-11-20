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

#include <ui/Accessibility.h>
#include <ui/AccessibilityAtSpi.h>
#include <ui/Button.h>
#include <ui/ColorButton.h>
#include <ui/ColorInput.h>
#include <ui/ColorLayer.h>
#include <ui/Dialog.h>
#include <ui/DragDrop.h>
#include <ui/ColorPicker.h>
#include <ui/CheckBox.h>
#include <ui/Command.h>
#include <ui/IconButton.h>
#include <ui/Menu.h>
#include <ui/MenuBar.h>
#include <ui/IconTheme.h>
#include <ui/NumberField.h>
#include <ui/Popup.h>
#include <ui/Positioner.h>
#include <ui/ProgressBar.h>
#include <ui/RadioButton.h>
#include <ui/SearchField.h>
#include <ui/SegmentedControl.h>
#include <ui/Slider.h>
#include <ui/Toast.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>
#include <ui/Toggle.h>
#include <ui/Undo.h>
#include <ui/Tooltip.h>
#include <ui/TreeView.h>
#include <ui/Flex.h>
#include <ui/Window.h>

#include <graphics/Bitmap.h>
#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

namespace {

using namespace coral::ui;

std::shared_ptr<const graphics::Font> testFont() {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, 16.0f) : nullptr;
}

PointerEvent pointerAt(const float x, const float y) {
  PointerEvent event;
  event.position = { x, y };
  event.button = PointerButton::Primary;
  return event;
}

KeyEvent key(Key key) {
  KeyEvent event;
  event.key = key;
  return event;
}

KeyEvent controlKey(Key key) {
  KeyEvent event;
  event.key = key;
  event.control = true;
  return event;
}

} // namespace

TEST(UiControls, IconButtonActivatesByPointerAndKeyboard) {
  IconButton button(nullptr, "Play");
  button.setBounds(Rect::MakeWH(32.0f, 32.0f));
  int clicks = 0;
  button.setOnClick([&] { ++clicks; });

  EXPECT_TRUE(button.onPointerDown(pointerAt(16.0f, 16.0f)));
  EXPECT_TRUE(button.onPointerUp(pointerAt(16.0f, 16.0f)));
  EXPECT_EQ(clicks, 1);
  EXPECT_TRUE(button.onKeyDown(key(Key::Enter)));
  EXPECT_EQ(clicks, 2);
  EXPECT_EQ(button.activationCount(), 2);
  EXPECT_EQ(button.accessibleName(), "Play");
}

TEST(UiControls, IconButtonHoverFollowsThePointerInsteadOfSticking) {
  IconButton button(nullptr, "Play");
  button.setBounds(Rect::MakeWH(32.0f, 32.0f));
  EXPECT_FALSE(button.hovered());

  EXPECT_TRUE(button.onPointerDown(pointerAt(16.0f, 16.0f)));
  EXPECT_TRUE(button.hovered());
  EXPECT_TRUE(button.onPointerUp(pointerAt(16.0f, 16.0f)));
  EXPECT_TRUE(button.hovered()) << "the pointer is still over the button";

  button.onPointerMove(pointerAt(40.0f, 40.0f));
  EXPECT_FALSE(button.hovered()) << "moving off the button clears hover";

  button.onPointerMove(pointerAt(8.0f, 8.0f));
  EXPECT_TRUE(button.hovered());
  button.onPointerLeave();
  EXPECT_FALSE(button.hovered());
}

TEST(UiControls, ColorButtonShowsAndActivatesItsColor) {
  ColorButton swatch(Color::MakeRGBA8(12, 34, 56, 255), "Accent");
  swatch.setBounds(Rect::MakeWH(32.0f, 32.0f));
  int clicks = 0;
  swatch.setOnClick([&] { ++clicks; });
  EXPECT_NEAR(swatch.color().red, 12.0f / 255.0f, 1e-5f);
  EXPECT_TRUE(swatch.onPointerDown(pointerAt(16.0f, 16.0f)));
  EXPECT_TRUE(swatch.onPointerUp(pointerAt(16.0f, 16.0f)));
  EXPECT_EQ(clicks, 1);
  EXPECT_EQ(swatch.activationCount(), 1);
}

TEST(UiControls, ThemeExposesElevationTokens) {
  const Theme theme;
  EXPECT_GT(theme.shadow.md.blurRadius, theme.shadow.sm.blurRadius);
  EXPECT_GT(theme.shadow.lg.blurRadius, theme.shadow.md.blurRadius);
  EXPECT_GT(theme.shadow.md.color.alpha, 0.0f);
}

TEST(UiControls, ColorPickerSelectsASwatch) {
  ColorPicker picker({ Color::MakeRGBA8(10, 20, 30, 255),
    Color::MakeRGBA8(40, 50, 60, 255) });
  picker.setBounds(Rect::MakeWH(56.0f, 28.0f));
  picker.measure({ 0.0f, 0.0f });
  int changes = 0;
  Color selected;
  picker.setOnChanged([&](const Color color) {
    ++changes;
    selected = color;
  });
  EXPECT_TRUE(picker.onPointerDown(pointerAt(36.0f, 12.0f)));
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(picker.selected(), 1);
  EXPECT_NEAR(selected.red, 40.0f / 255.0f, 1e-5f);
  EXPECT_NEAR(picker.selectedColor().green, 50.0f / 255.0f, 1e-5f);
}

TEST(UiControls, HsvColorPickerTracksHueAndSaturation) {
  ColorPicker picker;
  picker.setMode(ColorPickerMode::Hsv);
  picker.setBounds(Rect::MakeWH(200.0f, 220.0f));
  picker.measure({ 0.0f, 0.0f });
  int changes = 0;
  picker.setOnChanged([&](const Color) { ++changes; });

  const Rect square = Rect::MakeWH(176.0f, 176.0f);
  ASSERT_TRUE(picker.onPointerDown(pointerAt(
    square.getLeft() + square.getWidth() * 0.8f,
    square.getTop() + square.getHeight() * 0.2f)));
  EXPECT_GT(changes, 0);
  const Color warm = picker.color();
  EXPECT_GT(warm.red, 0.0f);

  const Rect hue = Rect::MakeXYWH(0.0f, 186.0f, 176.0f, 14.0f);
  ASSERT_TRUE(picker.onPointerDown(pointerAt(hue.getLeft() + hue.getWidth() * 0.55f,
    hue.getCenter().y)));
  EXPECT_GT(changes, 1);
  const Color cool = picker.color();
  EXPECT_NE(warm.blue, cool.blue);

  const Rect alpha = Rect::MakeXYWH(0.0f, 206.0f, 176.0f, 14.0f);
  ASSERT_TRUE(picker.onPointerDown(pointerAt(alpha.getLeft() + alpha.getWidth() * 0.25f,
    alpha.getCenter().y)));
  EXPECT_NEAR(picker.alpha(), 0.25f, 0.01f);
  EXPECT_NEAR(picker.color().alpha, 0.25f, 0.01f);
}

TEST(UiControls, IconButtonSizeVariantsChangeTheMeasuredSide) {
  IconButton compact(nullptr, "Comp", IconButtonSize::Compact);
  IconButton regular(nullptr, "Reg");
  IconButton large(nullptr, "Large", IconButtonSize::Large);
  EXPECT_FLOAT_EQ(compact.measure({ 0.0f, 0.0f }).width, 24.0f);
  EXPECT_FLOAT_EQ(regular.measure({ 0.0f, 0.0f }).width, 32.0f);
  EXPECT_FLOAT_EQ(large.measure({ 0.0f, 0.0f }).width, 40.0f);
  compact.setIconSize(12.0f);
  compact.setVariant(IconButtonVariant::Ghost);
}

TEST(UiControls, AnyLayerCanCarryAGenericShadow) {
  Layer layer;
  layer.setBounds(Rect::MakeWH(40.0f, 30.0f));
  EXPECT_FALSE(layer.hasShadow());
  Shadow shadow;
  shadow.blurRadius = 20.0f;
  layer.setShadow(shadow, 6.0f);
  EXPECT_TRUE(layer.hasShadow());
  EXPECT_FLOAT_EQ(layer.shadow().blurRadius, 20.0f);
  layer.clearShadow();
  EXPECT_FALSE(layer.hasShadow());
}

TEST(UiControls, ColorButtonOpensAndClosesItsPickerPopup) {
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 200.0f));
  auto swatch = std::make_shared<ColorButton>(Color::MakeRGBA8(1, 2, 3, 255), "Accent");
  swatch->setBounds(Rect::MakeXYWH(20.0f, 20.0f, 32.0f, 32.0f));
  root->addChild(swatch);
  swatch->setPickerEnabled(true);
  ASSERT_NE(swatch->picker(), nullptr);
  ASSERT_NE(swatch->popup(), nullptr);

  EXPECT_TRUE(swatch->onPointerDown(pointerAt(30.0f, 30.0f)));
  EXPECT_TRUE(swatch->onPointerUp(pointerAt(30.0f, 30.0f)));
  EXPECT_TRUE(swatch->popup()->isOpen());
  EXPECT_EQ(swatch->popup()->parent(), root.get());

  EXPECT_TRUE(swatch->onPointerDown(pointerAt(30.0f, 30.0f)));
  EXPECT_TRUE(swatch->onPointerUp(pointerAt(30.0f, 30.0f)));
  EXPECT_FALSE(swatch->popup()->isOpen());
}

TEST(UiControls, ColorInputParsesHexAndRgbFields) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  ColorInput input(font, Color::MakeRGBA8(255, 0, 0, 255));
  input.setBounds(Rect::MakeWH(256.0f, 28.0f));
  input.measure({ 0.0f, 0.0f });
  input.layout();
  Color changed;
  input.setOnChanged([&](const Color color) { changed = color; });

  ASSERT_EQ(input.children().size(), 4u);
  auto hex = std::dynamic_pointer_cast<TextInput>(input.children()[0]);
  auto red = std::dynamic_pointer_cast<TextInput>(input.children()[1]);
  auto green = std::dynamic_pointer_cast<TextInput>(input.children()[2]);
  auto blue = std::dynamic_pointer_cast<TextInput>(input.children()[3]);
  ASSERT_NE(hex, nullptr);
  ASSERT_NE(red, nullptr);

  hex->setText("#00FF00");
  EXPECT_NEAR(input.color().green, 1.0f, 1e-5f);
  EXPECT_NEAR(changed.green, 1.0f, 1e-5f);

  red->setText("0");
  green->setText("0");
  blue->setText("255");
  EXPECT_NEAR(input.color().blue, 1.0f, 1e-5f);
}

TEST(UiControls, IconThemeChoosesTheClosestResolution) {
  auto makeImage = [](const int side) {
    graphics::Bitmap bitmap = graphics::Bitmap::allocate(side, side);
    return std::make_shared<const Image>(graphics::Image::fromBitmap(bitmap));
  };
  IconTheme theme;
  theme.add("play", makeImage(16), 1.0f);
  theme.add("play", makeImage(32), 1.0f);
  ASSERT_TRUE(theme.contains("play"));
  const Image* small = theme.resolve("play", 10.0f);
  const Image* large = theme.resolve("play", 24.0f);
  ASSERT_NE(small, nullptr);
  ASSERT_NE(large, nullptr);
  EXPECT_EQ(small->width(), 16);
  EXPECT_EQ(large->width(), 32);
  EXPECT_EQ(theme.resolve("missing", 16.0f), nullptr);
}

TEST(UiControls, ZOrderControlsHitTestingWithoutReorderingChildren) {  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto lower = std::make_shared<Layer>();
  auto upper = std::make_shared<Layer>();
  lower->setBounds(Rect::MakeWH(100.0f, 100.0f));
  upper->setBounds(Rect::MakeWH(100.0f, 100.0f));
  lower->setZIndex(0);
  upper->setZIndex(1);
  root->addChild(lower);
  root->addChild(upper);

  EXPECT_EQ(root->hitTest({ 50.0f, 50.0f }), upper.get());
  EXPECT_EQ(root->children().front(), lower);
  lower->setZIndex(2);
  EXPECT_EQ(root->hitTest({ 50.0f, 50.0f }), lower.get());

  upper->setHitTestable(false);
  EXPECT_EQ(root->hitTest({ 50.0f, 50.0f }), lower.get());
  lower->setHitTestable(false);
  EXPECT_EQ(root->hitTest({ 50.0f, 50.0f }), root.get());
}

TEST(UiControls, CheckBoxTogglesByClickAndKeyboard) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  CheckBox check("Enabled", font);
  check.setBounds(Rect::MakeWH(180.0f, 28.0f));
  bool reported = false;
  check.setOnChanged([&reported](const bool value) { reported = value; });

  check.onPointerDown(pointerAt(20.0f, 14.0f));
  check.onPointerUp(pointerAt(20.0f, 14.0f));
  EXPECT_TRUE(check.checked());
  EXPECT_TRUE(reported);
  EXPECT_EQ(check.activationCount(), 1);

  KeyEvent repeat;
  repeat.key = Key::Space;
  repeat.repeat = true;
  check.onKeyDown(repeat);
  EXPECT_TRUE(check.checked());
  check.onKeyDown(key(Key::Space));
  EXPECT_FALSE(check.checked());
}

TEST(UiControls, ToggleAndRadioUseTheSameActivationContract) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Toggle toggle("Enabled", font);
  toggle.setBounds(Rect::MakeWH(180.0f, 28.0f));
  toggle.onPointerDown(pointerAt(10.0f, 10.0f));
  toggle.onPointerUp(pointerAt(10.0f, 10.0f));
  EXPECT_TRUE(toggle.on());

  RadioButton radio("Choice", font);
  radio.setBounds(Rect::MakeWH(180.0f, 28.0f));
  int selected = 0;
  radio.setOnSelected([&selected] { ++selected; });
  radio.onKeyDown(key(Key::Enter));
  EXPECT_TRUE(radio.selected());
  EXPECT_EQ(selected, 1);
  radio.onKeyDown(key(Key::Enter));
  EXPECT_EQ(selected, 1);
}

TEST(UiControls, SegmentedControlSelectsByPositionAndArrowKey) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  SegmentedControl control({ "One", "Two", "Three" }, font);
  control.setBounds(Rect::MakeWH(300.0f, 30.0f));
  int selected = 0;
  control.setOnChanged([&selected](const int index) { selected = index; });

  control.onPointerDown(pointerAt(70.0f, 15.0f));
  EXPECT_EQ(control.selectedIndex(), 1);
  EXPECT_EQ(selected, 1);
  control.onKeyDown(key(Key::Right));
  EXPECT_EQ(control.selectedIndex(), 2);
  control.onKeyDown(key(Key::Right));
  EXPECT_EQ(control.selectedIndex(), 0);
  control.onKeyDown(key(Key::Left));
  EXPECT_EQ(control.selectedIndex(), 2);
}

TEST(UiControls, SliderBracketsADragAndStepsWithTheKeyboard) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Slider slider("Opacity", font, 0.0, 1.0, 0.5);
  slider.setBounds(Rect::MakeWH(260.0f, 40.0f));
  slider.setStep(0.1);
  double changed = 0.0;
  int begins = 0;
  int ends = 0;
  slider.setOnChanged([&changed](const double value) { changed = value; });
  slider.setOnEditBegin([&begins] { ++begins; });
  slider.setOnEditEnd([&ends] { ++ends; });

  slider.onKeyDown(key(Key::Right));
  EXPECT_NEAR(slider.value(), 0.6, 1e-9);
  EXPECT_NEAR(changed, 0.6, 1e-9);

  slider.onPointerDown(pointerAt(100.0f, 20.0f));
  slider.onPointerMove(pointerAt(180.0f, 20.0f));
  slider.onPointerUp(pointerAt(180.0f, 20.0f));
  EXPECT_EQ(begins, 1);
  EXPECT_EQ(ends, 1);
  EXPECT_GT(slider.value(), 0.6);
}

TEST(UiControls, NumberFieldStepsAndCommitsTypedValues) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  NumberField field(font, 0.0, 10.0, 2.5);
  field.setBounds(Rect::MakeWH(180.0f, 32.0f));
  field.setStep(0.5);
  double changed = -1.0;
  field.setOnChanged([&changed](const double value) { changed = value; });
  field.onFocusChanged(true);

  field.onKeyDown(key(Key::Up));
  EXPECT_DOUBLE_EQ(field.value(), 3.0);
  EXPECT_DOUBLE_EQ(changed, 3.0);

  field.onKeyDown(controlKey(Key::A));
  field.onTextInput(U'4');
  field.onKeyDown(key(Key::Enter));
  EXPECT_DOUBLE_EQ(field.value(), 4.0);
  EXPECT_DOUBLE_EQ(changed, 4.0);

  field.onKeyDown(controlKey(Key::A));
  field.onTextInput(U'9');
  field.onKeyDown(key(Key::Enter));
  EXPECT_DOUBLE_EQ(field.value(), 9.0);
  field.setRange(0.0, 5.0);
  EXPECT_DOUBLE_EQ(field.value(), 5.0);
}

TEST(UiControls, SpinBoxStepsFromItsRightEdge) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  SpinBox spin(font, 0.0, 10.0, 5.0);
  spin.setBounds(Rect::MakeWH(180.0f, 32.0f));
  spin.setStep(1.0);
  spin.onPointerDown(pointerAt(170.0f, 6.0f));
  EXPECT_DOUBLE_EQ(spin.value(), 6.0);
  spin.onPointerDown(pointerAt(170.0f, 26.0f));
  EXPECT_DOUBLE_EQ(spin.value(), 5.0);
}

TEST(UiControls, SearchFieldReportsEditsSearchesAndClears) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  SearchField search("Search", font);
  search.setBounds(Rect::MakeWH(220.0f, 32.0f));
  std::string changed;
  std::string submitted;
  search.setOnChanged([&changed](const std::string& value) { changed = value; });
  search.setOnSearch([&submitted](const std::string& value) { submitted = value; });

  search.onTextInput(U'a');
  EXPECT_EQ(search.text(), "a");
  EXPECT_EQ(changed, "a");
  search.onKeyDown(key(Key::Enter));
  EXPECT_EQ(submitted, "a");

  search.onPointerDown(pointerAt(214.0f, 16.0f));
  EXPECT_TRUE(search.text().empty());
  EXPECT_TRUE(changed.empty());
}

TEST(UiControls, ToastHidesWhenItsDeadlinePasses) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Toast toast("Saved", font);
  int dismissed = 0;
  toast.setOnDismissed([&dismissed] { ++dismissed; });
  const auto start = std::chrono::steady_clock::now();
  toast.showFor("Saved", std::chrono::milliseconds(20));
  EXPECT_TRUE(toast.isOpen());
  toast.onFrame(start);
  EXPECT_TRUE(toast.isOpen());
  toast.onFrame(start + std::chrono::milliseconds(30));
  EXPECT_FALSE(toast.isOpen());
  EXPECT_EQ(dismissed, 1);
}

TEST(UiControls, ProgressBarClampsAndTooltipAnchors) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  ProgressBar progress(font);
  progress.setValue(1.5);
  EXPECT_DOUBLE_EQ(progress.value(), 1.0);
  progress.setValue(-1.0);
  EXPECT_DOUBLE_EQ(progress.value(), 0.0);

  Tooltip tooltip("Tip", font);
  tooltip.showAt({ 40.0f, 50.0f });
  EXPECT_TRUE(tooltip.isOpen());
  EXPECT_GT(tooltip.bounds().getWidth(), 0.0f);
  EXPECT_FALSE(tooltip.hitTestable());
  tooltip.hide();
  EXPECT_FALSE(tooltip.isOpen());
}

TEST(UiControls, ThemeTokensReachAlreadyBuiltAndLateControls) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 320;
  options.height = 200;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<Layer>();
  auto check = std::make_shared<CheckBox>("Enabled", font);
  root->addChild(check);
  window->setRoot(root);
  const float narrowGapWidth = check->measure({}).width;
  EXPECT_NE(check->theme(), nullptr);

  Theme theme = Theme::dark();
  theme.spacing.sm = 20.0f;
  theme.radius.md = 9.0f;
  window->setTheme(theme);
  const float wideGapWidth = check->measure({}).width;

  EXPECT_GT(wideGapWidth, narrowGapWidth);
  EXPECT_EQ(check->theme(), root->theme());

  auto late = std::make_shared<CheckBox>("Late", font);
  root->addChild(late);
  EXPECT_EQ(late->theme(), root->theme());
}

TEST(UiControls, AWindowSwitchesThemeAndRepaintsControls) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 120;
  options.height = 80;
  options.frameLimit = 0;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto button = std::make_shared<Button>("", font);
  button->setBounds(Rect::MakeWH(120.0f, 80.0f));
  window->setRoot(button);
  window->setTheme(Theme::light());
  ASSERT_TRUE(window->renderFrame());

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  const auto* center = static_cast<const uint8_t*>(pixmap.pixels()) +
    (40u * 120u + 60u) * 4u;
  const uint8_t* pixel = center;
  const auto light = Theme::light();
  EXPECT_EQ(pixel[0], static_cast<uint8_t>(light.controls.background.red * 255.0f + 0.5f));
  EXPECT_EQ(pixel[1], static_cast<uint8_t>(light.controls.background.green * 255.0f + 0.5f));
  EXPECT_EQ(pixel[2], static_cast<uint8_t>(light.controls.background.blue * 255.0f + 0.5f));

  window->setTheme(Theme::dark());
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap darkPixmap = bitmap.makePixmap();
  ASSERT_TRUE(darkPixmap.isValid());
  const uint8_t* darkPixel = static_cast<const uint8_t*>(darkPixmap.pixels()) +
    (40u * 120u + 60u) * 4u;
  const auto dark = Theme::dark();
  EXPECT_EQ(darkPixel[0], static_cast<uint8_t>(dark.controls.background.red * 255.0f + 0.5f));
  EXPECT_EQ(darkPixel[1], static_cast<uint8_t>(dark.controls.background.green * 255.0f + 0.5f));
  EXPECT_EQ(darkPixel[2], static_cast<uint8_t>(dark.controls.background.blue * 255.0f + 0.5f));
}

TEST(UiControls, TreeViewExpandsSelectsAndActivates) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  TreeView tree(font);
  const int root = tree.addRoot("Media");
  const int folder = tree.addChild(root, "Clips", true);
  const int clip = tree.addChild(folder, "intro.mp4");
  tree.setExpanded(folder, true);
  EXPECT_EQ(tree.visibleCount(), 3u);
  tree.setExpanded(root, false);
  EXPECT_EQ(tree.visibleCount(), 1u);
  tree.setExpanded(root, true);
  EXPECT_EQ(tree.visibleCount(), 3u);
  tree.setSelected(clip);
  EXPECT_EQ(tree.selected(), clip);
  EXPECT_EQ(tree.label(clip), "intro.mp4");
  EXPECT_TRUE(tree.isFolder(folder));
  EXPECT_FALSE(tree.isFolder(clip));
}

TEST(UiControls, TreeViewVirtualizesLargeTrees) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  TreeView tree(font);
  const int root = tree.addRoot("Media");
  for (int index = 0; index < 5000; ++index) {
    tree.addChild(root, "clip-" + std::to_string(index) + ".mp4");
  }
  EXPECT_EQ(tree.visibleCount(), 5001u);
  EXPECT_EQ(tree.content()->children().size(), 0u)
    << "the whole tree is drawn by one virtual content layer";
  tree.setBounds(Rect::MakeWH(300.0f, 240.0f));
  tree.measure({ 300.0f, 240.0f });
  tree.layout();
  EXPECT_GT(tree.content()->measuredSize().height, tree.bounds().getHeight());
}

TEST(UiControls, AccessibilityTreeExposesRolesAndNames) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto root = std::make_shared<Layer>();
  root->setAccessibleName("Editor");
  root->setAccessibleRole(AccessibilityRole::Group);
  auto button = std::make_shared<Button>("Run", font);
  auto field = std::make_shared<TextInput>("", font);
  field->setPlaceholder("Search media");
  auto label = std::make_shared<TextLayer>("Timeline", font, graphics::Color::White());
  root->addChild(button);
  root->addChild(field);
  root->addChild(label);
  const AccessibilityNode tree = accessibilityTree(*root);
  EXPECT_EQ(tree.name, "Editor");
  ASSERT_EQ(tree.children.size(), 3u);
  EXPECT_EQ(tree.children[0].role, AccessibilityRole::Button);
  EXPECT_EQ(tree.children[0].name, "Run");
  EXPECT_EQ(tree.children[1].role, AccessibilityRole::TextField);
  EXPECT_EQ(tree.children[1].name, "Search media");
  EXPECT_EQ(tree.children[2].role, AccessibilityRole::Label);
  EXPECT_EQ(tree.children[2].name, "Timeline");
}

TEST(UiControls, AccessibilityNodesCarryValueStateAndActions) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto root = std::make_shared<Layer>();
  root->setAccessibleRole(AccessibilityRole::Group);
  auto button = std::make_shared<Button>("Run", font);
  auto check = std::make_shared<CheckBox>("Loop", font, true);
  auto field = std::make_shared<TextInput>("", font);
  field->setText("media.mov");
  auto slider = std::make_shared<Slider>("Opacity", font, 0.0, 1.0, 0.5);
  root->addChild(button);
  root->addChild(check);
  root->addChild(field);
  root->addChild(slider);
  const AccessibilityNode tree = accessibilityTree(*root);
  ASSERT_EQ(tree.children.size(), 4u);

  // A button exposes Invoke and has no value.
  EXPECT_TRUE(hasAccessibilityAction(tree.children[0].actions, AccessibilityAction::Invoke));
  EXPECT_FALSE(hasAccessibilityAction(tree.children[0].actions, AccessibilityAction::SetValue));
  EXPECT_TRUE(tree.children[0].value.empty());

  // A checkbox exposes Toggle and reports its selected state.
  EXPECT_TRUE(hasAccessibilityAction(tree.children[1].actions, AccessibilityAction::Toggle));
  EXPECT_TRUE(tree.children[1].selected);
  check->setChecked(false);
  EXPECT_FALSE(accessibilityTree(*root).children[1].selected) << "the snapshot follows the state";

  // A text field is editable and carries its contents.
  EXPECT_TRUE(tree.children[2].editable);
  EXPECT_TRUE(hasAccessibilityAction(tree.children[2].actions, AccessibilityAction::SetValue));
  EXPECT_EQ(tree.children[2].value, "media.mov");

  // A slider exposes SetValue and reports its reading.
  EXPECT_TRUE(hasAccessibilityAction(tree.children[3].actions, AccessibilityAction::SetValue));
  EXPECT_EQ(tree.children[3].value, "0.50");
}

TEST(UiControls, AccessibilityInvokeReachesTheControl) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto root = std::make_shared<Layer>();
  root->setAccessibleRole(AccessibilityRole::Group);
  auto button = std::make_shared<Button>("Run", font);
  int clicks = 0;
  button->setOnClick([&clicks] { ++clicks; });
  root->addChild(button);

  const AccessibilityNode tree = accessibilityTree(*root);
  ASSERT_EQ(tree.children.size(), 1u);
  const AccessibilityNode& node = tree.children[0];
  EXPECT_TRUE(hasAccessibilityAction(node.actions, AccessibilityAction::Invoke));
  EXPECT_FALSE(node.path.empty()) << "a node carries the path a bridge addresses it by";

  // The bridge resolves the path against the live tree and asks the control to act; the control, not
  // the bridge, decides what Invoke means.
  Layer* resolved = accessibilityLayerAtPath(*root, node.path);
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved, button.get());
  EXPECT_TRUE(resolved->performAccessibilityAction(AccessibilityAction::Invoke, {}));
  EXPECT_EQ(clicks, 1);
  // A container has no Invoke and refuses instead of pretending.
  root->setAccessibleRole(AccessibilityRole::Generic);
  EXPECT_FALSE(root->performAccessibilityAction(AccessibilityAction::Invoke, {}));
}

TEST(UiControls, AccessibilityToggleAndValueReachTheControls) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto root = std::make_shared<Layer>();
  root->setAccessibleRole(AccessibilityRole::Group);
  auto check = std::make_shared<CheckBox>("Loop", font, false);
  auto field = std::make_shared<TextInput>("", font);
  auto slider = std::make_shared<Slider>("Opacity", font, 0.0, 1.0, 0.25);
  root->addChild(check);
  root->addChild(field);
  root->addChild(slider);
  const AccessibilityNode tree = accessibilityTree(*root);
  ASSERT_EQ(tree.children.size(), 3u);

  Layer* checkLayer = accessibilityLayerAtPath(*root, tree.children[0].path);
  ASSERT_NE(checkLayer, nullptr);
  EXPECT_TRUE(checkLayer->performAccessibilityAction(AccessibilityAction::Toggle, {}));
  EXPECT_TRUE(check->checked());
  EXPECT_FALSE(tree.children[2].editable) << "a slider is not a text field";

  Layer* fieldLayer = accessibilityLayerAtPath(*root, tree.children[1].path);
  ASSERT_NE(fieldLayer, nullptr);
  EXPECT_TRUE(fieldLayer->performAccessibilityAction(AccessibilityAction::SetValue, "media.mov"));
  EXPECT_EQ(field->text(), "media.mov");
  EXPECT_EQ(accessibilityTree(*root).children[1].value, "media.mov") << "the snapshot follows";

  Layer* sliderLayer = accessibilityLayerAtPath(*root, tree.children[2].path);
  ASSERT_NE(sliderLayer, nullptr);
  EXPECT_TRUE(sliderLayer->performAccessibilityAction(AccessibilityAction::SetValue, "0.75"));
  EXPECT_DOUBLE_EQ(slider->value(), 0.75);
  EXPECT_FALSE(sliderLayer->performAccessibilityAction(AccessibilityAction::SetValue, "nope"))
    << "an unparsable value is refused";
}

TEST(UiControls, StateChangesNotifyTheAccessibilityBridge) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto check = std::make_shared<CheckBox>("Loop", font, false);
  auto slider = std::make_shared<Slider>("Opacity", font, 0.0, 1.0, 0.25);
  // Events are routed through the layer's window, so the controls have to live in one.
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  auto root = std::make_shared<Flex>(Axis::Column);
  root->addChild(slider);
  root->addChild(check);
  window->setRoot(root);
  std::vector<AccessibilityEvent> events;
  setAccessibilityNotifier(window.get(), [&events](Layer&, const AccessibilityEvent event) {
    events.push_back(event);
  });
  slider->setValue(0.75);
  check->setChecked(true);
  setAccessibilityNotifier(window.get(), {});
  ASSERT_EQ(events.size(), 2u) << "construction does not notify, changes do";
  EXPECT_EQ(events[0], AccessibilityEvent::ValueChanged);
  EXPECT_EQ(events[1], AccessibilityEvent::ToggleChanged);
  slider->setValue(0.8);
  EXPECT_EQ(events.size(), 2u) << "without a notifier nothing is queued";
}

TEST(UiControls, AccessibilitySelectionItemSelectsTheRow) {
  auto root = std::make_shared<Layer>();
  root->setAccessibleRole(AccessibilityRole::List);
  auto first = std::make_shared<Layer>();
  first->setAccessibleRole(AccessibilityRole::ListItem);
  first->setAccessibleName("Intro");
  auto second = std::make_shared<Layer>();
  second->setAccessibleRole(AccessibilityRole::ListItem);
  second->setAccessibleName("B-roll");
  int selectedIndex = -1;
  first->setAccessibleOnSelect([&selectedIndex, first, second] {
    selectedIndex = 0;
    first->setAccessibleSelected(true);
    second->setAccessibleSelected(false);
  });
  second->setAccessibleOnSelect([&selectedIndex, first, second] {
    selectedIndex = 1;
    first->setAccessibleSelected(false);
    second->setAccessibleSelected(true);
  });
  root->addChild(first);
  root->addChild(second);

  const AccessibilityNode tree = accessibilityTree(*root);
  ASSERT_EQ(tree.children.size(), 2u);
  EXPECT_TRUE(hasAccessibilityAction(tree.children[0].actions, AccessibilityAction::Select));
  EXPECT_FALSE(tree.children[0].selected) << "nothing is selected yet";

  Layer* row = accessibilityLayerAtPath(*root, tree.children[1].path);
  ASSERT_NE(row, nullptr);
  EXPECT_TRUE(row->performAccessibilityAction(AccessibilityAction::Select, {}));
  EXPECT_EQ(selectedIndex, 1);
  const AccessibilityNode after = accessibilityTree(*root);
  EXPECT_TRUE(after.children[1].selected);
  EXPECT_FALSE(after.children[0].selected);
  EXPECT_FALSE(row->performAccessibilityAction(AccessibilityAction::SetValue, "x"))
    << "a row only answers Select";
}

TEST(UiControls, AtspiRoleAndInterfaceMapping) {
  using namespace coral::ui;
  EXPECT_STREQ(atspiRoleName(AccessibilityRole::Button), "push button");
  EXPECT_STREQ(atspiRoleName(AccessibilityRole::CheckBox), "check box");
  EXPECT_STREQ(atspiRoleName(AccessibilityRole::Slider), "slider");
  EXPECT_STREQ(atspiRoleName(AccessibilityRole::TextField), "text");
  EXPECT_STREQ(atspiRoleName(AccessibilityRole::ListItem), "list item");

  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto button = std::make_shared<Button>("Run", font);
  auto check = std::make_shared<CheckBox>("Loop", font, false);
  auto field = std::make_shared<TextInput>("", font);
  field->setText("clip.mov");
  auto slider = std::make_shared<Slider>("Opacity", font, 0.0, 1.0, 0.5);
  auto root = std::make_shared<Layer>();
  root->addChild(button);
  root->addChild(check);
  root->addChild(field);
  root->addChild(slider);
  const AccessibilityNode tree = accessibilityTree(*root);
  ASSERT_EQ(tree.children.size(), 4u);

  const auto has = [](const std::vector<std::string>& interfaces, const std::string& name) {
    return std::find(interfaces.begin(), interfaces.end(), name) != interfaces.end();
  };
  const auto buttonInterfaces = atspiInterfaces(tree.children[0]);
  EXPECT_TRUE(has(buttonInterfaces, "org.a11y.atspi.Action"));
  EXPECT_FALSE(has(buttonInterfaces, "org.a11y.atspi.Value"));
  EXPECT_TRUE(has(atspiInterfaces(tree.children[1]), "org.a11y.atspi.Action"));
  const auto fieldInterfaces = atspiInterfaces(tree.children[2]);
  EXPECT_TRUE(has(fieldInterfaces, "org.a11y.atspi.Value"));
  EXPECT_TRUE(has(fieldInterfaces, "org.a11y.atspi.EditableText"));
  EXPECT_TRUE(has(fieldInterfaces, "org.a11y.atspi.Text"));
  EXPECT_TRUE(has(atspiInterfaces(tree.children[3]), "org.a11y.atspi.Value"));
  EXPECT_EQ(atspiName(tree.children[2]), "clip.mov") << "an unnamed field reports its value";
  EXPECT_EQ(atspiName(AccessibilityNode{}), "unknown");
}

TEST(UiControls, ControlsDrawIntoAHeadlessFrame) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  Window::Options options;
  options.headless = true;
  options.width = 480;
  options.height = 320;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<Layer>();
  auto check = std::make_shared<CheckBox>("Enabled", font, true);
  auto toggle = std::make_shared<Toggle>("Toggle", font, true);
  auto radio = std::make_shared<RadioButton>("Choice", font, true);
  auto segments = std::make_shared<SegmentedControl>(
    std::vector<std::string>{ "One", "Two", "Three" }, font);
  auto slider = std::make_shared<Slider>("Opacity", font, 0.0, 1.0, 0.5);
  auto progress = std::make_shared<ProgressBar>(font, 0.6);
  auto number = std::make_shared<NumberField>(font, 0.0, 100.0, 42.0);
  auto search = std::make_shared<SearchField>("Search", font);
  number->setBounds(Rect::MakeXYWH(10.0f, 228.0f, 180.0f, 32.0f));
  search->setBounds(Rect::MakeXYWH(200.0f, 228.0f, 220.0f, 32.0f));
  check->setBounds(Rect::MakeXYWH(10.0f, 10.0f, 180.0f, 28.0f));
  toggle->setBounds(Rect::MakeXYWH(10.0f, 44.0f, 180.0f, 28.0f));
  radio->setBounds(Rect::MakeXYWH(10.0f, 78.0f, 180.0f, 28.0f));
  segments->setBounds(Rect::MakeXYWH(10.0f, 112.0f, 260.0f, 30.0f));
  slider->setBounds(Rect::MakeXYWH(10.0f, 150.0f, 260.0f, 40.0f));
  progress->setBounds(Rect::MakeXYWH(10.0f, 198.0f, 260.0f, 22.0f));
  root->addChild(check);
  root->addChild(toggle);
  root->addChild(radio);
  root->addChild(segments);
  root->addChild(slider);
  root->addChild(progress);
  root->addChild(number);
  root->addChild(search);

  auto tooltip = std::make_shared<Tooltip>("Tooltip", font);
  tooltip->showAt({ 300.0f, 80.0f });
  root->addChild(tooltip);
  auto popup = std::make_shared<Popup>(std::make_shared<Layer>());
  popup->showAt({ 300.0f, 150.0f });
  root->addChild(popup);
  auto toast = std::make_shared<Toast>("Saved", font);
  toast->showAt({ 10.0f, 270.0f });
  root->addChild(toast);

  window->setRoot(root);
  EXPECT_EQ(window->run(), 1);
  graphics::Bitmap bitmap;
  EXPECT_TRUE(window->snapshot(&bitmap));
  EXPECT_EQ(bitmap.info().width(), 480);
}

TEST(UiControls, PopupLaysOutItsContentInsideItsPadding) {
  auto content = std::make_shared<Layer>();
  content->setBounds(Rect::MakeWH(80.0f, 30.0f));
  Popup popup(content);
  popup.showAt({ 20.0f, 30.0f });
  EXPECT_TRUE(popup.isOpen());
  EXPECT_EQ(content->parent(), &popup);
  EXPECT_GT(popup.bounds().getWidth(), content->bounds().getWidth());
  EXPECT_FALSE(popup.content()->bounds().isEmpty());
}

TEST(UiControls, MenuCommandItemsFollowTheCommandRegistry) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  CommandRegistry commands;
  bool fired = false;
  bool sawChecked = false;
  commands.bind("view.grid", [&] {
    fired = true;
    sawChecked = commands.isChecked("view.grid");
  });

  Menu menu(font);
  menu.setCommands(&commands);
  menu.addCommandCheckItem("Show grid", "view.grid");
  menu.showAt(Point{ 0.0f, 0.0f });

  const Rect panel = menu.panelBounds();
  ASSERT_GT(panel.getWidth(), 0.0f);
  float y = panel.getTop() + 2.0f;
  int index = -1;
  for (; y < panel.getBottom(); y += 1.0f) {
    index = menu.itemAt(Point{ panel.getLeft() + 10.0f, y });
    if (index == 0) break;
  }
  ASSERT_EQ(index, 0) << "the first command item must be hittable";
  PointerEvent down = pointerAt(panel.getLeft() + 10.0f, y);
  EXPECT_TRUE(menu.onPointerDown(down));
  EXPECT_TRUE(fired);
  EXPECT_TRUE(sawChecked) << "the registry toggles before the handler runs";
  EXPECT_TRUE(commands.isChecked("view.grid"));
  EXPECT_FALSE(menu.isOpen());

  // A disabled command greys out and does nothing, and the menu stays open for another choice.
  commands.setEnabled("view.grid", false);
  fired = false;
  menu.refreshCommands();
  menu.showAt(Point{ 0.0f, 0.0f });
  y = menu.panelBounds().getTop() + 2.0f;
  for (; y < menu.panelBounds().getBottom(); y += 1.0f) {
    if (menu.itemAt(Point{ menu.panelBounds().getLeft() + 10.0f, y }) == 0) break;
  }
  EXPECT_TRUE(menu.onPointerDown(pointerAt(menu.panelBounds().getLeft() + 10.0f, y)));
  EXPECT_FALSE(fired) << "a disabled command item must not run";
  EXPECT_TRUE(menu.isOpen());
}

TEST(UiControls, OpenMenuFollowsCommandStateChangesWithoutPolling) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  CommandRegistry commands;
  bool fired = false;
  commands.bind("view.grid", [&] { fired = true; });

  Menu menu(font);
  menu.setCommands(&commands);
  menu.addCommandCheckItem("Show grid", "view.grid");
  menu.showAt(Point{ 0.0f, 0.0f });

  // No explicit refreshCommands(): the registry notification reaches the open menu.
  commands.setChecked("view.grid", true);
  EXPECT_TRUE(menu.isItemChecked(0)) << "an open menu follows the registry without polling";
  commands.setChecked("view.grid", false);
  EXPECT_FALSE(menu.isItemChecked(0));

  // A command disabled while the menu is open greys out before the next click.
  commands.setEnabled("view.grid", false);
  const Rect panel = menu.panelBounds();
  float y = panel.getTop() + 2.0f;
  for (; y < panel.getBottom(); y += 1.0f) {
    if (menu.itemAt(Point{ panel.getLeft() + 10.0f, y }) == 0) break;
  }
  EXPECT_TRUE(menu.onPointerDown(pointerAt(panel.getLeft() + 10.0f, y)));
  EXPECT_FALSE(fired) << "the refresh disabled the item before the click";
  EXPECT_TRUE(menu.isOpen());

  // Two bound menus each get their own listener; the older single-callback entry point does not.
  Menu second(font);
  second.setCommands(&commands);
  second.addCommandCheckItem("Grid", "view.grid");
  second.showAt(Point{ 0.0f, 0.0f });
  commands.setChecked("view.grid", true);
  EXPECT_TRUE(second.isItemChecked(0)) << "every bound menu follows the same registry";
}

TEST(UiControls, AWindowDestroysItsCommandTableAfterBoundMenus) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  Window::Options options;
  options.headless = true;
  options.width = 320;
  options.height = 200;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->commands().bind("view.grid", [] {});

  auto menu = std::make_shared<Menu>(font);
  menu->setCommands(&window->commands());
  menu->addCommandCheckItem("Show grid", "view.grid");
  window->pushOverlay(menu);
  menu->showAt(Point{ 0.0f, 0.0f });
  window->commands().setChecked("view.grid", true);
  ASSERT_TRUE(menu->isItemChecked(0)) << "an overlay menu follows the window's command table";

  menu.reset();   // the overlay host owns it from here on
  window.reset(); // overlays die first, then the command table those menus were bound to
  // Reaching this line means the menu's listener was removed before the table was destroyed.
}

TEST(UiControls, MenuBarOpensSwitchesAndClosesItsMenus) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(400.0f, 200.0f));
  auto bar = std::make_shared<MenuBar>(font);
  root->addChild(bar);
  bar->setBounds(Rect::MakeWH(400.0f, 26.0f));

  auto file = std::make_shared<Menu>(font);
  file->addItem("New", [] {});
  auto edit = std::make_shared<Menu>(font);
  edit->addItem("Undo", [] {});
  EXPECT_EQ(bar->addMenu("File", file), 0);
  EXPECT_EQ(bar->addMenu("Edit", edit), 1);
  bar->layout();

  const Rect fileLabel = bar->labelBounds(0);
  const Rect editLabel = bar->labelBounds(1);
  ASSERT_GT(fileLabel.getWidth(), 0.0f);
  ASSERT_GT(editLabel.getWidth(), 0.0f);

  EXPECT_TRUE(bar->onPointerDown(pointerAt(fileLabel.getLeft() + 4.0f, fileLabel.getTop() + 10.0f)));
  EXPECT_TRUE(bar->isOpen());
  EXPECT_TRUE(file->isOpen());
  EXPECT_FALSE(edit->isOpen());

  // While a menu is open, hovering the next label switches to it.
  bar->onPointerMove(pointerAt(editLabel.getLeft() + 4.0f, editLabel.getTop() + 10.0f));
  EXPECT_TRUE(edit->isOpen());
  EXPECT_FALSE(file->isOpen());

  // Clicking the open label closes it.
  EXPECT_TRUE(bar->onPointerDown(pointerAt(editLabel.getLeft() + 4.0f, editLabel.getTop() + 10.0f)));
  EXPECT_FALSE(bar->isOpen());
  EXPECT_FALSE(edit->isOpen());
}

TEST(UiControls, PositionerPrefersBelowAndShiftsInsideTheWindow) {
  PlacementRequest request;
  request.anchor = Rect::MakeXYWH(100.0f, 100.0f, 40.0f, 20.0f);
  request.size = Size{ 120.0f, 60.0f };
  request.available = Rect::MakeWH(320.0f, 200.0f);
  request.margin = Insets{ 0.0f, 0.0f, 0.0f, 0.0f };
  const PlacementResult below = placeFloating(request);
  EXPECT_FALSE(below.flipped);
  EXPECT_FALSE(below.shifted);
  EXPECT_EQ(below.placement, Placement::BelowStart);
  EXPECT_EQ(below.bounds.getLeft(), 100.0f);
  EXPECT_EQ(below.bounds.getTop(), 122.0f);

  // Near the right edge the surface slides left instead of leaving the window.
  request.anchor = Rect::MakeXYWH(280.0f, 40.0f, 40.0f, 20.0f);
  const PlacementResult shifted = placeFloating(request);
  EXPECT_TRUE(shifted.shifted);
  EXPECT_EQ(shifted.bounds.getRight(), 320.0f);
}

TEST(UiControls, PositionerFlipsAboveWhenBelowWouldOverflow) {
  PlacementRequest request;
  request.anchor = Rect::MakeXYWH(40.0f, 160.0f, 40.0f, 20.0f);
  request.size = Size{ 120.0f, 80.0f };
  request.available = Rect::MakeWH(320.0f, 200.0f);
  request.margin = Insets{ 0.0f, 0.0f, 0.0f, 0.0f };
  const PlacementResult result = placeFloating(request);
  EXPECT_TRUE(result.flipped);
  EXPECT_EQ(result.placement, Placement::AboveStart);
  EXPECT_EQ(result.bounds.getBottom(), 158.0f) << "the gap sits between the panel and the anchor";
}

TEST(UiControls, PopupFlipsAboveItsAnchorNearTheBottomEdge) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto popup = std::make_shared<Popup>(std::make_shared<TextLayer>("popup", font));
  root->addChild(popup);
  popup->showAt(Rect::MakeXYWH(20.0f, 100.0f, 40.0f, 16.0f));
  const Rect panel = popup->bounds();
  ASSERT_GT(panel.getHeight(), 0.0f);
  EXPECT_LE(panel.getBottom(), 120.0f) << "the popup stays inside the window";
  EXPECT_LT(panel.getBottom(), 100.0f) << "it flipped above the anchor";
}

namespace {

/** A focusable layer that counts pointer presses; stands in for a page control in overlay tests. */
class OverlayProbe final : public Layer {
 public:
  bool onPointerDown(const PointerEvent&) override {
    ++clicks;
    return true;
  }
  [[nodiscard]] bool focusable() const override {
    return true;
  }

  int clicks = 0;
};

KeyEvent keyPress(const Key key) {
  KeyEvent event;
  event.key = key;
  return event;
}

} // namespace

TEST(UiControls, DialogCarriesContentAndReportsItsResult) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  Dialog dialog("Title", font);
  std::vector<int> results;
  dialog.setOnResult([&results](const int result) { results.push_back(result); });
  const int ok = dialog.addButton("OK", [] {});
  const int cancel = dialog.addButton("Cancel", [] {});
  EXPECT_EQ(ok, 0);
  EXPECT_EQ(cancel, 1);
  dialog.setDefaultButton(ok);

  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(12, 34, 56, 255));
  content->setBounds(Rect::MakeWH(120.0f, 60.0f));
  dialog.setContent(content);
  dialog.show();
  dialog.layout();

  const Rect panel = dialog.panelBounds();
  ASSERT_GT(panel.getHeight(), 140.0f) << "the panel grows for the content slot";
  EXPECT_TRUE(panel.contains(Point{ content->bounds().getLeft() + 1.0f,
    content->bounds().getTop() + 1.0f }));

  EXPECT_TRUE(dialog.onKeyDown(keyPress(Key::Enter)));
  EXPECT_FALSE(dialog.isOpen());
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results.back(), 0) << "Enter activates the default button";

  dialog.show();
  EXPECT_TRUE(dialog.onKeyDown(keyPress(Key::Escape)));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results.back(), -1) << "Escape reports the cancel result";
}

TEST(UiControls, ModalOverlayBlocksThePageAndClosesOnEscape) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto page = std::make_shared<OverlayProbe>();
  page->setBounds(Rect::MakeWH(200.0f, 120.0f));
  root->addChild(page);
  window->setRoot(root);

  auto panel = std::make_shared<OverlayProbe>();
  panel->setBounds(Rect::MakeWH(80.0f, 40.0f));
  OverlayOptions overlayOptions;
  overlayOptions.modal = true;
  overlayOptions.dismissOnOutsideClick = false;
  const uint64_t id = window->pushOverlay(panel, overlayOptions);
  ASSERT_NE(id, 0u);
  EXPECT_TRUE(window->hasOverlay());

  window->pointerDown(pointerAt(10.0f, 10.0f));
  window->pointerUp(pointerAt(10.0f, 10.0f));
  EXPECT_EQ(page->clicks, 0) << "the modal overlay must not leak clicks to the page";

  window->keyDown(keyPress(Key::Escape));
  EXPECT_FALSE(window->hasOverlay()) << "Escape closes a dismissible overlay";
}

TEST(UiControls, OverlayRestoresFocusWhenDismissed) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto first = std::make_shared<OverlayProbe>();
  first->setBounds(Rect::MakeWH(40.0f, 20.0f));
  root->addChild(first);
  window->setRoot(root);
  window->focusLayer(first.get());
  ASSERT_EQ(window->focusedLayer(), first.get());

  auto dialog = std::make_shared<OverlayProbe>();
  dialog->setBounds(Rect::MakeWH(80.0f, 40.0f));
  OverlayOptions options2;
  options2.modal = true;
  options2.dismissOnEscape = false;
  window->pushOverlay(dialog, options2);
  EXPECT_EQ(window->focusedLayer(), dialog.get()) << "a modal overlay takes focus";

  window->dismissTopOverlay();
  EXPECT_EQ(window->focusedLayer(), first.get()) << "dismissing restores the interrupted focus";
}

TEST(UiControls, AnchoredOverlayFlipsAboveNearTheBottomEdge) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  window->setRoot(root);

  auto panel = std::make_shared<OverlayProbe>();
  panel->setBounds(Rect::MakeWH(80.0f, 40.0f));
  OverlayOptions overlayOptions;
  overlayOptions.anchor = Rect::MakeXYWH(20.0f, 100.0f, 40.0f, 16.0f);
  overlayOptions.preferred = Placement::BelowStart;
  overlayOptions.gap = 2.0f;
  window->pushOverlay(panel, overlayOptions);
  window->renderFrame();
  window->pump();
  EXPECT_LE(panel->bounds().getBottom(), 120.0f);
  EXPECT_LT(panel->bounds().getBottom(), 100.0f) << "it flipped above the anchor";
}

TEST(UiControls, TooltipShowsAfterItsHoverDelayAndCanBeCancelled) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  Tooltip tooltip("Hint", font);
  const auto start = std::chrono::steady_clock::now();
  tooltip.scheduleShowAt(Point{ 10.0f, 20.0f }, std::chrono::milliseconds(100));
  EXPECT_TRUE(tooltip.scheduled());
  tooltip.onFrame(start + std::chrono::milliseconds(50));
  EXPECT_FALSE(tooltip.isOpen()) << "the delay has not elapsed yet";
  tooltip.onFrame(start + std::chrono::milliseconds(150));
  EXPECT_TRUE(tooltip.isOpen());
  EXPECT_FALSE(tooltip.scheduled());

  tooltip.hide();
  tooltip.scheduleShowAt(Point{ 0.0f, 0.0f }, std::chrono::milliseconds(100));
  tooltip.cancelScheduled();
  tooltip.onFrame(start + std::chrono::seconds(1));
  EXPECT_FALSE(tooltip.isOpen()) << "a cancelled timer must not show the tooltip";
}

TEST(UiControls, TextInputAnchorsImeCandidatesAfterTheComposition) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  TextInput input("ab", font);
  input.setBounds(Rect::MakeWH(200.0f, 32.0f));
  input.onFocusChanged(true);
  input.setCursor(1);
  const float caretWidth = input.imeCursorRect().getWidth();
  ASSERT_GT(caretWidth, 0.0f);

  input.onTextComposition("zhong");
  const float compositionWidth = input.imeCursorRect().getWidth();
  EXPECT_GT(compositionWidth, caretWidth) << "the anchor spans the preedit, not just the caret";
  EXPECT_NEAR(compositionWidth - caretWidth, font->measureWidth("zhong"), 0.5f);

  input.onTextComposition(std::string());
  EXPECT_FLOAT_EQ(input.imeCursorRect().getWidth(), caretWidth);
}

namespace {

/** Records drag routing; returns `effect` from enter/over and accepts drops. */
class DragProbe final : public Layer {
 public:
  DragEffect onDragEnter(const DragEvent& event) override {
    ++enters;
    lastType = event.payload.type();
    return effect;
  }
  DragEffect onDragOver(const DragEvent&) override {
    ++overs;
    return effect;
  }
  void onDragLeave() override {
    ++leaves;
  }
  bool onDrop(const DropEvent& event) override {
    ++drops;
    dropped = event.payload.data();
    dropType = event.payload.type();
    dropEffect = event.effect;
    external = event.external;
    return true;
  }

  DragEffect effect = DragEffect::Copy;
  int enters = 0;
  int overs = 0;
  int leaves = 0;
  int drops = 0;
  bool external = false;
  std::string lastType;
  std::string dropType;
  std::string dropped;
  DragEffect dropEffect = DragEffect::None;
};

} // namespace

TEST(UiControls, InternalDragEntersOverAndDropsOnTheTarget) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto probe = std::make_shared<DragProbe>();
  probe->setBounds(Rect::MakeWH(100.0f, 100.0f));
  root->addChild(probe);
  window->setRoot(root);

  ASSERT_TRUE(window->beginDrag(DragPayload::text("hello")));
  EXPECT_TRUE(window->isDragging());
  ASSERT_NE(window->dragPayload(), nullptr);
  EXPECT_EQ(window->dragPayload()->type(), "text/plain");

  window->pointerMove(pointerAt(10.0f, 10.0f));
  EXPECT_EQ(probe->enters, 1);
  EXPECT_EQ(probe->lastType, "text/plain");
  window->pointerMove(pointerAt(20.0f, 20.0f));
  EXPECT_GE(probe->overs, 1);

  window->pointerUp(pointerAt(20.0f, 20.0f));
  EXPECT_FALSE(window->isDragging());
  EXPECT_EQ(probe->drops, 1);
  EXPECT_EQ(probe->dropped, "hello");
  EXPECT_EQ(probe->dropEffect, DragEffect::Copy);
  EXPECT_FALSE(probe->external);
}

TEST(UiControls, DragLeaveFiresWhenThePointerMovesToAnotherTarget) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto left = std::make_shared<DragProbe>();
  left->setBounds(Rect::MakeXYWH(0.0f, 0.0f, 80.0f, 100.0f));
  auto right = std::make_shared<DragProbe>();
  right->setBounds(Rect::MakeXYWH(100.0f, 0.0f, 80.0f, 100.0f));
  root->addChild(left);
  root->addChild(right);
  window->setRoot(root);

  ASSERT_TRUE(window->beginDrag(DragPayload::text("x")));
  window->pointerMove(pointerAt(10.0f, 10.0f));
  EXPECT_EQ(left->enters, 1);
  window->pointerMove(pointerAt(110.0f, 10.0f));
  EXPECT_EQ(right->enters, 1);
  EXPECT_EQ(left->leaves, 1);
  window->cancelDrag();
  EXPECT_EQ(right->leaves, 1);
  EXPECT_FALSE(window->isDragging());
}

TEST(UiControls, EscapeCancelsADragWithoutDropping) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto probe = std::make_shared<DragProbe>();
  probe->setBounds(Rect::MakeWH(100.0f, 100.0f));
  root->addChild(probe);
  window->setRoot(root);

  ASSERT_TRUE(window->beginDrag(DragPayload::text("payload")));
  window->pointerMove(pointerAt(10.0f, 10.0f));
  EXPECT_EQ(probe->enters, 1);

  const KeyEvent escape = keyPress(Key::Escape);
  window->keyDown(escape);
  EXPECT_FALSE(window->isDragging());
  EXPECT_EQ(probe->drops, 0) << "a cancelled drag must not drop";
  EXPECT_EQ(probe->leaves, 1);
}

TEST(UiControls, ExternalFileDropCarriesADragPayload) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto probe = std::make_shared<DragProbe>();
  probe->setBounds(Rect::MakeWH(100.0f, 100.0f));
  root->addChild(probe);
  window->setRoot(root);

  DropEvent drop;
  drop.position = { 10.0f, 10.0f };
  drop.paths = { "/tmp/clip.mp4" };
  window->drop(drop);
  ASSERT_EQ(probe->drops, 1);
  EXPECT_TRUE(probe->external) << "a platform file drop is external";
  EXPECT_EQ(probe->dropType, "text/uri-list");
  EXPECT_EQ(probe->dropped, "/tmp/clip.mp4");
}

TEST(UiControls, DragPreviewFollowsThePointerAndDisappearsOnDrop) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(200.0f, 120.0f));
  auto probe = std::make_shared<DragProbe>();
  probe->setBounds(Rect::MakeWH(100.0f, 100.0f));
  root->addChild(probe);
  window->setRoot(root);

  auto preview = std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 40, 60, 255));
  preview->setBounds(Rect::MakeWH(24.0f, 24.0f));
  ASSERT_TRUE(window->beginDrag(DragPayload::text("drag"), preview));
  EXPECT_TRUE(window->hasOverlay()) << "the drag image lives in the overlay";
  window->pointerMove(pointerAt(40.0f, 50.0f));
  window->renderFrame();
  window->pump();
  EXPECT_GE(preview->bounds().getLeft(), 40.0f);
  window->pointerUp(pointerAt(40.0f, 50.0f));
  EXPECT_FALSE(window->hasOverlay()) << "the drag image is dismissed with the drag";
}

TEST(UiUndo, StackUndoesAndRedoesInOrder) {
  UndoStack stack;
  int value = 0;
  EXPECT_FALSE(stack.canUndo());
  EXPECT_FALSE(stack.canRedo());
  EXPECT_TRUE(stack.push(std::make_unique<FunctionUndoAction>(
    [&value] { value = 0; }, [&value] { value = 1; }, "Set")));
  EXPECT_TRUE(stack.canUndo());
  EXPECT_EQ(stack.undoLabel(), "Set");
  EXPECT_TRUE(stack.undo());
  EXPECT_EQ(value, 0);
  EXPECT_TRUE(stack.canRedo());
  EXPECT_TRUE(stack.redo());
  EXPECT_EQ(value, 1);
  EXPECT_FALSE(stack.redo());
}

TEST(UiUndo, TransactionGroupsEditsIntoOneStep) {
  UndoStack stack;
  int value = 0;
  ASSERT_TRUE(stack.beginTransaction("Drag"));
  EXPECT_TRUE(stack.inTransaction());
  EXPECT_FALSE(stack.beginTransaction("nested")) << "a nested begin is ignored";
  for (int step = 1; step <= 3; ++step) {
    const int previous = value;
    value = step;
    stack.push(std::make_unique<FunctionUndoAction>([&value, previous] { value = previous; },
      [&value, step] { value = step; }));
  }
  EXPECT_EQ(stack.undoCount(), 0u) << "nothing is recorded until the transaction commits";
  EXPECT_TRUE(stack.commitTransaction());
  EXPECT_EQ(stack.undoCount(), 1u);
  EXPECT_EQ(stack.undoLabel(), "Drag");
  EXPECT_TRUE(stack.undo());
  EXPECT_EQ(value, 0);
  EXPECT_TRUE(stack.redo());
  EXPECT_EQ(value, 3);
}

TEST(UiUndo, EmptyTransactionCommitsNothing) {
  UndoStack stack;
  EXPECT_TRUE(stack.beginTransaction("Nothing"));
  EXPECT_FALSE(stack.commitTransaction());
  EXPECT_FALSE(stack.canUndo());
  EXPECT_FALSE(stack.inTransaction());
}

TEST(UiUndo, LimitDropsOldestAndANewEditClearsRedo) {
  UndoStack stack(2);
  int value = 0;
  for (int step = 1; step <= 4; ++step) {
    const int previous = value;
    value = step;
    stack.push(std::make_unique<FunctionUndoAction>([&value, previous] { value = previous; },
      [&value, step] { value = step; }));
  }
  EXPECT_EQ(stack.undoCount(), 2u) << "the limit keeps the newest edits";
  stack.undo();
  EXPECT_TRUE(stack.canRedo());
  const int previous = value;
  value = 99;
  stack.push(std::make_unique<FunctionUndoAction>([&value, previous] { value = previous; },
    [&value] { value = 99; }));
  EXPECT_FALSE(stack.canRedo()) << "recording a new edit drops the redo branch";
}

TEST(UiUndo, ChangedCallbackFiresForEveryMutation) {
  UndoStack stack;
  int changes = 0;
  stack.setChangedCallback([&changes] { ++changes; });
  int value = 0;
  stack.push(std::make_unique<FunctionUndoAction>([&value] { value = 0; }, [&value] { value = 1; }));
  stack.undo();
  stack.redo();
  stack.clear();
  EXPECT_EQ(changes, 4);
}

TEST(UiWindow, UndoCommandsFollowTheSharedStack) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 90;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  int value = 0;
  window->undoStack().push(std::make_unique<FunctionUndoAction>(
    [&value] { value = 0; }, [&value] { value = 1; }));
  ASSERT_TRUE(window->commands().isEnabled("edit.undo"));

  KeyEvent undo;
  undo.key = Key::Z;
  undo.control = true;
  window->keyDown(undo);
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(window->commands().isEnabled("edit.undo"));
  EXPECT_TRUE(window->commands().isEnabled("edit.redo"));

  KeyEvent redo;
  redo.key = Key::Y;
  redo.control = true;
  window->keyDown(redo);
  EXPECT_EQ(value, 1) << "Ctrl+Y is an alias for edit.redo";
}

TEST(UiControls, SliderDragBecomesOneUndoStep) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  UndoStack stack;
  Slider slider("Opacity", font, 0.0, 1.0, 0.0);
  slider.setBounds(Rect::MakeXYWH(0.0f, 0.0f, 200.0f, 32.0f));
  slider.setUndoStack(&stack, "Change opacity");
  double value = 0.0;
  slider.setOnChanged([&stack, &value](const double next) {
    const double previous = value;
    value = next;
    stack.push(std::make_unique<FunctionUndoAction>([&value, previous] { value = previous; },
      [&value, next] { value = next; }));
  });

  slider.onPointerDown(pointerAt(60.0f, 16.0f));
  slider.onPointerMove(pointerAt(120.0f, 16.0f));
  slider.onPointerMove(pointerAt(160.0f, 16.0f));
  slider.onPointerUp(pointerAt(160.0f, 16.0f));

  EXPECT_EQ(stack.undoCount(), 1u) << "a drag is one undo step, not one per pixel";
  EXPECT_EQ(stack.undoLabel(), "Change opacity");
  const double dragged = value;
  EXPECT_TRUE(stack.undo());
  EXPECT_DOUBLE_EQ(value, 0.0);
  EXPECT_TRUE(stack.redo());
  EXPECT_DOUBLE_EQ(value, dragged);
}