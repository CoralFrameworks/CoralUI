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

#ifndef CORAL_UI_TYPES_H
#define CORAL_UI_TYPES_H

#include <limits>
#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Color.h>
#include <graphics/ColorFilter.h>
#include <graphics/ColorSpace.h>
#include <graphics/Enums.h>
#include <graphics/Font.h>
#include <graphics/FontFallback.h>
#include <graphics/Image.h>
#include <graphics/ImageFilter.h>
#include <graphics/MaskFilter.h>
#include <graphics/Paint.h>
#include <graphics/Path.h>
#include <graphics/PathEffect.h>
#include <graphics/SamplingOptions.h>
#include <graphics/Shader.h>
#include <graphics/TextBlob.h>
#include <graphics/Typeface.h>

#include <geometry/Matrix.h>
#include <geometry/RoundRect.h>
#include <ui/Geometry.h>

/**
 * The graphics value types a UI control is allowed to name.
 *
 * These aliases keep control headers from spelling `libs/graphics` types directly. They are
 * intentionally type aliases, not wrappers: a host that already owns a `graphics::Color` or
 * `graphics::Font` keeps passing it without a conversion, while a control header only needs to
 * include `ui/Types.h`. Backend objects (device, surface, swapchain) are deliberately absent.
 */
namespace coral::ui {

using Color = graphics::Color;
using Font = graphics::Font;
using FontFallback = graphics::FontFallback;
using Paint = graphics::Paint;
using Bitmap = graphics::Bitmap;
using Path = graphics::Path;
using Image = graphics::Image;
using TextBlob = graphics::TextBlob;
using BlendMode = graphics::BlendMode;
using Matrix = graphics::Matrix;
using RoundRect = graphics::RoundRect;
using MaskMode = graphics::MaskMode;
using Shader = graphics::Shader;
using ColorFilter = graphics::ColorFilter;
using MaskFilter = graphics::MaskFilter;
using PathEffect = graphics::PathEffect;
using ImageFilter = graphics::ImageFilter;
using SamplingOptions = graphics::SamplingOptions;
using ColorSpace = graphics::ColorSpace;
using Typeface = graphics::Typeface;

/**
 * What a parent allows a child to occupy while measuring.
 *
 * The legacy `Size` parameter spells "unbounded" as `0`, which cannot be told apart from "zero
 * pixels"; `Constraints` says it explicitly with infinity and adds the minimums a container needs to
 * clamp to. `Layer::measure(const Size&)` remains the implementation hook, and the constraints overload
 * adapts to it, so a container can adopt constraints one at a time.
 */
struct Constraints {
  float minWidth = 0.0f;
  float minHeight = 0.0f;
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();

  /** No limit on either axis: measure the natural size. */
  [[nodiscard]] static Constraints unbounded() noexcept {
    return Constraints{};
  }
  /** Both axes fixed: the child must report exactly this size. */
  [[nodiscard]] static Constraints tight(const Size size) noexcept {
    return Constraints{ size.width, size.height, size.width, size.height };
  }
  [[nodiscard]] bool hasBoundedWidth() const noexcept {
    return maxWidth < std::numeric_limits<float>::infinity();
  }
  [[nodiscard]] bool hasBoundedHeight() const noexcept {
    return maxHeight < std::numeric_limits<float>::infinity();
  }
  /** The legacy `Size` spelling of the maximums: `0` means unbounded there. */
  [[nodiscard]] Size availableSize() const noexcept {
    return Size{ hasBoundedWidth() ? maxWidth : 0.0f, hasBoundedHeight() ? maxHeight : 0.0f };
  }
};

/**
 * A numeric range a node exposes (slider, progress bar).
 *
 * Platform bridges map this onto their own range pattern; the values are already in the control's
 * own units, so the bridge does not have to know how an `int` or a `double` control is implemented.
 */
struct AccessibleRange {
  double value = 0.0;
  double minimum = 0.0;
  double maximum = 1.0;
  /** Step a keyboard/mouse-wheel adjustment should take; 0 means "no small change". */
  double step = 0.0;
};

/** A scrollable viewport: 0..1 percentages, and the visible fraction of the content. */
struct AccessibleScroll {
  double horizontalPercent = 0.0;
  double verticalPercent = 0.0;
  /** Visible fraction of the content along each axis (1.0 when it all fits). */
  double horizontalViewSize = 1.0;
  double verticalViewSize = 1.0;
  bool canScrollHorizontally = false;
  bool canScrollVertically = false;
};

/**
 * A text selection in UTF-16 code units, which is what UI Automation and AT-SPI address text with.
 *
 * The control keeps its own units (TextInput counts UTF-8 bytes); it converts when it fills the
 * snapshot, so a bridge never has to know about the storage encoding.
 */
struct AccessibleTextSelection {
  int start = 0;
  int end = 0;
};

/** A grid/table shape, plus which cell this node is (row/column < 0 for the container itself). */
struct AccessibleGrid {
  int rows = 0;
  int columns = 0;
  int row = -1;
  int column = -1;
};

/** The actions a node exposes, so a platform bridge can map them onto its own patterns. */
enum class AccessibilityAction : uint8_t {
  None = 0,
  Invoke = 1 << 0,
  Toggle = 1 << 1,
  SetValue = 1 << 2,
  Expand = 1 << 3,
  Scroll = 1 << 4,
  Select = 1 << 5,
};

[[nodiscard]] constexpr AccessibilityAction operator|(const AccessibilityAction left,
  const AccessibilityAction right) noexcept {
  return static_cast<AccessibilityAction>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}
constexpr AccessibilityAction& operator|=(AccessibilityAction& left, const AccessibilityAction right) noexcept {
  left = left | right;
  return left;
}
[[nodiscard]] constexpr bool hasAccessibilityAction(const AccessibilityAction actions,
  const AccessibilityAction wanted) noexcept {
  return (static_cast<uint8_t>(actions) & static_cast<uint8_t>(wanted)) != 0;
}

} // namespace coral::ui

#endif // CORAL_UI_TYPES_H
