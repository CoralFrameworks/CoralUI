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

#ifndef CORAL_UI_ELEMENT_H
#define CORAL_UI_ELEMENT_H

#include <ui/Banner.h>
#include <ui/Chip.h>
#include <ui/ColorPicker.h>
#include <ui/DateField.h>
#include <ui/DateTimeField.h>
#include <ui/DateRangeField.h>
#include <ui/Divider.h>
#include <ui/TimeField.h>
#include <ui/TimePicker.h>
#include <ui/TimeRangeField.h>
#include <ui/TreeView.h>
#include <ui/Flex.h>
#include <ui/Skeleton.h>
#include <ui/Stat.h>
#include <ui/StatusIndicator.h>
#include <ui/StyledText.h>
#include <ui/Table.h>
#include <ui/Timeline.h>
#include <ui/Ref.h>
#include <ui/Types.h>

#include <graphics/Font.h>
#include <core/observer/Property.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace graphics {
class Image;
}

namespace coral::ui {

class Layer;
class ContentHost;
class Component;
class ComponentContext;

/** Immutable description of one UI node. Elements are values; layers are the mutable view tree. */
class Element;
using ElementPtr = std::shared_ptr<const Element>;

/**
 * Element kinds the first reconciler slice understands.
 *
 * This is intentionally a closed set: a component-based extension point comes next, after the
 * mount/update/key semantics are proven against these leaves and one container.
 */
enum class ElementKind : uint8_t {
  Text,
  Color,
  Stack,
  Grid,
  Dock,
  When,
  Component,
  Props,
  Button,
  IconButton,
  ColorButton,
  Spinner,
  Slider,
  TextInput,
  CheckBox,
  Toggle,
  RadioButton,
  ProgressBar,
  SegmentedControl,
  Table,
  ListView,
  RecyclingList,
  Scroll,
  ContentView,
  Divider,
  Kbd,
  Badge,
  Icon,
  Avatar,
  AvatarGroup,
  Skeleton,
  Stat,
  Meter,
  ProgressRing,
  StatusIndicator,
  StepIndicator,
  PropertyRow,
  TagList,
  Link,
  Chip,
  Rating,
  Stepper,
  RangeSlider,
  Pagination,
  ToggleButton,
  Breadcrumb,
  Banner,
  TitleBar,
  SplitButton,
  DropZone,
  Calendar,
  StyledText,
  ToggleGroup,
  RadioGroup,
  CheckBoxGroup,
  MultiSelect,
  Card,
  Expander,
  FormField,
  SectionHeader,
  EmptyState,
  SplitView,
  TabBar,
  Toolbar,
  StatusBar,
  Select,
  ComboBox,
  SearchField,
  PasswordField,
  NumberField,
  TagInput,
  ColorInput,
  Timeline,
  ColorPicker,
  CandidateListView,
  TableView,
  DateField,
  TimeField,
  DateTimeField,
  DateRangeField,
  TimeRangeField,
  TimePicker,
  TreeView,
  Layer,
};

/** Layer properties that are independent of an element's own content. */
struct ElementProps {
  /** Absolute bounds; unset leaves the layer's current bounds alone. */
  std::optional<Rect> bounds;
  /**
   * When set and the bounds change, the layer animates to the new rectangle instead of snapping.
   *
   * A zero duration keeps the snap path. The runner is `Layer::animateBounds`, so the same frame
   * loop, curves and interrupt behaviour apply as a manual layout animation.
   */
  std::optional<std::chrono::milliseconds> layoutTransition;
  LayoutParams layout;
  bool visible = true;
  float opacity = 1.0f;
  bool clip = false;
  /** Solid fill behind the child content; unset leaves the layer's background alone. */
  std::optional<Color> background;
  /** Theme-token fill, re-resolved on theme switches; takes precedence over `background`. */
  std::optional<ThemeColorRole> backgroundRole;
  /** Rounded clip radius (together with `clip`); unset leaves the layer's radius alone. */
  std::optional<float> cornerRadius;
  /** Soft shadow drawn behind the layer; unset leaves the layer's shadow alone. */
  std::optional<Shadow> shadow;
  /**
   * Filled with the mounted layer when this props wrapper is reconciled.
   *
   * The wrapper is transparent, so the ref points at the child's layer, not an extra node. Reading it
   * from a component's own build must use `LayerRef::layer()` (null until the mount completes).
   */
  std::shared_ptr<LayerRef> ref;
  /**
   * Named style for the mounted layer; unset leaves the layer's current name alone.
   *
   * Set it when a page wants one instance styled differently from its kind's default rule (see
   * `Layer::defaultStyleName`).
   */
  std::optional<std::string> styleName;
  /**
   * Shared-element identity.
   *
   * When a tagged element leaves the tree its bounds are remembered; a new element with the same tag
   * mounts at those bounds and animates to its own, which is the "hero" transition between two pages.
   */
  std::string sharedElementTag;
};

/**
 * Immutable element base.
 *
 * The tree is rebuilt by the caller on every state change; `Reconciler` compares the new tree with
 * the previous one and mutates the smallest set of layers. A `key` gives an element identity across
 * reorders; an empty key means positional identity within its parent.
 */
class Element {
 public:
  virtual ~Element() = default;

  [[nodiscard]] virtual ElementKind kind() const noexcept = 0;
  /**
   * Whether @p other produces the same measured layout for the same available width.
   *
   * Callback-only changes (onClick/onChanged) are ignored; a different key, text, font, colour or
   * child structure is not. Container kinds with unobservable builders (Component/RecyclingList)
   * return false conservatively. RecyclingListView uses this to avoid re-measuring a row when its
   * builder returns an equivalent-but-new Element.
   */
  [[nodiscard]] virtual bool measureEquivalent(const Element& other) const {
    (void) other;
    return false;
  }
  [[nodiscard]] const std::string& key() const noexcept { return mKey; }
  [[nodiscard]] const std::vector<ElementPtr>& children() const noexcept { return mChildren; }

  Element(const Element&) = delete;
  Element& operator=(const Element&) = delete;

 protected:
  explicit Element(std::string key, std::vector<ElementPtr> children = {})
    : mKey(std::move(key)), mChildren(std::move(children)) {
  }

 private:
  std::string mKey;
  std::vector<ElementPtr> mChildren;
};

namespace detail {

inline bool sameInsets(const Insets& left, const Insets& right) {
  return left == right;
}

inline bool sameRect(const Rect& left, const Rect& right) {
  return left.getLeft() == right.getLeft() && left.getTop() == right.getTop() &&
    left.getWidth() == right.getWidth() && left.getHeight() == right.getHeight();
}


inline bool sameShadow(const Shadow& left, const Shadow& right) {
  return left.blurRadius == right.blurRadius && left.offsetX == right.offsetX &&
    left.offsetY == right.offsetY && left.color == right.color;
}

inline bool sameOptionalColor(const std::optional<Color>& left, const std::optional<Color>& right) {
  if (left.has_value() != right.has_value()) return false;
  return !left.has_value() || *left == *right;
}

/**
 * Value equality of a `PropsElement`'s props.
 *
 * `PropsElement` used to inherit the conservative `measureEquivalent() == false`, so every
 * `WithProps` subtree was re-measured on every rebuild. Comparing the props by value restores the
 * contract while staying conservative about optional fields (an unset field never equals a set one).
 */
inline bool sameElementProps(const ElementProps& left, const ElementProps& right) {
  if (left.bounds.has_value() != right.bounds.has_value()) return false;
  if (left.bounds.has_value() && !sameRect(*left.bounds, *right.bounds)) return false;
  if (!sameLayoutParams(left.layout, right.layout)) return false;
  if (left.visible != right.visible || left.opacity != right.opacity || left.clip != right.clip) {
    return false;
  }
  if (left.background.has_value() != right.background.has_value()) return false;
  if (left.background.has_value() && !(*left.background == *right.background)) return false;
  if (left.backgroundRole != right.backgroundRole) return false;
  if (left.cornerRadius.has_value() != right.cornerRadius.has_value()) return false;
  if (left.cornerRadius.has_value() && *left.cornerRadius != *right.cornerRadius) return false;
  if (left.shadow.has_value() != right.shadow.has_value()) return false;
  if (left.shadow.has_value() && !sameShadow(*left.shadow, *right.shadow)) return false;
  if (left.styleName.has_value() != right.styleName.has_value()) return false;
  if (left.styleName.has_value() && *left.styleName != *right.styleName) return false;
  return true;
}

inline bool sameChildren(const std::vector<ElementPtr>& left,
    const std::vector<ElementPtr>& right) {
  if (left.size() != right.size()) return false;
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i] == nullptr || right[i] == nullptr) {
      if (left[i] != right[i]) return false;
      continue;
    }
    if (!left[i]->measureEquivalent(*right[i])) return false;
  }
  return true;
}

} // namespace detail

/** A single line of text. */
class TextElement final : public Element {
 public:
  TextElement(std::string text, std::shared_ptr<const graphics::Font> font,
    Color color = Color::White(), std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)), mColor(color) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Text; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Text || key() != other.key()) return false;
    const auto& text = static_cast<const TextElement&>(other);
    return mText == text.mText && mFont == text.mFont && mColor == text.mColor;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] Color color() const noexcept { return mColor; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  Color mColor;
};

/** A solid rectangle. */
class ColorElement final : public Element {
 public:
  explicit ColorElement(Color color, std::string key = {})
    : Element(std::move(key)), mColor(color) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Color; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Color || key() != other.key()) return false;
    return mColor == static_cast<const ColorElement&>(other).mColor;
  }
  [[nodiscard]] Color color() const noexcept { return mColor; }

 private:
  Color mColor;
};

/** A horizontal or vertical rule. */
class DividerElement final : public Element {
 public:
  explicit DividerElement(Divider::Orientation orientation = Divider::Orientation::Horizontal,
    float thickness = 1.0f, float inset = 0.0f, std::optional<Color> color = {}, std::string key = {})
    : Element(std::move(key)), mOrientation(orientation), mThickness(thickness), mInset(inset),
      mColor(color) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Divider; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Divider || key() != other.key()) return false;
    const auto& divider = static_cast<const DividerElement&>(other);
    return mOrientation == divider.mOrientation && mThickness == divider.mThickness &&
      mInset == divider.mInset && detail::sameOptionalColor(mColor, divider.mColor);
  }
  [[nodiscard]] Divider::Orientation orientation() const noexcept { return mOrientation; }
  [[nodiscard]] float thickness() const noexcept { return mThickness; }
  [[nodiscard]] float inset() const noexcept { return mInset; }
  [[nodiscard]] const std::optional<Color>& color() const noexcept { return mColor; }

 private:
  Divider::Orientation mOrientation;
  float mThickness;
  float mInset;
  std::optional<Color> mColor;
};

/** A keyboard shortcut cap. */
class KbdElement final : public Element {
 public:
  KbdElement(std::string label, std::shared_ptr<const graphics::Font> font, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Kbd; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Kbd || key() != other.key()) return false;
    const auto& kbd = static_cast<const KbdElement&>(other);
    return mLabel == kbd.mLabel && mFont == kbd.mFont;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
};

/** A status/count pill. */
class BadgeElement final : public Element {
 public:
  BadgeElement(std::string text, std::shared_ptr<const graphics::Font> font, bool dot = false,
    std::shared_ptr<const graphics::Image> icon = {}, std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)), mDot(dot),
      mIcon(std::move(icon)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Badge; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Badge || key() != other.key()) return false;
    const auto& badge = static_cast<const BadgeElement&>(other);
    return mText == badge.mText && mFont == badge.mFont && mDot == badge.mDot &&
      mIcon == badge.mIcon;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] bool dot() const noexcept { return mDot; }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept { return mIcon; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  bool mDot;
  std::shared_ptr<const graphics::Image> mIcon;
};

/** A square tinted image. */
class IconElement final : public Element {
 public:
  explicit IconElement(std::shared_ptr<const graphics::Image> image = {}, float size = 20.0f,
    std::optional<Color> color = {}, std::string key = {})
    : Element(std::move(key)), mImage(std::move(image)), mSize(size), mColor(color) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Icon; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Icon || key() != other.key()) return false;
    const auto& icon = static_cast<const IconElement&>(other);
    return mImage == icon.mImage && mSize == icon.mSize &&
      detail::sameOptionalColor(mColor, icon.mColor);
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& image() const noexcept { return mImage; }
  [[nodiscard]] float size() const noexcept { return mSize; }
  [[nodiscard]] const std::optional<Color>& color() const noexcept { return mColor; }

 private:
  std::shared_ptr<const graphics::Image> mImage;
  float mSize;
  std::optional<Color> mColor;
};

/** A round initials/image avatar. */
class AvatarElement final : public Element {
 public:
  AvatarElement(std::string initials, std::shared_ptr<const graphics::Font> font, float size = 32.0f,
    std::shared_ptr<const graphics::Image> image = {}, bool statusVisible = false,
    std::optional<Color> statusColor = {}, std::string key = {})
    : Element(std::move(key)), mInitials(std::move(initials)), mFont(std::move(font)), mSize(size),
      mImage(std::move(image)), mStatusVisible(statusVisible), mStatusColor(statusColor) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Avatar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Avatar || key() != other.key()) return false;
    const auto& avatar = static_cast<const AvatarElement&>(other);
    return mInitials == avatar.mInitials && mFont == avatar.mFont && mSize == avatar.mSize &&
      mImage == avatar.mImage && mStatusVisible == avatar.mStatusVisible &&
      detail::sameOptionalColor(mStatusColor, avatar.mStatusColor);
  }
  [[nodiscard]] const std::string& initials() const noexcept { return mInitials; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] float size() const noexcept { return mSize; }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& image() const noexcept { return mImage; }
  [[nodiscard]] bool statusVisible() const noexcept { return mStatusVisible; }
  [[nodiscard]] const std::optional<Color>& statusColor() const noexcept { return mStatusColor; }

 private:
  std::string mInitials;
  std::shared_ptr<const graphics::Font> mFont;
  float mSize;
  std::shared_ptr<const graphics::Image> mImage;
  bool mStatusVisible;
  std::optional<Color> mStatusColor;
};

/** An overlapping row of avatars. */
class AvatarGroupElement final : public Element {
 public:
  AvatarGroupElement(std::vector<std::string> initials, std::shared_ptr<const graphics::Font> font,
    int maxVisible = 4, float size = 28.0f, std::string key = {})
    : Element(std::move(key)), mInitials(std::move(initials)), mFont(std::move(font)),
      mMaxVisible(maxVisible), mSize(size) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::AvatarGroup; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::AvatarGroup || key() != other.key()) return false;
    const auto& group = static_cast<const AvatarGroupElement&>(other);
    return mInitials == group.mInitials && mFont == group.mFont &&
      mMaxVisible == group.mMaxVisible && mSize == group.mSize;
  }
  [[nodiscard]] const std::vector<std::string>& initials() const noexcept { return mInitials; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int maxVisible() const noexcept { return mMaxVisible; }
  [[nodiscard]] float size() const noexcept { return mSize; }

 private:
  std::vector<std::string> mInitials;
  std::shared_ptr<const graphics::Font> mFont;
  int mMaxVisible;
  float mSize;
};

/** A loading placeholder block. */
class SkeletonElement final : public Element {
 public:
  explicit SkeletonElement(float width = 160.0f, float height = 14.0f, int lines = 1,
    float lineGap = 6.0f, Skeleton::Shape shape = Skeleton::Shape::Rounded, bool animating = true,
    std::string key = {})
    : Element(std::move(key)), mWidth(width), mHeight(height), mLines(lines), mLineGap(lineGap),
      mShape(shape), mAnimating(animating) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Skeleton; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Skeleton || key() != other.key()) return false;
    const auto& skeleton = static_cast<const SkeletonElement&>(other);
    return mWidth == skeleton.mWidth && mHeight == skeleton.mHeight && mLines == skeleton.mLines &&
      mLineGap == skeleton.mLineGap && mShape == skeleton.mShape &&
      mAnimating == skeleton.mAnimating;
  }
  [[nodiscard]] float width() const noexcept { return mWidth; }
  [[nodiscard]] float height() const noexcept { return mHeight; }
  [[nodiscard]] int lines() const noexcept { return mLines; }
  [[nodiscard]] float lineGap() const noexcept { return mLineGap; }
  [[nodiscard]] Skeleton::Shape shape() const noexcept { return mShape; }
  [[nodiscard]] bool animating() const noexcept { return mAnimating; }

 private:
  float mWidth;
  float mHeight;
  int mLines;
  float mLineGap;
  Skeleton::Shape mShape;
  bool mAnimating;
};

/** A compact dashboard statistic. */
class StatElement final : public Element {
 public:
  StatElement(std::string label, std::string value, std::shared_ptr<const graphics::Font> font,
    std::string caption = {}, StatTrend trend = StatTrend::None, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mValue(std::move(value)),
      mFont(std::move(font)), mCaption(std::move(caption)), mTrend(trend) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Stat; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Stat || key() != other.key()) return false;
    const auto& stat = static_cast<const StatElement&>(other);
    return mLabel == stat.mLabel && mValue == stat.mValue && mFont == stat.mFont &&
      mCaption == stat.mCaption && mTrend == stat.mTrend;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::string& value() const noexcept { return mValue; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& caption() const noexcept { return mCaption; }
  [[nodiscard]] StatTrend trend() const noexcept { return mTrend; }

 private:
  std::string mLabel;
  std::string mValue;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mCaption;
  StatTrend mTrend;
};

/** A low/high meter with thresholds. */
class MeterElement final : public Element {
 public:
  explicit MeterElement(double minimum = 0.0, double maximum = 1.0, double value = 0.0,
    std::string key = {})
    : Element(std::move(key)), mMinimum(minimum), mMaximum(maximum), mValue(value) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Meter; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Meter || key() != other.key()) return false;
    const auto& meter = static_cast<const MeterElement&>(other);
    return mMinimum == meter.mMinimum && mMaximum == meter.mMaximum && mValue == meter.mValue;
  }
  [[nodiscard]] double minimum() const noexcept { return mMinimum; }
  [[nodiscard]] double maximum() const noexcept { return mMaximum; }
  [[nodiscard]] double value() const noexcept { return mValue; }

 private:
  double mMinimum;
  double mMaximum;
  double mValue;
};

/** A circular progress indicator. */
class ProgressRingElement final : public Element {
 public:
  explicit ProgressRingElement(float size = 36.0f, double value = 0.0, float thickness = 4.0f,
    bool indeterminate = false, std::string key = {})
    : Element(std::move(key)), mSize(size), mValue(value), mThickness(thickness),
      mIndeterminate(indeterminate) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ProgressRing; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ProgressRing || key() != other.key()) return false;
    const auto& ring = static_cast<const ProgressRingElement&>(other);
    return mSize == ring.mSize && mValue == ring.mValue && mThickness == ring.mThickness &&
      mIndeterminate == ring.mIndeterminate;
  }
  [[nodiscard]] float size() const noexcept { return mSize; }
  [[nodiscard]] double value() const noexcept { return mValue; }
  [[nodiscard]] float thickness() const noexcept { return mThickness; }
  [[nodiscard]] bool indeterminate() const noexcept { return mIndeterminate; }

 private:
  float mSize;
  double mValue;
  float mThickness;
  bool mIndeterminate;
};

/** A coloured status dot with a label. */
class StatusIndicatorElement final : public Element {
 public:
  StatusIndicatorElement(std::string label, std::shared_ptr<const graphics::Font> font,
    StatusKind status = StatusKind::Online, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)), mStatus(status) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override {
    return ElementKind::StatusIndicator;
  }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::StatusIndicator || key() != other.key()) return false;
    const auto& status = static_cast<const StatusIndicatorElement&>(other);
    return mLabel == status.mLabel && mFont == status.mFont && mStatus == status.mStatus;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] StatusKind status() const noexcept { return mStatus; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  StatusKind mStatus;
};

/** A numbered step trail. */
class StepIndicatorElement final : public Element {
 public:
  using ChangeHandler = std::function<void(int)>;

  StepIndicatorElement(std::vector<std::string> steps, std::shared_ptr<const graphics::Font> font,
    int current = 0, ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mSteps(std::move(steps)), mFont(std::move(font)), mCurrent(current),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::StepIndicator; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::StepIndicator || key() != other.key()) return false;
    const auto& steps = static_cast<const StepIndicatorElement&>(other);
    return mSteps == steps.mSteps && mFont == steps.mFont && mCurrent == steps.mCurrent;
  }
  [[nodiscard]] const std::vector<std::string>& steps() const noexcept { return mSteps; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int current() const noexcept { return mCurrent; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mSteps;
  std::shared_ptr<const graphics::Font> mFont;
  int mCurrent;
  ChangeHandler mOnChanged;
};

/** A label/value settings row. */
class PropertyRowElement final : public Element {
 public:
  PropertyRowElement(std::string label, std::string value,
    std::shared_ptr<const graphics::Font> font, std::string help = {}, float labelWidth = 0.0f,
    std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mValue(std::move(value)),
      mFont(std::move(font)), mHelp(std::move(help)), mLabelWidth(labelWidth) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::PropertyRow; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::PropertyRow || key() != other.key()) return false;
    const auto& row = static_cast<const PropertyRowElement&>(other);
    return mLabel == row.mLabel && mValue == row.mValue && mFont == row.mFont &&
      mHelp == row.mHelp && mLabelWidth == row.mLabelWidth;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::string& value() const noexcept { return mValue; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& help() const noexcept { return mHelp; }
  [[nodiscard]] float labelWidth() const noexcept { return mLabelWidth; }

 private:
  std::string mLabel;
  std::string mValue;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mHelp;
  float mLabelWidth;
};

/** A single-line tag list with a +N overflow marker. */
class TagListElement final : public Element {
 public:
  TagListElement(std::vector<std::string> tags, std::shared_ptr<const graphics::Font> font,
    int maxVisible = 4, std::string key = {})
    : Element(std::move(key)), mTags(std::move(tags)), mFont(std::move(font)),
      mMaxVisible(maxVisible) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TagList; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TagList || key() != other.key()) return false;
    const auto& tags = static_cast<const TagListElement&>(other);
    return mTags == tags.mTags && mFont == tags.mFont && mMaxVisible == tags.mMaxVisible;
  }
  [[nodiscard]] const std::vector<std::string>& tags() const noexcept { return mTags; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int maxVisible() const noexcept { return mMaxVisible; }

 private:
  std::vector<std::string> mTags;
  std::shared_ptr<const graphics::Font> mFont;
  int mMaxVisible;
};
/** An inline text link. */
class LinkElement final : public Element {
 public:
  LinkElement(std::string text, std::shared_ptr<const graphics::Font> font,
    std::function<void()> onActivate = {}, std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)),
      mOnActivate(std::move(onActivate)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Link; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Link || key() != other.key()) return false;
    const auto& link = static_cast<const LinkElement&>(other);
    return mText == link.mText && mFont == link.mFont;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::function<void()>& onActivate() const noexcept { return mOnActivate; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  std::function<void()> mOnActivate;
};

/** A compact tag/chip with an optional remove button. */
class ChipElement final : public Element {
 public:
  ChipElement(std::string text, std::shared_ptr<const graphics::Font> font,
    ChipVariant variant = ChipVariant::Neutral, std::shared_ptr<const graphics::Image> icon = {},
    bool removable = false, bool selected = false, std::function<void()> onRemove = {},
    std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)), mVariant(variant),
      mIcon(std::move(icon)), mRemovable(removable), mSelected(selected),
      mOnRemove(std::move(onRemove)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Chip; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Chip || key() != other.key()) return false;
    const auto& chip = static_cast<const ChipElement&>(other);
    return mText == chip.mText && mFont == chip.mFont && mVariant == chip.mVariant &&
      mIcon == chip.mIcon && mRemovable == chip.mRemovable && mSelected == chip.mSelected;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] ChipVariant variant() const noexcept { return mVariant; }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept { return mIcon; }
  [[nodiscard]] bool removable() const noexcept { return mRemovable; }
  [[nodiscard]] bool selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::function<void()>& onRemove() const noexcept { return mOnRemove; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  ChipVariant mVariant;
  std::shared_ptr<const graphics::Image> mIcon;
  bool mRemovable;
  bool mSelected;
  std::function<void()> mOnRemove;
};

/** A star rating; integer or half-step. */
class RatingElement final : public Element {
 public:
  explicit RatingElement(int stars = 5, double rating = 0.0, bool halfSteps = false,
    bool readOnly = false, std::function<void(double)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mStars(stars), mRating(rating), mHalfSteps(halfSteps),
      mReadOnly(readOnly), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Rating; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Rating || key() != other.key()) return false;
    const auto& rating = static_cast<const RatingElement&>(other);
    return mStars == rating.mStars && mRating == rating.mRating &&
      mHalfSteps == rating.mHalfSteps && mReadOnly == rating.mReadOnly;
  }
  [[nodiscard]] int stars() const noexcept { return mStars; }
  [[nodiscard]] double rating() const noexcept { return mRating; }
  [[nodiscard]] bool halfSteps() const noexcept { return mHalfSteps; }
  [[nodiscard]] bool readOnly() const noexcept { return mReadOnly; }
  [[nodiscard]] const std::function<void(double)>& onChanged() const noexcept { return mOnChanged; }

 private:
  int mStars;
  double mRating;
  bool mHalfSteps;
  bool mReadOnly;
  std::function<void(double)> mOnChanged;
};

/** A numeric stepper. */
class StepperElement final : public Element {
 public:
  StepperElement(double minimum, double maximum, double value, double step = 1.0,
    std::function<void(double)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mMinimum(minimum), mMaximum(maximum), mValue(value), mStep(step),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Stepper; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Stepper || key() != other.key()) return false;
    const auto& stepper = static_cast<const StepperElement&>(other);
    return mMinimum == stepper.mMinimum && mMaximum == stepper.mMaximum &&
      mValue == stepper.mValue && mStep == stepper.mStep;
  }
  [[nodiscard]] double minimum() const noexcept { return mMinimum; }
  [[nodiscard]] double maximum() const noexcept { return mMaximum; }
  [[nodiscard]] double value() const noexcept { return mValue; }
  [[nodiscard]] double step() const noexcept { return mStep; }
  [[nodiscard]] const std::function<void(double)>& onChanged() const noexcept { return mOnChanged; }

 private:
  double mMinimum;
  double mMaximum;
  double mValue;
  double mStep;
  std::function<void(double)> mOnChanged;
};

/** A two-handle range slider. */
class RangeSliderElement final : public Element {
 public:
  explicit RangeSliderElement(double minimum = 0.0, double maximum = 1.0, double low = 0.25,
    double high = 0.75, double step = 0.01, std::function<void(double, double)> onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mMinimum(minimum), mMaximum(maximum), mLow(low), mHigh(high),
      mStep(step), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::RangeSlider; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::RangeSlider || key() != other.key()) return false;
    const auto& range = static_cast<const RangeSliderElement&>(other);
    return mMinimum == range.mMinimum && mMaximum == range.mMaximum && mLow == range.mLow &&
      mHigh == range.mHigh && mStep == range.mStep;
  }
  [[nodiscard]] double minimum() const noexcept { return mMinimum; }
  [[nodiscard]] double maximum() const noexcept { return mMaximum; }
  [[nodiscard]] double low() const noexcept { return mLow; }
  [[nodiscard]] double high() const noexcept { return mHigh; }
  [[nodiscard]] double step() const noexcept { return mStep; }
  [[nodiscard]] const std::function<void(double, double)>& onChanged() const noexcept {
    return mOnChanged;
  }

 private:
  double mMinimum;
  double mMaximum;
  double mLow;
  double mHigh;
  double mStep;
  std::function<void(double, double)> mOnChanged;
};

/** A numbered page selector. */
class PaginationElement final : public Element {
 public:
  PaginationElement(std::shared_ptr<const graphics::Font> font, int pageCount = 1, int page = 0,
    std::function<void(int)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mPageCount(pageCount), mPage(page),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Pagination; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Pagination || key() != other.key()) return false;
    const auto& pagination = static_cast<const PaginationElement&>(other);
    return mFont == pagination.mFont && mPageCount == pagination.mPageCount &&
      mPage == pagination.mPage;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int pageCount() const noexcept { return mPageCount; }
  [[nodiscard]] int page() const noexcept { return mPage; }
  [[nodiscard]] const std::function<void(int)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  int mPageCount;
  int mPage;
  std::function<void(int)> mOnChanged;
};

/** A button with a persistent selected state. */
class ToggleButtonElement final : public Element {
 public:
  ToggleButtonElement(std::string label, std::shared_ptr<const graphics::Font> font,
    bool selected = false, std::function<void(bool)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)),
      mSelected(selected), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ToggleButton; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ToggleButton || key() != other.key()) return false;
    const auto& button = static_cast<const ToggleButtonElement&>(other);
    return mLabel == button.mLabel && mFont == button.mFont && mSelected == button.mSelected;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] bool selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::function<void(bool)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  bool mSelected;
  std::function<void(bool)> mOnChanged;
};

/** A clickable trail of path segments. */
class BreadcrumbElement final : public Element {
 public:
  using SelectHandler = std::function<void(int, std::string_view)>;

  BreadcrumbElement(std::vector<std::string> items, std::shared_ptr<const graphics::Font> font,
    std::string separator = "/", int currentIndex = -1, SelectHandler onSelect = {},
    std::string key = {})
    : Element(std::move(key)), mItems(std::move(items)), mFont(std::move(font)),
      mSeparator(std::move(separator)), mCurrentIndex(currentIndex),
      mOnSelect(std::move(onSelect)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Breadcrumb; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Breadcrumb || key() != other.key()) return false;
    const auto& crumb = static_cast<const BreadcrumbElement&>(other);
    return mItems == crumb.mItems && mFont == crumb.mFont && mSeparator == crumb.mSeparator &&
      mCurrentIndex == crumb.mCurrentIndex;
  }
  [[nodiscard]] const std::vector<std::string>& items() const noexcept { return mItems; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& separator() const noexcept { return mSeparator; }
  [[nodiscard]] int currentIndex() const noexcept { return mCurrentIndex; }
  [[nodiscard]] const SelectHandler& onSelect() const noexcept { return mOnSelect; }

 private:
  std::vector<std::string> mItems;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mSeparator;
  int mCurrentIndex;
  SelectHandler mOnSelect;
};

/** A severity banner with an optional action and dismiss button. */
class BannerElement final : public Element {
 public:
  BannerElement(std::string text, std::shared_ptr<const graphics::Font> font,
    BannerSeverity severity = BannerSeverity::Info, std::string actionLabel = {},
    std::function<void()> onAction = {}, bool dismissible = false,
    std::shared_ptr<const graphics::Image> icon = {}, std::function<void()> onDismiss = {},
    std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)), mSeverity(severity),
      mActionLabel(std::move(actionLabel)), mOnAction(std::move(onAction)),
      mDismissible(dismissible), mIcon(std::move(icon)), mOnDismiss(std::move(onDismiss)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Banner; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Banner || key() != other.key()) return false;
    const auto& banner = static_cast<const BannerElement&>(other);
    return mText == banner.mText && mFont == banner.mFont && mSeverity == banner.mSeverity &&
      mActionLabel == banner.mActionLabel && mDismissible == banner.mDismissible &&
      mIcon == banner.mIcon;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] BannerSeverity severity() const noexcept { return mSeverity; }
  [[nodiscard]] const std::string& actionLabel() const noexcept { return mActionLabel; }
  [[nodiscard]] const std::function<void()>& onAction() const noexcept { return mOnAction; }
  [[nodiscard]] bool dismissible() const noexcept { return mDismissible; }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept { return mIcon; }
  [[nodiscard]] const std::function<void()>& onDismiss() const noexcept { return mOnDismiss; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  BannerSeverity mSeverity;
  std::string mActionLabel;
  std::function<void()> mOnAction;
  bool mDismissible;
  std::shared_ptr<const graphics::Image> mIcon;
  std::function<void()> mOnDismiss;
};

/** A client-side window title bar. */
class TitleBarElement final : public Element {
 public:
  TitleBarElement(std::string title, std::shared_ptr<const graphics::Font> font,
    std::function<void()> onMinimize = {}, std::function<void()> onMaximize = {},
    std::function<void()> onClose = {}, bool showMinimize = true, bool showMaximize = true,
    bool showClose = true, std::string key = {})
    : Element(std::move(key)), mTitle(std::move(title)), mFont(std::move(font)),
      mOnMinimize(std::move(onMinimize)), mOnMaximize(std::move(onMaximize)),
      mOnClose(std::move(onClose)), mShowMinimize(showMinimize), mShowMaximize(showMaximize),
      mShowClose(showClose) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TitleBar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TitleBar || key() != other.key()) return false;
    const auto& bar = static_cast<const TitleBarElement&>(other);
    return mTitle == bar.mTitle && mFont == bar.mFont && mShowMinimize == bar.mShowMinimize &&
      mShowMaximize == bar.mShowMaximize && mShowClose == bar.mShowClose;
  }
  [[nodiscard]] const std::string& title() const noexcept { return mTitle; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::function<void()>& onMinimize() const noexcept { return mOnMinimize; }
  [[nodiscard]] const std::function<void()>& onMaximize() const noexcept { return mOnMaximize; }
  [[nodiscard]] const std::function<void()>& onClose() const noexcept { return mOnClose; }
  [[nodiscard]] bool showMinimize() const noexcept { return mShowMinimize; }
  [[nodiscard]] bool showMaximize() const noexcept { return mShowMaximize; }
  [[nodiscard]] bool showClose() const noexcept { return mShowClose; }

 private:
  std::string mTitle;
  std::shared_ptr<const graphics::Font> mFont;
  std::function<void()> mOnMinimize;
  std::function<void()> mOnMaximize;
  std::function<void()> mOnClose;
  bool mShowMinimize;
  bool mShowMaximize;
  bool mShowClose;
};

/** A primary action plus a dropdown of related options. */
class SplitButtonElement final : public Element {
 public:
  using OptionHandler = std::function<void(int, std::string_view)>;

  SplitButtonElement(std::string text, std::shared_ptr<const graphics::Font> font,
    std::vector<std::string> options = {}, std::function<void()> onClick = {},
    OptionHandler onOption = {}, std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)),
      mOptions(std::move(options)), mOnClick(std::move(onClick)), mOnOption(std::move(onOption)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::SplitButton; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::SplitButton || key() != other.key()) return false;
    const auto& button = static_cast<const SplitButtonElement&>(other);
    return mText == button.mText && mFont == button.mFont && mOptions == button.mOptions;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::function<void()>& onClick() const noexcept { return mOnClick; }
  [[nodiscard]] const OptionHandler& onOption() const noexcept { return mOnOption; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  std::vector<std::string> mOptions;
  std::function<void()> mOnClick;
  OptionHandler mOnOption;
};

/** A drop target for files or text. */
class DropZoneElement final : public Element {
 public:
  DropZoneElement(std::string label, std::shared_ptr<const graphics::Font> font,
    std::function<void(std::vector<std::string>)> onFilesDropped = {},
    std::function<void(std::string)> onTextDropped = {}, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)),
      mOnFilesDropped(std::move(onFilesDropped)), mOnTextDropped(std::move(onTextDropped)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::DropZone; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::DropZone || key() != other.key()) return false;
    const auto& zone = static_cast<const DropZoneElement&>(other);
    return mLabel == zone.mLabel && mFont == zone.mFont;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::function<void(std::vector<std::string>)>& onFilesDropped() const noexcept {
    return mOnFilesDropped;
  }
  [[nodiscard]] const std::function<void(std::string)>& onTextDropped() const noexcept {
    return mOnTextDropped;
  }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  std::function<void(std::vector<std::string>)> mOnFilesDropped;
  std::function<void(std::string)> mOnTextDropped;
};

/** A month grid with a selected date. */
class CalendarElement final : public Element {
 public:
  CalendarElement(std::chrono::year_month_day date, std::shared_ptr<const graphics::Font> font,
    int firstWeekday = 0, std::function<void(std::chrono::year_month_day)> onSelect = {},
    std::function<void()> onCancel = {}, std::string key = {})
    : Element(std::move(key)), mDate(date), mFont(std::move(font)), mFirstWeekday(firstWeekday),
      mOnSelect(std::move(onSelect)), mOnCancel(std::move(onCancel)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Calendar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Calendar || key() != other.key()) return false;
    const auto& calendar = static_cast<const CalendarElement&>(other);
    return mDate == calendar.mDate && mFont == calendar.mFont &&
      mFirstWeekday == calendar.mFirstWeekday;
  }
  [[nodiscard]] const std::chrono::year_month_day& date() const noexcept { return mDate; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int firstWeekday() const noexcept { return mFirstWeekday; }
  [[nodiscard]] const std::function<void(std::chrono::year_month_day)>& onSelect() const noexcept {
    return mOnSelect;
  }
  [[nodiscard]] const std::function<void()>& onCancel() const noexcept { return mOnCancel; }

 private:
  std::chrono::year_month_day mDate;
  std::shared_ptr<const graphics::Font> mFont;
  int mFirstWeekday;
  std::function<void(std::chrono::year_month_day)> mOnSelect;
  std::function<void()> mOnCancel;
};
/** A styled rich-text document with optional links. */
class StyledTextElement final : public Element {
 public:
  explicit StyledTextElement(TextDocument document = {},
    std::function<void(std::string_view)> onLinkActivated = {}, std::string key = {})
    : Element(std::move(key)), mDocument(std::move(document)),
      mOnLinkActivated(std::move(onLinkActivated)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::StyledText; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::StyledText || key() != other.key()) return false;
    return mDocument == static_cast<const StyledTextElement&>(other).mDocument;
  }
  [[nodiscard]] const TextDocument& document() const noexcept { return mDocument; }
  [[nodiscard]] const std::function<void(std::string_view)>& onLinkActivated() const noexcept {
    return mOnLinkActivated;
  }

 private:
  TextDocument mDocument;
  std::function<void(std::string_view)> mOnLinkActivated;
};
/** A multi-select row of toggle buttons. */
class ToggleGroupElement final : public Element {
 public:
  using ChangeHandler = std::function<void(std::vector<int>)>;

  ToggleGroupElement(std::vector<std::string> options, std::shared_ptr<const graphics::Font> font,
    std::vector<int> selected = {}, ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mOptions(std::move(options)), mFont(std::move(font)),
      mSelected(std::move(selected)), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ToggleGroup; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ToggleGroup || key() != other.key()) return false;
    const auto& group = static_cast<const ToggleGroupElement&>(other);
    return mOptions == group.mOptions && mFont == group.mFont && mSelected == group.mSelected;
  }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<int>& selected() const noexcept { return mSelected; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mOptions;
  std::shared_ptr<const graphics::Font> mFont;
  std::vector<int> mSelected;
  ChangeHandler mOnChanged;
};

/** An exclusive radio group. */
class RadioGroupElement final : public Element {
 public:
  RadioGroupElement(std::vector<std::string> options, std::shared_ptr<const graphics::Font> font,
    int selected = 0, std::function<void(int)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mOptions(std::move(options)), mFont(std::move(font)),
      mSelected(selected), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::RadioGroup; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::RadioGroup || key() != other.key()) return false;
    const auto& group = static_cast<const RadioGroupElement&>(other);
    return mOptions == group.mOptions && mFont == group.mFont && mSelected == group.mSelected;
  }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::function<void(int)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mOptions;
  std::shared_ptr<const graphics::Font> mFont;
  int mSelected;
  std::function<void(int)> mOnChanged;
};

/** A multi-select check box group. */
class CheckBoxGroupElement final : public Element {
 public:
  using ChangeHandler = std::function<void(std::vector<int>)>;

  CheckBoxGroupElement(std::vector<std::string> options, std::shared_ptr<const graphics::Font> font,
    std::vector<int> selected = {}, ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mOptions(std::move(options)), mFont(std::move(font)),
      mSelected(std::move(selected)), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::CheckBoxGroup; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::CheckBoxGroup || key() != other.key()) return false;
    const auto& group = static_cast<const CheckBoxGroupElement&>(other);
    return mOptions == group.mOptions && mFont == group.mFont && mSelected == group.mSelected;
  }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<int>& selected() const noexcept { return mSelected; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mOptions;
  std::shared_ptr<const graphics::Font> mFont;
  std::vector<int> mSelected;
  ChangeHandler mOnChanged;
};

/** A dropdown multi-select. */
class MultiSelectElement final : public Element {
 public:
  using ChangeHandler = std::function<void(std::vector<int>)>;

  MultiSelectElement(std::vector<std::string> options, std::shared_ptr<const graphics::Font> font,
    std::vector<int> selected = {}, std::string placeholder = {}, ChangeHandler onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mOptions(std::move(options)), mFont(std::move(font)),
      mSelected(std::move(selected)), mPlaceholder(std::move(placeholder)),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::MultiSelect; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::MultiSelect || key() != other.key()) return false;
    const auto& select = static_cast<const MultiSelectElement&>(other);
    return mOptions == select.mOptions && mFont == select.mFont && mSelected == select.mSelected &&
      mPlaceholder == select.mPlaceholder;
  }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<int>& selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::string& placeholder() const noexcept { return mPlaceholder; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mOptions;
  std::shared_ptr<const graphics::Font> mFont;
  std::vector<int> mSelected;
  std::string mPlaceholder;
  ChangeHandler mOnChanged;
};
/** A titled card with a content slot and an optional header action. */
class CardElement final : public Element {
 public:
  CardElement(std::string title, std::shared_ptr<const graphics::Font> font,
    ElementPtr content = {}, ElementPtr headerAction = {}, std::string key = {})
    : Element(std::move(key), { std::move(content), std::move(headerAction) }),
      mTitle(std::move(title)), mFont(std::move(font)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Card; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Card || key() != other.key()) return false;
    const auto& card = static_cast<const CardElement&>(other);
    return mTitle == card.mTitle && mFont == card.mFont &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::string& title() const noexcept { return mTitle; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const ElementPtr& content() const noexcept { return children()[0]; }
  [[nodiscard]] const ElementPtr& headerAction() const noexcept { return children()[1]; }

 private:
  std::string mTitle;
  std::shared_ptr<const graphics::Font> mFont;
};

/** A collapsible section with one content slot. */
class ExpanderElement final : public Element {
 public:
  ExpanderElement(std::string title, std::shared_ptr<const graphics::Font> font,
    ElementPtr content = {}, bool expanded = true,
    std::function<void(bool)> onChanged = {}, std::string key = {})
    : Element(std::move(key), { std::move(content) }), mTitle(std::move(title)),
      mFont(std::move(font)), mExpanded(expanded), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Expander; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Expander || key() != other.key()) return false;
    const auto& expander = static_cast<const ExpanderElement&>(other);
    return mTitle == expander.mTitle && mFont == expander.mFont &&
      mExpanded == expander.mExpanded && detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::string& title() const noexcept { return mTitle; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] bool expanded() const noexcept { return mExpanded; }
  [[nodiscard]] const ElementPtr& content() const noexcept { return children()[0]; }
  [[nodiscard]] const std::function<void(bool)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::string mTitle;
  std::shared_ptr<const graphics::Font> mFont;
  bool mExpanded;
  std::function<void(bool)> mOnChanged;
};

/** A labelled form row with help/error text and a content slot. */
class FormFieldElement final : public Element {
 public:
  FormFieldElement(std::string label, std::shared_ptr<const graphics::Font> font,
    ElementPtr content = {}, std::string help = {}, std::string error = {}, std::string key = {})
    : Element(std::move(key), { std::move(content) }), mLabel(std::move(label)),
      mFont(std::move(font)), mHelp(std::move(help)), mError(std::move(error)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::FormField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::FormField || key() != other.key()) return false;
    const auto& field = static_cast<const FormFieldElement&>(other);
    return mLabel == field.mLabel && mFont == field.mFont && mHelp == field.mHelp &&
      mError == field.mError && detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& help() const noexcept { return mHelp; }
  [[nodiscard]] const std::string& error() const noexcept { return mError; }
  [[nodiscard]] const ElementPtr& content() const noexcept { return children()[0]; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mHelp;
  std::string mError;
};

/** A section title/subtitle with an optional action slot. */
class SectionHeaderElement final : public Element {
 public:
  SectionHeaderElement(std::string title, std::shared_ptr<const graphics::Font> font,
    std::string subtitle = {}, ElementPtr action = {}, std::string key = {})
    : Element(std::move(key), { std::move(action) }), mTitle(std::move(title)),
      mFont(std::move(font)), mSubtitle(std::move(subtitle)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::SectionHeader; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::SectionHeader || key() != other.key()) return false;
    const auto& header = static_cast<const SectionHeaderElement&>(other);
    return mTitle == header.mTitle && mFont == header.mFont && mSubtitle == header.mSubtitle &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::string& title() const noexcept { return mTitle; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& subtitle() const noexcept { return mSubtitle; }
  [[nodiscard]] const ElementPtr& action() const noexcept { return children()[0]; }

 private:
  std::string mTitle;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mSubtitle;
};

/** An empty-state panel with content and action slots. */
class EmptyStateElement final : public Element {
 public:
  EmptyStateElement(std::string title, std::shared_ptr<const graphics::Font> font,
    std::string description = {}, ElementPtr content = {}, ElementPtr action = {},
    std::shared_ptr<const graphics::Image> icon = {}, std::string key = {})
    : Element(std::move(key), { std::move(content), std::move(action) }),
      mTitle(std::move(title)), mFont(std::move(font)), mDescription(std::move(description)),
      mIcon(std::move(icon)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::EmptyState; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::EmptyState || key() != other.key()) return false;
    const auto& empty = static_cast<const EmptyStateElement&>(other);
    return mTitle == empty.mTitle && mFont == empty.mFont &&
      mDescription == empty.mDescription && mIcon == empty.mIcon &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::string& title() const noexcept { return mTitle; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& description() const noexcept { return mDescription; }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept { return mIcon; }
  [[nodiscard]] const ElementPtr& content() const noexcept { return children()[0]; }
  [[nodiscard]] const ElementPtr& action() const noexcept { return children()[1]; }

 private:
  std::string mTitle;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mDescription;
  std::shared_ptr<const graphics::Image> mIcon;
};
/** A two-pane split with a draggable fraction. */
class SplitViewElement final : public Element {
 public:
  SplitViewElement(Axis axis, float fraction, ElementPtr first, ElementPtr second,
    std::function<void(float)> onFractionChanged = {}, std::string key = {})
    : Element(std::move(key), { std::move(first), std::move(second) }), mAxis(axis),
      mFraction(fraction), mOnFractionChanged(std::move(onFractionChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::SplitView; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::SplitView || key() != other.key()) return false;
    const auto& split = static_cast<const SplitViewElement&>(other);
    return mAxis == split.mAxis && mFraction == split.mFraction &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] Axis axis() const noexcept { return mAxis; }
  [[nodiscard]] float fraction() const noexcept { return mFraction; }
  [[nodiscard]] const ElementPtr& first() const noexcept { return children()[0]; }
  [[nodiscard]] const ElementPtr& second() const noexcept { return children()[1]; }
  [[nodiscard]] const std::function<void(float)>& onFractionChanged() const noexcept {
    return mOnFractionChanged;
  }

 private:
  Axis mAxis;
  float mFraction;
  std::function<void(float)> mOnFractionChanged;
};
/** One tab: a label, a content element and an identity key. */
struct TabItem {
  std::string label;
  ElementPtr content;
  std::string key;
};

/** A tab bar with one content slot per tab. */
class TabBarElement final : public Element {
 public:
  TabBarElement(std::shared_ptr<const graphics::Font> font, std::vector<TabItem> tabs,
    int active = 0, std::function<void(int)> onChanged = {}, std::string key = {})
    : Element(std::move(key), contentsOf(tabs)), mFont(std::move(font)), mTabs(std::move(tabs)),
      mActive(active), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TabBar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TabBar || key() != other.key()) return false;
    const auto& bar = static_cast<const TabBarElement&>(other);
    if (mActive != bar.mActive || mFont != bar.mFont || mTabs.size() != bar.mTabs.size()) {
      return false;
    }
    for (size_t i = 0; i < mTabs.size(); ++i) {
      if (mTabs[i].label != bar.mTabs[i].label || mTabs[i].key != bar.mTabs[i].key) return false;
    }
    return detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<TabItem>& tabs() const noexcept { return mTabs; }
  [[nodiscard]] int active() const noexcept { return mActive; }
  [[nodiscard]] const std::function<void(int)>& onChanged() const noexcept { return mOnChanged; }

 private:
  static std::vector<ElementPtr> contentsOf(const std::vector<TabItem>& tabs) {
    std::vector<ElementPtr> content;
    content.reserve(tabs.size());
    for (const TabItem& tab: tabs) content.push_back(tab.content);
    return content;
  }

  std::shared_ptr<const graphics::Font> mFont;
  std::vector<TabItem> mTabs;
  int mActive;
  std::function<void(int)> mOnChanged;
};
/** One toolbar entry: a control, a separator, or a virtualized overflow item. */
struct ToolbarItem {
  ElementPtr element;
  bool flexible = false;
  bool separator = false;
  std::string overflowLabel;
  std::function<void()> overflowAction;
  std::string key;
};

/** A horizontal toolbar; items may be flexible or overflow into a menu. */
class ToolbarElement final : public Element {
 public:
  ToolbarElement(std::shared_ptr<const graphics::Font> font, std::vector<ToolbarItem> items,
    float height = 44.0f, std::string key = {})
    : Element(std::move(key), contentsOf(items)), mFont(std::move(font)), mItems(std::move(items)),
      mHeight(height) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Toolbar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Toolbar || key() != other.key()) return false;
    const auto& toolbar = static_cast<const ToolbarElement&>(other);
    if (mFont != toolbar.mFont || mHeight != toolbar.mHeight ||
        mItems.size() != toolbar.mItems.size()) {
      return false;
    }
    for (size_t i = 0; i < mItems.size(); ++i) {
      if (mItems[i].flexible != toolbar.mItems[i].flexible ||
          mItems[i].separator != toolbar.mItems[i].separator ||
          mItems[i].key != toolbar.mItems[i].key ||
          mItems[i].overflowLabel != toolbar.mItems[i].overflowLabel) {
        return false;
      }
    }
    return detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<ToolbarItem>& items() const noexcept { return mItems; }
  [[nodiscard]] float height() const noexcept { return mHeight; }

 private:
  static std::vector<ElementPtr> contentsOf(const std::vector<ToolbarItem>& items) {
    std::vector<ElementPtr> content;
    content.reserve(items.size());
    for (const ToolbarItem& item: items) content.push_back(item.element);
    return content;
  }

  std::shared_ptr<const graphics::Font> mFont;
  std::vector<ToolbarItem> mItems;
  float mHeight;
};
/** A left/right status strip with leading/trailing item slots. */
class StatusBarElement final : public Element {
 public:
  StatusBarElement(std::shared_ptr<const graphics::Font> font, std::string left = {},
    std::string right = {}, std::vector<ElementPtr> leading = {},
    std::vector<ElementPtr> trailing = {}, std::string key = {})
    : Element(std::move(key), join(leading, trailing)), mFont(std::move(font)),
      mLeft(std::move(left)), mRight(std::move(right)), mLeadingCount(leading.size()),
      mTrailingCount(trailing.size()) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::StatusBar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::StatusBar || key() != other.key()) return false;
    const auto& bar = static_cast<const StatusBarElement&>(other);
    return mLeft == bar.mLeft && mRight == bar.mRight && mFont == bar.mFont &&
      mLeadingCount == bar.mLeadingCount && mTrailingCount == bar.mTrailingCount &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& leftText() const noexcept { return mLeft; }
  [[nodiscard]] const std::string& rightText() const noexcept { return mRight; }
  [[nodiscard]] size_t leadingCount() const noexcept { return mLeadingCount; }
  [[nodiscard]] size_t trailingCount() const noexcept { return mTrailingCount; }

 private:
  static std::vector<ElementPtr> join(const std::vector<ElementPtr>& leading,
      const std::vector<ElementPtr>& trailing) {
    std::vector<ElementPtr> items = leading;
    items.insert(items.end(), trailing.begin(), trailing.end());
    return items;
  }

  std::shared_ptr<const graphics::Font> mFont;
  std::string mLeft;
  std::string mRight;
  size_t mLeadingCount;
  size_t mTrailingCount;
};
/** A dropdown select. */
class SelectElement final : public Element {
 public:
  SelectElement(std::vector<std::string> options, std::shared_ptr<const graphics::Font> font,
    int selected = -1, std::string placeholder = {}, std::function<void(int)> onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mOptions(std::move(options)), mFont(std::move(font)),
      mSelected(selected), mPlaceholder(std::move(placeholder)), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Select; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Select || key() != other.key()) return false;
    const auto& select = static_cast<const SelectElement&>(other);
    return mOptions == select.mOptions && mFont == select.mFont && mSelected == select.mSelected &&
      mPlaceholder == select.mPlaceholder;
  }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::string& placeholder() const noexcept { return mPlaceholder; }
  [[nodiscard]] const std::function<void(int)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mOptions;
  std::shared_ptr<const graphics::Font> mFont;
  int mSelected;
  std::string mPlaceholder;
  std::function<void(int)> mOnChanged;
};

/** An editable combo box. */
class ComboBoxElement final : public Element {
 public:
  using ChangeHandler = std::function<void(int, std::string_view)>;

  ComboBoxElement(std::vector<std::string> options, std::shared_ptr<const graphics::Font> font,
    std::string text = {}, int selected = -1, std::string placeholder = {},
    ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mOptions(std::move(options)), mFont(std::move(font)),
      mText(std::move(text)), mSelected(selected), mPlaceholder(std::move(placeholder)),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ComboBox; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ComboBox || key() != other.key()) return false;
    const auto& combo = static_cast<const ComboBoxElement&>(other);
    return mOptions == combo.mOptions && mFont == combo.mFont && mText == combo.mText &&
      mSelected == combo.mSelected && mPlaceholder == combo.mPlaceholder;
  }
  [[nodiscard]] const std::vector<std::string>& options() const noexcept { return mOptions; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::string& placeholder() const noexcept { return mPlaceholder; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mOptions;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mText;
  int mSelected;
  std::string mPlaceholder;
  ChangeHandler mOnChanged;
};

/** A text field with a search affordance. */
class SearchFieldElement final : public Element {
 public:
  SearchFieldElement(std::string placeholder, std::shared_ptr<const graphics::Font> font,
    std::string text = {}, std::function<void(const std::string&)> onChanged = {},
    std::function<void(const std::string&)> onSearch = {}, std::string key = {})
    : Element(std::move(key)), mPlaceholder(std::move(placeholder)), mFont(std::move(font)),
      mText(std::move(text)), mOnChanged(std::move(onChanged)), mOnSearch(std::move(onSearch)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::SearchField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::SearchField || key() != other.key()) return false;
    const auto& search = static_cast<const SearchFieldElement&>(other);
    return mPlaceholder == search.mPlaceholder && mFont == search.mFont &&
      mText == search.mText;
  }
  [[nodiscard]] const std::string& placeholder() const noexcept { return mPlaceholder; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::function<void(const std::string&)>& onChanged() const noexcept {
    return mOnChanged;
  }
  [[nodiscard]] const std::function<void(const std::string&)>& onSearch() const noexcept {
    return mOnSearch;
  }

 private:
  std::string mPlaceholder;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mText;
  std::function<void(const std::string&)> mOnChanged;
  std::function<void(const std::string&)> mOnSearch;
};

/** A masked text field. */
class PasswordFieldElement final : public Element {
 public:
  PasswordFieldElement(std::string placeholder, std::shared_ptr<const graphics::Font> font,
    std::string text = {}, size_t maxLength = 0,
    std::function<void(const std::string&)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mPlaceholder(std::move(placeholder)), mFont(std::move(font)),
      mText(std::move(text)), mMaxLength(maxLength), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::PasswordField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::PasswordField || key() != other.key()) return false;
    const auto& password = static_cast<const PasswordFieldElement&>(other);
    return mPlaceholder == password.mPlaceholder && mFont == password.mFont &&
      mText == password.mText && mMaxLength == password.mMaxLength;
  }
  [[nodiscard]] const std::string& placeholder() const noexcept { return mPlaceholder; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] size_t maxLength() const noexcept { return mMaxLength; }
  [[nodiscard]] const std::function<void(const std::string&)>& onChanged() const noexcept {
    return mOnChanged;
  }

 private:
  std::string mPlaceholder;
  std::shared_ptr<const graphics::Font> mFont;
  std::string mText;
  size_t mMaxLength;
  std::function<void(const std::string&)> mOnChanged;
};

/** A formatted numeric field. */
class NumberFieldElement final : public Element {
 public:
  NumberFieldElement(std::shared_ptr<const graphics::Font> font, double minimum, double maximum,
    double value, double step = 1.0, int decimals = 0,
    std::function<void(double)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mMinimum(minimum), mMaximum(maximum),
      mValue(value), mStep(step), mDecimals(decimals), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::NumberField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::NumberField || key() != other.key()) return false;
    const auto& field = static_cast<const NumberFieldElement&>(other);
    return mFont == field.mFont && mMinimum == field.mMinimum && mMaximum == field.mMaximum &&
      mValue == field.mValue && mStep == field.mStep && mDecimals == field.mDecimals;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] double minimum() const noexcept { return mMinimum; }
  [[nodiscard]] double maximum() const noexcept { return mMaximum; }
  [[nodiscard]] double value() const noexcept { return mValue; }
  [[nodiscard]] double step() const noexcept { return mStep; }
  [[nodiscard]] int decimals() const noexcept { return mDecimals; }
  [[nodiscard]] const std::function<void(double)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  double mMinimum;
  double mMaximum;
  double mValue;
  double mStep;
  int mDecimals;
  std::function<void(double)> mOnChanged;
};

/** A token/tag editor. */
class TagInputElement final : public Element {
 public:
  TagInputElement(std::shared_ptr<const graphics::Font> font, std::vector<std::string> tokens,
    std::string placeholder = "Add tag",
    std::function<void(const std::vector<std::string>&)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mTokens(std::move(tokens)),
      mPlaceholder(std::move(placeholder)), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TagInput; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TagInput || key() != other.key()) return false;
    const auto& tags = static_cast<const TagInputElement&>(other);
    return mFont == tags.mFont && mTokens == tags.mTokens && mPlaceholder == tags.mPlaceholder;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<std::string>& tokens() const noexcept { return mTokens; }
  [[nodiscard]] const std::string& placeholder() const noexcept { return mPlaceholder; }
  [[nodiscard]] const std::function<void(const std::vector<std::string>&)>& onChanged() const noexcept {
    return mOnChanged;
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  std::vector<std::string> mTokens;
  std::string mPlaceholder;
  std::function<void(const std::vector<std::string>&)> mOnChanged;
};

/** A colour field with a swatch. */
class ColorInputElement final : public Element {
 public:
  ColorInputElement(std::shared_ptr<const graphics::Font> font, Color color,
    std::function<void(Color)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mColor(color),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ColorInput; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ColorInput || key() != other.key()) return false;
    const auto& input = static_cast<const ColorInputElement&>(other);
    return mFont == input.mFont && mColor == input.mColor;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] Color color() const noexcept { return mColor; }
  [[nodiscard]] const std::function<void(Color)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  Color mColor;
  std::function<void(Color)> mOnChanged;
};
/** A vertical activity timeline. */
class TimelineElement final : public Element {
 public:
  TimelineElement(std::vector<TimelineEntry> entries, std::shared_ptr<const graphics::Font> font,
    std::string key = {})
    : Element(std::move(key)), mEntries(std::move(entries)), mFont(std::move(font)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Timeline; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Timeline || key() != other.key()) return false;
    const auto& timeline = static_cast<const TimelineElement&>(other);
    if (mFont != timeline.mFont || mEntries.size() != timeline.mEntries.size()) return false;
    for (size_t i = 0; i < mEntries.size(); ++i) {
      const TimelineEntry& a = mEntries[i];
      const TimelineEntry& b = timeline.mEntries[i];
      if (a.title != b.title || a.detail != b.detail || a.time != b.time || a.state != b.state) {
        return false;
      }
    }
    return true;
  }
  [[nodiscard]] const std::vector<TimelineEntry>& entries() const noexcept { return mEntries; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }

 private:
  std::vector<TimelineEntry> mEntries;
  std::shared_ptr<const graphics::Font> mFont;
};

/** A palette/HSV colour picker. */
class ColorPickerElement final : public Element {
 public:
  ColorPickerElement(std::vector<Color> palette, ColorPickerMode mode, Color color, float alpha,
    int selected, std::function<void(Color)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mPalette(std::move(palette)), mMode(mode), mColor(color),
      mAlpha(alpha), mSelected(selected), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ColorPicker; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ColorPicker || key() != other.key()) return false;
    const auto& picker = static_cast<const ColorPickerElement&>(other);
    return mPalette == picker.mPalette && mMode == picker.mMode && mColor == picker.mColor &&
      mAlpha == picker.mAlpha && mSelected == picker.mSelected;
  }
  [[nodiscard]] const std::vector<Color>& palette() const noexcept { return mPalette; }
  [[nodiscard]] ColorPickerMode mode() const noexcept { return mMode; }
  [[nodiscard]] Color color() const noexcept { return mColor; }
  [[nodiscard]] float alpha() const noexcept { return mAlpha; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::function<void(Color)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<Color> mPalette;
  ColorPickerMode mMode;
  Color mColor;
  float mAlpha;
  int mSelected;
  std::function<void(Color)> mOnChanged;
};

/** An IME candidate list. */
class CandidateListViewElement final : public Element {
 public:
  CandidateListViewElement(std::vector<std::string> candidates, size_t selected,
    std::shared_ptr<const graphics::Font> font, std::string key = {})
    : Element(std::move(key)), mCandidates(std::move(candidates)), mSelected(selected),
      mFont(std::move(font)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override {
    return ElementKind::CandidateListView;
  }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::CandidateListView || key() != other.key()) return false;
    const auto& list = static_cast<const CandidateListViewElement&>(other);
    return mCandidates == list.mCandidates && mSelected == list.mSelected &&
      mFont == list.mFont;
  }
  [[nodiscard]] const std::vector<std::string>& candidates() const noexcept { return mCandidates; }
  [[nodiscard]] size_t selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }

 private:
  std::vector<std::string> mCandidates;
  size_t mSelected;
  std::shared_ptr<const graphics::Font> mFont;
};

/** A scrolling table view with row activation. */
class TableViewElement final : public Element {
 public:
  TableViewElement(std::vector<TableColumn> columns,
    std::vector<std::vector<std::shared_ptr<coral::ui::Layer>>> rows,
    std::shared_ptr<const graphics::Font> font, float gap = 0.0f, Insets padding = {},
    int selected = -1,
    std::function<void(int)> onRowActivated = {}, std::string key = {})
    : Element(std::move(key)), mColumns(std::move(columns)), mRows(std::move(rows)),
      mFont(std::move(font)), mGap(gap), mPadding(padding), mSelected(selected),
      mOnRowActivated(std::move(onRowActivated)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TableView; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TableView || key() != other.key()) return false;
    const auto& table = static_cast<const TableViewElement&>(other);
    if (mFont != table.mFont || mGap != table.mGap || mSelected != table.mSelected ||
        mColumns.size() != table.mColumns.size() || mRows.size() != table.mRows.size() ||
        !detail::sameInsets(mPadding, table.mPadding)) {
      return false;
    }
    for (size_t c = 0; c < mColumns.size(); ++c) {
      if (mColumns[c].width != table.mColumns[c].width ||
          mColumns[c].weight != table.mColumns[c].weight ||
          mColumns[c].percent != table.mColumns[c].percent) {
        return false;
      }
    }
    for (size_t r = 0; r < mRows.size(); ++r) {
      if (mRows[r].size() != table.mRows[r].size()) return false;
      for (size_t c = 0; c < mRows[r].size(); ++c) {
        if (mRows[r][c] != table.mRows[r][c]) return false;
      }
    }
    return true;
  }
  [[nodiscard]] const std::vector<TableColumn>& columns() const noexcept { return mColumns; }
  [[nodiscard]] const std::vector<std::vector<std::shared_ptr<Layer>>>& rows() const noexcept {
    return mRows;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] float gap() const noexcept { return mGap; }
  [[nodiscard]] const Insets& padding() const noexcept { return mPadding; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] const std::function<void(int)>& onRowActivated() const noexcept {
    return mOnRowActivated;
  }

 private:
  std::vector<TableColumn> mColumns;
  std::vector<std::vector<std::shared_ptr<Layer>>> mRows;
  std::shared_ptr<const graphics::Font> mFont;
  float mGap;
  Insets mPadding;
  int mSelected;
  std::function<void(int)> mOnRowActivated;
};
/** A date field with calendar popup. */
class DateFieldElement final : public Element {
 public:
  DateFieldElement(std::shared_ptr<const graphics::Font> font,
    std::chrono::year_month_day date = {}, std::function<void(std::chrono::year_month_day)> onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mDate(date), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::DateField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::DateField || key() != other.key()) return false;
    const auto& field = static_cast<const DateFieldElement&>(other);
    return mFont == field.mFont && mDate == field.mDate;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] std::chrono::year_month_day date() const noexcept { return mDate; }
  [[nodiscard]] const std::function<void(std::chrono::year_month_day)>& onChanged() const noexcept {
    return mOnChanged;
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  std::chrono::year_month_day mDate;
  std::function<void(std::chrono::year_month_day)> mOnChanged;
};

/** A time field with picker popup. */
class TimeFieldElement final : public Element {
 public:
  TimeFieldElement(std::shared_ptr<const graphics::Font> font, int hour = 0, int minute = 0,
    std::optional<bool> use24HourClock = std::nullopt, std::function<void(int, int)> onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mHour(hour), mMinute(minute),
      mUse24Hour(use24HourClock), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TimeField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TimeField || key() != other.key()) return false;
    const auto& field = static_cast<const TimeFieldElement&>(other);
    return mFont == field.mFont && mHour == field.mHour && mMinute == field.mMinute &&
      mUse24Hour == field.mUse24Hour;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int hour() const noexcept { return mHour; }
  [[nodiscard]] int minute() const noexcept { return mMinute; }
  [[nodiscard]] const std::optional<bool>& use24HourClock() const noexcept { return mUse24Hour; }
  [[nodiscard]] const std::function<void(int, int)>& onChanged() const noexcept { return mOnChanged; }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  int mHour;
  int mMinute;
  std::optional<bool> mUse24Hour;
  std::function<void(int, int)> mOnChanged;
};

/** A combined date + time field. */
class DateTimeFieldElement final : public Element {
 public:
  DateTimeFieldElement(std::shared_ptr<const graphics::Font> font,
    std::chrono::year_month_day date = {}, int hour = 0, int minute = 0,
    std::function<void(std::chrono::year_month_day, int, int)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mDate(date), mHour(hour), mMinute(minute),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::DateTimeField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::DateTimeField || key() != other.key()) return false;
    const auto& field = static_cast<const DateTimeFieldElement&>(other);
    return mFont == field.mFont && mDate == field.mDate && mHour == field.mHour &&
      mMinute == field.mMinute;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] std::chrono::year_month_day date() const noexcept { return mDate; }
  [[nodiscard]] int hour() const noexcept { return mHour; }
  [[nodiscard]] int minute() const noexcept { return mMinute; }
  [[nodiscard]] const std::function<void(std::chrono::year_month_day, int, int)>& onChanged()
    const noexcept {
    return mOnChanged;
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  std::chrono::year_month_day mDate;
  int mHour;
  int mMinute;
  std::function<void(std::chrono::year_month_day, int, int)> mOnChanged;
};

/** A date range field. */
class DateRangeFieldElement final : public Element {
 public:
  DateRangeFieldElement(std::shared_ptr<const graphics::Font> font,
    std::chrono::year_month_day start = {}, std::chrono::year_month_day end = {},
    std::function<void(std::chrono::year_month_day, std::chrono::year_month_day)> onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mStart(start), mEnd(end),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::DateRangeField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::DateRangeField || key() != other.key()) return false;
    const auto& field = static_cast<const DateRangeFieldElement&>(other);
    return mFont == field.mFont && mStart == field.mStart && mEnd == field.mEnd;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] std::chrono::year_month_day start() const noexcept { return mStart; }
  [[nodiscard]] std::chrono::year_month_day end() const noexcept { return mEnd; }
  [[nodiscard]] const std::function<void(std::chrono::year_month_day, std::chrono::year_month_day)>&
    onChanged() const noexcept {
    return mOnChanged;
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  std::chrono::year_month_day mStart;
  std::chrono::year_month_day mEnd;
  std::function<void(std::chrono::year_month_day, std::chrono::year_month_day)> mOnChanged;
};

/** A time range field. */
class TimeRangeFieldElement final : public Element {
 public:
  TimeRangeFieldElement(std::shared_ptr<const graphics::Font> font, int startHour = 0,
    int startMinute = 0, int endHour = 1, int endMinute = 0,
    std::function<void(int, int, int, int)> onChanged = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mStartHour(startHour),
      mStartMinute(startMinute), mEndHour(endHour), mEndMinute(endMinute),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TimeRangeField; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TimeRangeField || key() != other.key()) return false;
    const auto& field = static_cast<const TimeRangeFieldElement&>(other);
    return mFont == field.mFont && mStartHour == field.mStartHour &&
      mStartMinute == field.mStartMinute && mEndHour == field.mEndHour &&
      mEndMinute == field.mEndMinute;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int startHour() const noexcept { return mStartHour; }
  [[nodiscard]] int startMinute() const noexcept { return mStartMinute; }
  [[nodiscard]] int endHour() const noexcept { return mEndHour; }
  [[nodiscard]] int endMinute() const noexcept { return mEndMinute; }
  [[nodiscard]] const std::function<void(int, int, int, int)>& onChanged() const noexcept {
    return mOnChanged;
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  int mStartHour;
  int mStartMinute;
  int mEndHour;
  int mEndMinute;
  std::function<void(int, int, int, int)> mOnChanged;
};

/** A compact time picker. */
class TimePickerElement final : public Element {
 public:
  TimePickerElement(std::shared_ptr<const graphics::Font> font, int hour, int minute,
    bool use24HourClock = true, std::function<void(int, int)> onChanged = {},
    std::function<void()> onCancel = {}, std::function<void()> onDone = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mHour(hour), mMinute(minute),
      mUse24Hour(use24HourClock), mOnChanged(std::move(onChanged)), mOnCancel(std::move(onCancel)),
      mOnDone(std::move(onDone)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TimePicker; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TimePicker || key() != other.key()) return false;
    const auto& picker = static_cast<const TimePickerElement&>(other);
    return mFont == picker.mFont && mHour == picker.mHour && mMinute == picker.mMinute &&
      mUse24Hour == picker.mUse24Hour;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int hour() const noexcept { return mHour; }
  [[nodiscard]] int minute() const noexcept { return mMinute; }
  [[nodiscard]] bool use24HourClock() const noexcept { return mUse24Hour; }
  [[nodiscard]] const std::function<void(int, int)>& onChanged() const noexcept { return mOnChanged; }
  [[nodiscard]] const std::function<void()>& onCancel() const noexcept { return mOnCancel; }
  [[nodiscard]] const std::function<void()>& onDone() const noexcept { return mOnDone; }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  int mHour;
  int mMinute;
  bool mUse24Hour;
  std::function<void(int, int)> mOnChanged;
  std::function<void()> mOnCancel;
  std::function<void()> mOnDone;
};
/** One node in a declarative tree. */
struct TreeNodeSpec {
  std::string label;
  std::string detail;
  bool folder = false;
  bool expanded = true;
  std::shared_ptr<const graphics::Image> icon;
  std::vector<TreeNodeSpec> children;
};

/** A hierarchical tree with stable path-based selection. */
class TreeViewElement final : public Element {
 public:
  using PathHandler = std::function<void(const std::vector<std::size_t>&, std::string_view)>;

  TreeViewElement(std::shared_ptr<const graphics::Font> font, std::vector<TreeNodeSpec> roots,
    float rowHeight = 28.0f, std::vector<std::size_t> selected = {}, PathHandler onSelected = {},
    PathHandler onActivated = {}, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mRoots(std::move(roots)),
      mRowHeight(rowHeight), mSelected(std::move(selected)), mOnSelected(std::move(onSelected)),
      mOnActivated(std::move(onActivated)), mPaths(buildPaths(mRoots)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TreeView; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TreeView || key() != other.key()) return false;
    const auto& tree = static_cast<const TreeViewElement&>(other);
    return mFont == tree.mFont && mRowHeight == tree.mRowHeight && mSelected == tree.mSelected &&
      sameNodes(mRoots, tree.mRoots);
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const std::vector<TreeNodeSpec>& roots() const noexcept { return mRoots; }
  [[nodiscard]] float rowHeight() const noexcept { return mRowHeight; }
  [[nodiscard]] const std::vector<std::size_t>& selected() const noexcept { return mSelected; }
  [[nodiscard]] int selectedId() const noexcept {
    for (std::size_t i = 0; i < mPaths->size(); ++i) {
      if ((*mPaths)[i] == mSelected) return static_cast<int>(i);
    }
    return -1;
  }
  [[nodiscard]] const std::shared_ptr<const std::vector<std::vector<std::size_t>>>& paths()
    const noexcept {
    return mPaths;
  }
  [[nodiscard]] const PathHandler& onSelected() const noexcept { return mOnSelected; }
  [[nodiscard]] const PathHandler& onActivated() const noexcept { return mOnActivated; }

 private:
  static bool sameNodes(const std::vector<TreeNodeSpec>& a, const std::vector<TreeNodeSpec>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (a[i].label != b[i].label || a[i].detail != b[i].detail ||
          a[i].folder != b[i].folder || a[i].expanded != b[i].expanded ||
          a[i].icon != b[i].icon || !sameNodes(a[i].children, b[i].children)) {
        return false;
      }
    }
    return true;
  }

  static std::shared_ptr<const std::vector<std::vector<std::size_t>>> buildPaths(
      const std::vector<TreeNodeSpec>& roots) {
    auto paths = std::make_shared<std::vector<std::vector<std::size_t>>>();
    const auto walk = [&paths](const auto& self, const std::vector<TreeNodeSpec>& nodes,
                         const std::vector<std::size_t>& prefix) -> void {
      for (std::size_t i = 0; i < nodes.size(); ++i) {
        std::vector<std::size_t> path = prefix;
        path.push_back(i);
        paths->push_back(path);
        self(self, nodes[i].children, path);
      }
    };
    walk(walk, roots, {});
    return paths;
  }

  std::shared_ptr<const graphics::Font> mFont;
  std::vector<TreeNodeSpec> mRoots;
  float mRowHeight;
  std::vector<std::size_t> mSelected;
  PathHandler mOnSelected;
  PathHandler mOnActivated;
  std::shared_ptr<const std::vector<std::vector<std::size_t>>> mPaths;
};
/** A flex container; the element-level equivalent of `ui::Flex`. */
class StackElement final : public Element {
 public:
  StackElement(Axis axis, std::vector<ElementPtr> children, std::string key = {},
    float gap = 0.0f, Insets padding = {}, Align crossAlign = Align::Start)
    : Element(std::move(key), std::move(children)), mAxis(axis), mGap(gap), mPadding(padding),
      mCrossAlign(crossAlign) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Stack; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Stack || key() != other.key()) return false;
    const auto& stack = static_cast<const StackElement&>(other);
    return mAxis == stack.mAxis && mGap == stack.mGap && mCrossAlign == stack.mCrossAlign &&
      detail::sameInsets(mPadding, stack.mPadding) &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] Axis axis() const noexcept { return mAxis; }
  [[nodiscard]] float gap() const noexcept { return mGap; }
  [[nodiscard]] const Insets& padding() const noexcept { return mPadding; }
  [[nodiscard]] Align crossAlign() const noexcept { return mCrossAlign; }

 private:
  Axis mAxis;
  float mGap;
  Insets mPadding;
  Align mCrossAlign;
};

/**
 * An equal-column grid of children (row-major, each row as tall as its tallest child).
 *
 * The layout primitive an inspector/settings form needs; spans stay available on the imperative
 * `Grid` for controls that need them.
 */
class GridElement final : public Element {
 public:
  GridElement(int columns, std::vector<ElementPtr> children, std::string key = {},
    float gap = 8.0f, Insets padding = {})
    : Element(std::move(key), std::move(children)), mColumns(columns), mGap(gap),
      mPadding(padding) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Grid; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Grid || key() != other.key()) return false;
    const auto& grid = static_cast<const GridElement&>(other);
    return mColumns == grid.mColumns && mGap == grid.mGap &&
      detail::sameInsets(mPadding, grid.mPadding) &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] int columns() const noexcept { return mColumns; }
  [[nodiscard]] float gap() const noexcept { return mGap; }
  [[nodiscard]] const Insets& padding() const noexcept { return mPadding; }

 private:
  int mColumns;
  float mGap;
  Insets mPadding;
};

/**
 * A four-region dock layout (left/centre/right/bottom).
 *
 * Each region holds one optional branch; sizes apply to the outer regions. The dock layer survives
 * updates, and a region is patched in place until its branch identity changes.
 */
class DockElement final : public Element {
 public:
  DockElement(ElementPtr center, ElementPtr left = {}, ElementPtr right = {}, ElementPtr bottom = {},
    float leftWidth = 200.0f, float rightWidth = 240.0f, float bottomHeight = 180.0f,
    std::string key = {})
    : Element(std::move(key)), mCenter(std::move(center)), mLeft(std::move(left)),
      mRight(std::move(right)), mBottom(std::move(bottom)), mLeftWidth(leftWidth),
      mRightWidth(rightWidth), mBottomHeight(bottomHeight) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Dock; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Dock || key() != other.key()) return false;
    const auto& dock = static_cast<const DockElement&>(other);
    const auto same = [](const ElementPtr& left, const ElementPtr& right) {
      if (left == nullptr || right == nullptr) return left == right;
      return left->measureEquivalent(*right);
    };
    return mLeftWidth == dock.mLeftWidth && mRightWidth == dock.mRightWidth &&
      mBottomHeight == dock.mBottomHeight && same(mCenter, dock.mCenter) && same(mLeft, dock.mLeft) &&
      same(mRight, dock.mRight) && same(mBottom, dock.mBottom);
  }
  [[nodiscard]] const ElementPtr& center() const noexcept { return mCenter; }
  [[nodiscard]] const ElementPtr& left() const noexcept { return mLeft; }
  [[nodiscard]] const ElementPtr& right() const noexcept { return mRight; }
  [[nodiscard]] const ElementPtr& bottom() const noexcept { return mBottom; }
  [[nodiscard]] float leftWidth() const noexcept { return mLeftWidth; }
  [[nodiscard]] float rightWidth() const noexcept { return mRightWidth; }
  [[nodiscard]] float bottomHeight() const noexcept { return mBottomHeight; }

 private:
  ElementPtr mCenter;
  ElementPtr mLeft;
  ElementPtr mRight;
  ElementPtr mBottom;
  float mLeftWidth;
  float mRightWidth;
  float mBottomHeight;
};

/**
 * A conditional branch.
 *
 * The reconciler mounts exactly one branch; when the condition or a branch's key changes the old
 * branch layer is replaced. This is the element-tree equivalent of an `if` in a declarative build
 * function, without putting a null child in a container.
 */
class WhenElement final : public Element {
 public:
  WhenElement(bool condition, ElementPtr thenBranch, ElementPtr elseBranch = {}, std::string key = {})
    : Element(std::move(key)), mCondition(condition), mThen(std::move(thenBranch)),
      mElse(std::move(elseBranch)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::When; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::When || key() != other.key()) return false;
    const auto& when = static_cast<const WhenElement&>(other);
    if (mCondition != when.mCondition) return false;
    const auto branchEqual = [](const ElementPtr& left, const ElementPtr& right) {
      if (left == nullptr || right == nullptr) return left == right;
      return left->measureEquivalent(*right);
    };
    return branchEqual(mThen, when.mThen) && branchEqual(mElse, when.mElse);
  }
  [[nodiscard]] bool condition() const noexcept { return mCondition; }
  [[nodiscard]] const ElementPtr& thenBranch() const noexcept { return mThen; }
  [[nodiscard]] const ElementPtr& elseBranch() const noexcept { return mElse; }

 private:
  bool mCondition;
  ElementPtr mThen;
  ElementPtr mElse;
};

/**
 * A function element.
 *
 * `build` runs during reconciliation; it may read a view model and return a new subtree. The
 * component's own key/identity is the element key, so an outer keyed list can preserve the
 * component's layer while its internals are patched.
 */
class ComponentElement final : public Element {
 public:
  using Builder = std::function<ElementPtr()>;

  ComponentElement(Builder build, std::string key = {})
    : Element(std::move(key)), mBuild(std::move(build)) {
    if (mBuild) mBuilt = mBuild();
  }

  /**
   * A reconciler-owned component node.
   *
   * The component instance and its context are stored here; the reconciler calls `onMount` once,
   * `onUpdate` on later patches and `onUnmount` when the node leaves the tree, and asks the component
   * for a fresh subtree on every patch. `context` is created by the parent component's
   * `ComponentContext::makeChild()` so it inherits the providers.
   */
  ComponentElement(std::shared_ptr<Component> component, std::shared_ptr<ComponentContext> context,
    std::string key = {})
    : Element(std::move(key)), mComponent(std::move(component)), mContext(std::move(context)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Component; }
  [[nodiscard]] const Builder& builder() const noexcept { return mBuild; }
  [[nodiscard]] const ElementPtr& built() const noexcept { return mBuilt; }
  [[nodiscard]] bool ownsComponent() const noexcept { return mComponent != nullptr; }
  [[nodiscard]] const std::shared_ptr<Component>& component() const noexcept { return mComponent; }
  [[nodiscard]] const std::shared_ptr<ComponentContext>& context() const noexcept {
    return mContext;
  }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Component || key() != other.key()) return false;
    const auto& component = static_cast<const ComponentElement&>(other);
    if (ownsComponent() || component.ownsComponent()) return mComponent == component.mComponent;
    if (mBuilt == nullptr || component.mBuilt == nullptr) return mBuilt == component.mBuilt;
    return mBuilt->measureEquivalent(*component.mBuilt);
  }

 private:
  Builder mBuild;
  ElementPtr mBuilt;
  std::shared_ptr<Component> mComponent;
  std::shared_ptr<ComponentContext> mContext;
};

/**
 * Applies layout/visibility/compositing props to a child without changing the child's content kind.
 *
 * The wrapper is transparent in the layer tree: the child's layer receives the props directly, so a
 * keyed `PropsElement` can stay mounted while its bounds or opacity change.
 */
class PropsElement final : public Element {
 public:
  PropsElement(ElementPtr child, ElementProps props, std::string key = {})
    : Element(std::move(key), child != nullptr ? std::vector<ElementPtr>{ std::move(child) }
                                               : std::vector<ElementPtr>{}),
      mProps(props) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Props; }
  [[nodiscard]] const ElementProps& props() const noexcept { return mProps; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Props || key() != other.key()) return false;
    const auto& props = static_cast<const PropsElement&>(other);
    if (!detail::sameElementProps(mProps, props.mProps)) return false;
    return detail::sameChildren(children(), props.children());
  }

 private:
  ElementProps mProps;
};



/** An icon-only button; the accessible name is separate from the pixels. */
class IconButtonElement final : public Element {
 public:
  using ClickHandler = std::function<void()>;

  IconButtonElement(std::shared_ptr<const graphics::Image> icon, std::string accessibleName,
    ClickHandler onClick = {}, std::string key = {})
    : Element(std::move(key)), mIcon(std::move(icon)), mAccessibleName(std::move(accessibleName)),
      mOnClick(std::move(onClick)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::IconButton; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::IconButton || key() != other.key()) return false;
    const auto& button = static_cast<const IconButtonElement&>(other);
    return mIcon == button.mIcon && mAccessibleName == button.mAccessibleName;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Image>& icon() const noexcept {
    return mIcon;
  }
  [[nodiscard]] const std::string& accessibleName() const noexcept { return mAccessibleName; }
  [[nodiscard]] const ClickHandler& onClick() const noexcept { return mOnClick; }

 private:
  std::shared_ptr<const graphics::Image> mIcon;
  std::string mAccessibleName;
  ClickHandler mOnClick;
};

/** An indeterminate busy indicator; `spinning` drives the window's animation loop. */
class SpinnerElement final : public Element {
 public:
  SpinnerElement(bool spinning, Color color, std::string key = {})
    : Element(std::move(key)), mSpinning(spinning), mColor(color) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Spinner; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Spinner || key() != other.key()) return false;
    const auto& spinner = static_cast<const SpinnerElement&>(other);
    return mSpinning == spinner.mSpinning && mColor == spinner.mColor;
  }
  [[nodiscard]] bool spinning() const noexcept { return mSpinning; }
  [[nodiscard]] Color color() const noexcept { return mColor; }

 private:
  bool mSpinning = false;
  Color mColor;
};
/** A colour swatch that behaves like a button; the accessible name is separate from the colour. */
class ColorButtonElement final : public Element {
 public:
  using ClickHandler = std::function<void()>;

  ColorButtonElement(Color color, std::string accessibleName, ClickHandler onClick = {},
    std::string key = {})
    : Element(std::move(key)), mColor(color), mAccessibleName(std::move(accessibleName)),
      mOnClick(std::move(onClick)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ColorButton; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ColorButton || key() != other.key()) return false;
    const auto& button = static_cast<const ColorButtonElement&>(other);
    return mColor == button.mColor && mAccessibleName == button.mAccessibleName;
  }
  [[nodiscard]] Color color() const noexcept { return mColor; }
  [[nodiscard]] const std::string& accessibleName() const noexcept { return mAccessibleName; }
  [[nodiscard]] const ClickHandler& onClick() const noexcept { return mOnClick; }

 private:
  Color mColor;
  std::string mAccessibleName;
  ClickHandler mOnClick;
};
/** A labelled checkbox; `checked` is the model snapshot and `onChanged` reports user toggles. */
class CheckBoxElement final : public Element {
 public:
  using ChangeHandler = std::function<void(bool)>;

  CheckBoxElement(std::string label, std::shared_ptr<const graphics::Font> font, bool checked,
    ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)),
      mChecked(checked), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::CheckBox; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::CheckBox || key() != other.key()) return false;
    const auto& box = static_cast<const CheckBoxElement&>(other);
    return mLabel == box.mLabel && mFont == box.mFont && mChecked == box.mChecked;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] bool checked() const noexcept { return mChecked; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  bool mChecked = false;
  ChangeHandler mOnChanged;
};

/** A labelled switch; same model contract as CheckBox. */
class ToggleElement final : public Element {
 public:
  using ChangeHandler = std::function<void(bool)>;

  ToggleElement(std::string label, std::shared_ptr<const graphics::Font> font, bool on,
    ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)), mOn(on),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Toggle; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Toggle || key() != other.key()) return false;
    const auto& toggle = static_cast<const ToggleElement&>(other);
    return mLabel == toggle.mLabel && mFont == toggle.mFont && mOn == toggle.mOn;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] bool on() const noexcept { return mOn; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  bool mOn = false;
  ChangeHandler mOnChanged;
};

/** One radio row; the group itself is the owner's concern (the layer reports a selection). */
class RadioButtonElement final : public Element {
 public:
  using SelectHandler = std::function<void()>;

  RadioButtonElement(std::string label, std::shared_ptr<const graphics::Font> font, bool selected,
    SelectHandler onSelected = {}, std::string key = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)),
      mSelected(selected), mOnSelected(std::move(onSelected)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::RadioButton; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::RadioButton || key() != other.key()) return false;
    const auto& radio = static_cast<const RadioButtonElement&>(other);
    return mLabel == radio.mLabel && mFont == radio.mFont && mSelected == radio.mSelected;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] bool selected() const noexcept { return mSelected; }
  [[nodiscard]] const SelectHandler& onSelected() const noexcept { return mOnSelected; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  bool mSelected = false;
  SelectHandler mOnSelected;
};


/** A segmented selector; `selected` is the model index and `onChanged` reports user picks. */
class SegmentedControlElement final : public Element {
 public:
  using ChangeHandler = std::function<void(int)>;

  SegmentedControlElement(std::vector<std::string> segments,
    std::shared_ptr<const graphics::Font> font, int selected, ChangeHandler onChanged = {},
    std::string key = {})
    : Element(std::move(key)), mSegments(std::move(segments)), mFont(std::move(font)),
      mSelected(selected), mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override {
    return ElementKind::SegmentedControl;
  }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::SegmentedControl || key() != other.key()) return false;
    const auto& control = static_cast<const SegmentedControlElement&>(other);
    return mSegments == control.mSegments && mFont == control.mFont &&
      mSelected == control.mSelected;
  }
  [[nodiscard]] const std::vector<std::string>& segments() const noexcept { return mSegments; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::vector<std::string> mSegments;
  std::shared_ptr<const graphics::Font> mFont;
  int mSelected = 0;
  ChangeHandler mOnChanged;
};

/**
 * A column-aligned table element.
 *
 * Cells are live layers, not nested elements: a table is a layout primitive over already-built
 * content (labels, images, editors), and the reconciler hands the rows to the `Table` layer directly.
 * Columns, rows, gap and padding are patched in place, so a data refresh keeps the layer identity.
 */
class TableElement final : public Element {
 public:
  TableElement(std::vector<TableColumn> columns,
    std::vector<std::vector<std::shared_ptr<Layer>>> rows, float gap = 8.0f,
    Insets padding = {}, std::string key = {})
    : Element(std::move(key)), mColumns(std::move(columns)), mRows(std::move(rows)), mGap(gap),
      mPadding(padding) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Table; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Table || key() != other.key()) return false;
    const auto& table = static_cast<const TableElement&>(other);
    if (mGap != table.mGap || !detail::sameInsets(mPadding, table.mPadding)) return false;
    if (mColumns.size() != table.mColumns.size()) return false;
    for (size_t index = 0; index < mColumns.size(); ++index) {
      if (mColumns[index].width != table.mColumns[index].width ||
          mColumns[index].weight != table.mColumns[index].weight) {
        return false;
      }
    }
    if (mRows.size() != table.mRows.size()) return false;
    for (size_t row = 0; row < mRows.size(); ++row) {
      if (mRows[row].size() != table.mRows[row].size()) return false;
      for (size_t column = 0; column < mRows[row].size(); ++column) {
        if (mRows[row][column] != table.mRows[row][column]) return false;
      }
    }
    return true;
  }
  [[nodiscard]] const std::vector<TableColumn>& columns() const noexcept { return mColumns; }
  [[nodiscard]] const std::vector<std::vector<std::shared_ptr<Layer>>>& rows() const noexcept {
    return mRows;
  }
  [[nodiscard]] float gap() const noexcept { return mGap; }
  [[nodiscard]] const Insets& padding() const noexcept { return mPadding; }

 private:
  std::vector<TableColumn> mColumns;
  std::vector<std::vector<std::shared_ptr<Layer>>> mRows;
  float mGap = 8.0f;
  Insets mPadding{};
};
/** A progress bar; `value` is a 0..1 fraction. */
class ProgressBarElement final : public Element {
 public:
  ProgressBarElement(std::shared_ptr<const graphics::Font> font, double value, std::string key = {})
    : Element(std::move(key)), mFont(std::move(font)), mValue(value) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ProgressBar; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ProgressBar || key() != other.key()) return false;
    const auto& bar = static_cast<const ProgressBarElement&>(other);
    return mFont == bar.mFont && mValue == bar.mValue;
  }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] double value() const noexcept { return mValue; }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  double mValue = 0.0;
};
/**
 * A clipped scroll viewport with one content child.
 *
 * `preferredHeight` is the viewport height a Flex parent should use; 0 means "fit the content up to
 * the available height". The child keeps its natural height and scrolls inside the viewport.
 */
class ScrollElement final : public Element {
 public:
  ScrollElement(ElementPtr content, float preferredHeight = 0.0f, std::string key = {})
    : Element(std::move(key),
        content != nullptr ? std::vector<ElementPtr>{ std::move(content) }
                           : std::vector<ElementPtr>{}),
      mPreferredHeight(preferredHeight) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Scroll; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Scroll || key() != other.key()) return false;
    const auto& scroll = static_cast<const ScrollElement&>(other);
    return mPreferredHeight == scroll.mPreferredHeight &&
      detail::sameChildren(children(), other.children());
  }
  [[nodiscard]] float preferredHeight() const noexcept { return mPreferredHeight; }

 private:
  float mPreferredHeight;
};

/**
 * Embeds a live `ContentHost` (a preview or video frame) in an element tree.
 *
 * The host is sampled zero-copy by the resulting `ContentView`; swapping the host updates the same
 * view, so focus/overlay children and layout are preserved.
 */
class ContentViewElement final : public Element {
 public:
  ContentViewElement(std::shared_ptr<ContentHost> host, Color placeholder = Color::MakeRGBA8(35, 38, 46, 255),
    std::string key = {})
    : Element(std::move(key)), mHost(std::move(host)), mPlaceholder(placeholder) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ContentView; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ContentView || key() != other.key()) return false;
    const auto& view = static_cast<const ContentViewElement&>(other);
    return mHost == view.mHost && mPlaceholder == view.mPlaceholder;
  }
  [[nodiscard]] const std::shared_ptr<ContentHost>& host() const noexcept { return mHost; }
  [[nodiscard]] Color placeholder() const noexcept { return mPlaceholder; }

 private:
  std::shared_ptr<ContentHost> mHost;
  Color mPlaceholder;
};

/**
 * A labelled numeric slider.
 *
 * `value` is the model-driven snapshot; drag/keyboard updates call `onChanged`, and an owner records
 * one edit through `onEditBegin`/`onEditEnd`.
 */
class SliderElement final : public Element {
 public:
  using ChangeHandler = std::function<void(double)>;

  SliderElement(std::string label, std::shared_ptr<const graphics::Font> font, double minimum,
    double maximum, double value, ChangeHandler onChanged = {}, std::string key = {},
    std::function<void()> onEditBegin = {}, std::function<void()> onEditEnd = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)),
      mMinimum(minimum), mMaximum(maximum), mValue(value), mOnChanged(std::move(onChanged)),
      mOnEditBegin(std::move(onEditBegin)), mOnEditEnd(std::move(onEditEnd)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Slider; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Slider || key() != other.key()) return false;
    const auto& slider = static_cast<const SliderElement&>(other);
    return mLabel == slider.mLabel && mFont == slider.mFont && mMinimum == slider.mMinimum &&
      mMaximum == slider.mMaximum && mValue == slider.mValue;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] double minimum() const noexcept { return mMinimum; }
  [[nodiscard]] double maximum() const noexcept { return mMaximum; }
  [[nodiscard]] double value() const noexcept { return mValue; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }
  [[nodiscard]] const std::function<void()>& onEditBegin() const noexcept { return mOnEditBegin; }
  [[nodiscard]] const std::function<void()>& onEditEnd() const noexcept { return mOnEditEnd; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  double mMinimum;
  double mMaximum;
  double mValue;
  ChangeHandler mOnChanged;
  std::function<void()> mOnEditBegin;
  std::function<void()> mOnEditEnd;
};

/**
 * Embeds a prebuilt `Layer` (a product widget) in an element tree.
 *
 * This is the escape hatch for widgets that do not have an element wrapper yet: the layer is used
 * as-is, so its own layout/state stays under the widget's control. A layer can only be mounted in
 * one place at a time.
 */
class LayerElement final : public Element {
 public:
  LayerElement(std::shared_ptr<Layer> layer, std::string key = {})
    : Element(std::move(key)), mLayer(std::move(layer)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Layer; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    return other.kind() == ElementKind::Layer && key() == other.key() &&
      mLayer == static_cast<const LayerElement&>(other).mLayer;
  }
  [[nodiscard]] const std::shared_ptr<Layer>& layer() const noexcept { return mLayer; }

 private:
  std::shared_ptr<Layer> mLayer;
};

/** A clickable button with a label. */
class ButtonElement final : public Element {
 public:
  using ClickHandler = std::function<void()>;
  using EnabledProperty = std::shared_ptr<observer::Property<bool>>;

  ButtonElement(std::string label, std::shared_ptr<const graphics::Font> font,
    ClickHandler onClick = {}, std::string key = {}, bool enabled = true,
    EnabledProperty enabledProperty = {})
    : Element(std::move(key)), mLabel(std::move(label)), mFont(std::move(font)),
      mOnClick(std::move(onClick)), mEnabled(enabled), mEnabledProperty(std::move(enabledProperty)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::Button; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::Button || key() != other.key()) return false;
    const auto& button = static_cast<const ButtonElement&>(other);
    return mLabel == button.mLabel && mFont == button.mFont && mEnabled == button.mEnabled &&
      mEnabledProperty == button.mEnabledProperty;
  }
  [[nodiscard]] const std::string& label() const noexcept { return mLabel; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const ClickHandler& onClick() const noexcept { return mOnClick; }
  [[nodiscard]] bool enabled() const noexcept { return mEnabled; }
  /** Optional live enabled state; when set, the reconciler subscribes and ignores the bool snapshot. */
  [[nodiscard]] const EnabledProperty& enabledProperty() const noexcept { return mEnabledProperty; }

 private:
  std::string mLabel;
  std::shared_ptr<const graphics::Font> mFont;
  ClickHandler mOnClick;
  bool mEnabled;
  EnabledProperty mEnabledProperty;
};

/** A single-line editable text field; changes are reported through `onChanged`. */
class TextInputElement final : public Element {
 public:
  using ChangeHandler = std::function<void(const std::string&)>;

  TextInputElement(std::string text, std::shared_ptr<const graphics::Font> font,
    ChangeHandler onChanged = {}, std::string key = {})
    : Element(std::move(key)), mText(std::move(text)), mFont(std::move(font)),
      mOnChanged(std::move(onChanged)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::TextInput; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::TextInput || key() != other.key()) return false;
    const auto& input = static_cast<const TextInputElement&>(other);
    return mText == input.mText && mFont == input.mFont;
  }
  [[nodiscard]] const std::string& text() const noexcept { return mText; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] const ChangeHandler& onChanged() const noexcept { return mOnChanged; }

 private:
  std::string mText;
  std::shared_ptr<const graphics::Font> mFont;
  ChangeHandler mOnChanged;
};

/** A virtualized list; rows stay strings, selection is reported by index. */
class ListViewElement final : public Element {
 public:
  using SelectionHandler = std::function<void(int, const std::string&)>;

  ListViewElement(std::vector<std::string> items, std::shared_ptr<const graphics::Font> font,
    float rowHeight = 30.0f, SelectionHandler onSelection = {}, int selected = -1,
    std::string key = {})
    : Element(std::move(key)), mItems(std::move(items)), mFont(std::move(font)),
      mRowHeight(rowHeight), mOnSelection(std::move(onSelection)), mSelected(selected) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::ListView; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::ListView || key() != other.key()) return false;
    const auto& list = static_cast<const ListViewElement&>(other);
    return mItems == list.mItems && mFont == list.mFont && mRowHeight == list.mRowHeight;
  }
  [[nodiscard]] const std::vector<std::string>& items() const noexcept { return mItems; }
  [[nodiscard]] const std::shared_ptr<const graphics::Font>& font() const noexcept { return mFont; }
  [[nodiscard]] float rowHeight() const noexcept { return mRowHeight; }
  [[nodiscard]] const SelectionHandler& onSelection() const noexcept { return mOnSelection; }
  [[nodiscard]] int selected() const noexcept { return mSelected; }

 private:
  std::vector<std::string> mItems;
  std::shared_ptr<const graphics::Font> mFont;
  float mRowHeight;
  SelectionHandler mOnSelection;
  int mSelected;
};

/** Selection/focus/hover state passed to a recycling-row builder. */
struct RecyclingRowState {
  bool selected = false;
  bool focused = false;
  bool hovered = false;
};

/** A keyed, per-row virtualized list; the builder returns a real Element subtree per visible row. */
class RecyclingListElement final : public Element {
 public:
  using RowBuilder = std::function<ElementPtr(const std::string& key, size_t index,
    const RecyclingRowState& state)>;
  class RowSnapshot {
   public:
    [[nodiscard]] const RowBuilder& builder() const noexcept { return mBuilder; }
    [[nodiscard]] const std::vector<ElementPtr>& rows() const noexcept { return mRows; }
    /** Factory: the only way to construct a snapshot, so rows and builder cannot be paired wrongly. */
    [[nodiscard]] static std::shared_ptr<const RowSnapshot> create(
        const std::vector<std::string>& keys, RowBuilder builder);
   private:
    RowBuilder mBuilder;
    std::vector<ElementPtr> mRows;
  };
  using RowSnapshotPtr = std::shared_ptr<const RowSnapshot>;

  RecyclingListElement(std::vector<std::string> keys, RowBuilder rowBuilder, float rowHeight = 30.0f,
    std::string key = {}, RowSnapshotPtr rowSnapshot = {})
    : Element(std::move(key)), mKeys(std::move(keys)),
      mRowBuilder(rowSnapshot != nullptr ? rowSnapshot->builder() : std::move(rowBuilder)),
      mRowHeight(rowHeight), mRowSnapshot(std::move(rowSnapshot)) {
  }

  [[nodiscard]] ElementKind kind() const noexcept override { return ElementKind::RecyclingList; }
  [[nodiscard]] bool measureEquivalent(const Element& other) const override {
    if (other.kind() != ElementKind::RecyclingList || key() != other.key()) return false;
    const auto& list = static_cast<const RecyclingListElement&>(other);
    if (mKeys != list.mKeys || mRowHeight != list.mRowHeight) return false;
    // If both callers supplied snapshots, compare the actual rows without rebuilding; a stale or
    // accidentally reused snapshot still compares its contents, so it cannot silently make
    // different row trees equal.
    const auto& left = builtRows();
    const auto& right = list.builtRows();
    if (left.size() != right.size()) return false;
    for (size_t i = 0; i < left.size(); ++i) {
      if (left[i] == nullptr || right[i] == nullptr) {
        if (left[i] != right[i]) return false;
        continue;
      }
      if (!left[i]->measureEquivalent(*right[i])) return false;
    }
    return true;
  }
  [[nodiscard]] const std::vector<std::string>& keys() const noexcept { return mKeys; }
  [[nodiscard]] const RowBuilder& rowBuilder() const noexcept { return mRowBuilder; }
  [[nodiscard]] float rowHeight() const noexcept { return mRowHeight; }
  [[nodiscard]] const RowSnapshotPtr& rowSnapshot() const noexcept { return mRowSnapshot; }

 private:
  [[nodiscard]] const std::vector<ElementPtr>& builtRows() const {
    if (mRowSnapshot != nullptr) return mRowSnapshot->rows();
    if (!mRowsBuilt) {
      mRowsBuilt = true;
      mBuiltRows.reserve(mKeys.size());
      for (size_t i = 0; i < mKeys.size(); ++i) {
        mBuiltRows.push_back(mRowBuilder ? mRowBuilder(mKeys[i], i, RecyclingRowState{}) : nullptr);
      }
    }
    return mBuiltRows;
  }

  std::vector<std::string> mKeys;
  RowBuilder mRowBuilder;
  float mRowHeight;
  RowSnapshotPtr mRowSnapshot;
  mutable bool mRowsBuilt = false;
  mutable std::vector<ElementPtr> mBuiltRows;
};

inline std::shared_ptr<const RecyclingListElement::RowSnapshot>
RecyclingListElement::RowSnapshot::create(const std::vector<std::string>& keys,
    RowBuilder builder) {
  auto snapshot = std::shared_ptr<RowSnapshot>(new RowSnapshot());
  snapshot->mBuilder = std::move(builder);
  snapshot->mRows.reserve(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    snapshot->mRows.push_back(snapshot->mBuilder
      ? snapshot->mBuilder(keys[i], i, RecyclingRowState{}) : nullptr);
  }
  return snapshot;
}

namespace components {

/**
 * Maps a sequence to keyed child elements.
 *
 * The callback should set a stable key on each returned element; an unkeyed list falls back to
 * positional reconciliation. This is a helper rather than a layer: the caller inserts the returned
 * vector into a `StackElement`, so the layout remains explicit.
 */
template <class T, class Build>
[[nodiscard]] std::vector<ElementPtr> ForEach(const std::vector<T>& items, Build&& build) {
  std::vector<ElementPtr> children;
  children.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    children.push_back(build(items[i], i));
  }
  return children;
}


/** Factory helpers for the common control elements. */
[[nodiscard]] inline ElementPtr CheckBox(std::string label,
    std::shared_ptr<const graphics::Font> font, const bool checked = false,
    std::function<void(bool)> onChanged = {}, std::string key = {}) {
  return std::make_shared<CheckBoxElement>(std::move(label), std::move(font), checked,
    std::move(onChanged), std::move(key));
}
[[nodiscard]] inline ElementPtr Toggle(std::string label,
    std::shared_ptr<const graphics::Font> font, const bool on = false,
    std::function<void(bool)> onChanged = {}, std::string key = {}) {
  return std::make_shared<ToggleElement>(std::move(label), std::move(font), on,
    std::move(onChanged), std::move(key));
}
[[nodiscard]] inline ElementPtr RadioButton(std::string label,
    std::shared_ptr<const graphics::Font> font, const bool selected = false,
    std::function<void()> onSelected = {}, std::string key = {}) {
  return std::make_shared<RadioButtonElement>(std::move(label), std::move(font), selected,
    std::move(onSelected), std::move(key));
}
[[nodiscard]] inline ElementPtr ProgressBar(std::shared_ptr<const graphics::Font> font,
    const double value = 0.0, std::string key = {}) {
  return std::make_shared<ProgressBarElement>(std::move(font), value, std::move(key));
}[[nodiscard]] inline ElementPtr Spinner(bool spinning = true, Color color = Color::MakeRGBA8(120, 170, 255, 255),
    std::string key = {}) {
  return std::make_shared<SpinnerElement>(spinning, color, std::move(key));
}[[nodiscard]] inline ElementPtr ColorButton(Color color, std::string accessibleName,
    std::function<void()> onClick = {}, std::string key = {}) {
  return std::make_shared<ColorButtonElement>(color, std::move(accessibleName),
    std::move(onClick), std::move(key));
}[[nodiscard]] inline ElementPtr IconButton(std::shared_ptr<const graphics::Image> icon,
    std::string accessibleName, std::function<void()> onClick = {}, std::string key = {}) {
  return std::make_shared<IconButtonElement>(std::move(icon), std::move(accessibleName),
    std::move(onClick), std::move(key));
}[[nodiscard]] inline ElementPtr Table(std::vector<TableColumn> columns,
    std::vector<std::vector<std::shared_ptr<Layer>>> rows, const float gap = 8.0f,
    Insets padding = {}, std::string key = {}) {
  return std::make_shared<TableElement>(std::move(columns), std::move(rows), gap, padding,
    std::move(key));
}
[[nodiscard]] inline ElementPtr SegmentedControl(std::vector<std::string> segments,
    std::shared_ptr<const graphics::Font> font, const int selected = 0,
    std::function<void(int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<SegmentedControlElement>(std::move(segments), std::move(font), selected,
    std::move(onChanged), std::move(key));
}
/** Builds a conditional element; a null branch means "nothing". */
[[nodiscard]] inline ElementPtr When(const bool condition, ElementPtr thenBranch,
    ElementPtr elseBranch = {}, std::string key = {}) {
  return std::make_shared<WhenElement>(condition, std::move(thenBranch), std::move(elseBranch),
    std::move(key));
}

/** Builds a function element. */
[[nodiscard]] inline ElementPtr Component(ComponentElement::Builder build, std::string key = {}) {
  return std::make_shared<ComponentElement>(std::move(build), std::move(key));
}

/** Wraps a child with layout/visual props. */
[[nodiscard]] inline ElementPtr WithProps(ElementPtr child, ElementProps props, std::string key = {}) {
  return std::make_shared<PropsElement>(std::move(child), props, std::move(key));
}

/** Wraps a prebuilt layer (product widget) as an element. */
[[nodiscard]] inline ElementPtr Layer(std::shared_ptr<Layer> layer, std::string key = {}) {
  return std::make_shared<LayerElement>(std::move(layer), std::move(key));
}

/** Builds a labelled numeric slider. */
[[nodiscard]] inline ElementPtr Slider(std::string label, std::shared_ptr<const graphics::Font> font,
    double minimum, double maximum, double value, SliderElement::ChangeHandler onChanged = {},
    std::string key = {}, std::function<void()> onEditBegin = {},
    std::function<void()> onEditEnd = {}) {
  return std::make_shared<SliderElement>(std::move(label), std::move(font), minimum, maximum, value,
    std::move(onChanged), std::move(key), std::move(onEditBegin), std::move(onEditEnd));
}

/** Builds a button element. */
[[nodiscard]] inline ElementPtr Button(std::string label, std::shared_ptr<const graphics::Font> font,
    ButtonElement::ClickHandler onClick = {}, std::string key = {}, bool enabled = true,
    ButtonElement::EnabledProperty enabledProperty = {}) {
  return std::make_shared<ButtonElement>(std::move(label), std::move(font), std::move(onClick),
    std::move(key), enabled, std::move(enabledProperty));
}

/** Builds a text field element. */
[[nodiscard]] inline ElementPtr TextInput(std::string text,
    std::shared_ptr<const graphics::Font> font, TextInputElement::ChangeHandler onChanged = {},
    std::string key = {}) {
  return std::make_shared<TextInputElement>(std::move(text), std::move(font), std::move(onChanged),
    std::move(key));
}

/** Builds a four-region dock layout. */
[[nodiscard]] inline ElementPtr Dock(ElementPtr center, ElementPtr left = {}, ElementPtr right = {},
    ElementPtr bottom = {}, float leftWidth = 200.0f, float rightWidth = 240.0f,
    float bottomHeight = 180.0f, std::string key = {}) {
  return std::make_shared<DockElement>(std::move(center), std::move(left), std::move(right),
    std::move(bottom), leftWidth, rightWidth, bottomHeight, std::move(key));
}

/** Builds an equal-column grid. */
[[nodiscard]] inline ElementPtr Grid(int columns, std::vector<ElementPtr> children,
    std::string key = {}, float gap = 8.0f, Insets padding = {}) {
  return std::make_shared<GridElement>(columns, std::move(children), std::move(key), gap, padding);
}

/** Builds a scroll viewport around one content child. */
[[nodiscard]] inline ElementPtr Scroll(ElementPtr content, float preferredHeight = 0.0f,
    std::string key = {}) {
  return std::make_shared<ScrollElement>(std::move(content), preferredHeight, std::move(key));
}

/** Builds a live preview/viewport element from a ContentHost. */
[[nodiscard]] inline ElementPtr ContentView(std::shared_ptr<ContentHost> host,
    Color placeholder = Color::MakeRGBA8(35, 38, 46, 255), std::string key = {}) {
  return std::make_shared<ContentViewElement>(std::move(host), placeholder, std::move(key));
}

/** Builds a virtualized list element. */
[[nodiscard]] inline ElementPtr ListView(std::vector<std::string> items,
    std::shared_ptr<const graphics::Font> font, float rowHeight = 30.0f,
    ListViewElement::SelectionHandler onSelection = {}, int selected = -1, std::string key = {}) {
  return std::make_shared<ListViewElement>(std::move(items), std::move(font), rowHeight,
    std::move(onSelection), selected, std::move(key));
}

/** Builds a row snapshot from the same builder, so callers cannot pair a snapshot with a different row set accidentally. */
[[nodiscard]] inline RecyclingListElement::RowSnapshotPtr MakeRecyclingListSnapshot(
    const std::vector<std::string>& keys, RecyclingListElement::RowBuilder rowBuilder) {
  return RecyclingListElement::RowSnapshot::create(keys, std::move(rowBuilder));
}

/** Builds a keyed per-row virtualized list element. */
[[nodiscard]] inline ElementPtr RecyclingList(std::vector<std::string> keys,
    RecyclingListElement::RowBuilder rowBuilder, float rowHeight = 30.0f, std::string key = {},
    RecyclingListElement::RowSnapshotPtr rowSnapshot = {}) {
  return std::make_shared<RecyclingListElement>(std::move(keys), std::move(rowBuilder), rowHeight,
    std::move(key), std::move(rowSnapshot));
}

/** Builds a horizontal or vertical rule. */
[[nodiscard]] inline ElementPtr Divider(Divider::Orientation orientation = Divider::Orientation::Horizontal,
    float thickness = 1.0f, float inset = 0.0f, std::optional<Color> color = {},
    std::string key = {}) {
  return std::make_shared<DividerElement>(orientation, thickness, inset, color, std::move(key));
}

/** Builds a keyboard shortcut cap. */
[[nodiscard]] inline ElementPtr Kbd(std::string label,
    std::shared_ptr<const graphics::Font> font, std::string key = {}) {
  return std::make_shared<KbdElement>(std::move(label), std::move(font), std::move(key));
}

/** Builds a status/count pill. */
[[nodiscard]] inline ElementPtr Badge(std::string text,
    std::shared_ptr<const graphics::Font> font, bool dot = false,
    std::shared_ptr<const graphics::Image> icon = {}, std::string key = {}) {
  return std::make_shared<BadgeElement>(std::move(text), std::move(font), dot, std::move(icon),
    std::move(key));
}

/** Builds a square tinted image. */
[[nodiscard]] inline ElementPtr Icon(std::shared_ptr<const graphics::Image> image = {},
    float size = 20.0f, std::optional<Color> color = {}, std::string key = {}) {
  return std::make_shared<IconElement>(std::move(image), size, color, std::move(key));
}

/** Builds a round initials/image avatar. */
[[nodiscard]] inline ElementPtr Avatar(std::string initials,
    std::shared_ptr<const graphics::Font> font, float size = 32.0f,
    std::shared_ptr<const graphics::Image> image = {}, bool statusVisible = false,
    std::optional<Color> statusColor = {}, std::string key = {}) {
  return std::make_shared<AvatarElement>(std::move(initials), std::move(font), size,
    std::move(image), statusVisible, statusColor, std::move(key));
}

/** Builds an overlapping row of avatars. */
[[nodiscard]] inline ElementPtr AvatarGroup(std::vector<std::string> initials,
    std::shared_ptr<const graphics::Font> font, int maxVisible = 4, float size = 28.0f,
    std::string key = {}) {
  return std::make_shared<AvatarGroupElement>(std::move(initials), std::move(font), maxVisible, size,
    std::move(key));
}

/** Builds a loading placeholder. */
[[nodiscard]] inline ElementPtr Skeleton(float width = 160.0f, float height = 14.0f, int lines = 1,
    float lineGap = 6.0f, Skeleton::Shape shape = Skeleton::Shape::Rounded, bool animating = true,
    std::string key = {}) {
  return std::make_shared<SkeletonElement>(width, height, lines, lineGap, shape, animating,
    std::move(key));
}

/** Builds a dashboard statistic. */
[[nodiscard]] inline ElementPtr Stat(std::string label, std::string value,
    std::shared_ptr<const graphics::Font> font, std::string caption = {},
    StatTrend trend = StatTrend::None, std::string key = {}) {
  return std::make_shared<StatElement>(std::move(label), std::move(value), std::move(font),
    std::move(caption), trend, std::move(key));
}

/** Builds a low/high meter. */
[[nodiscard]] inline ElementPtr Meter(double minimum = 0.0, double maximum = 1.0, double value = 0.0,
    std::string key = {}) {
  return std::make_shared<MeterElement>(minimum, maximum, value, std::move(key));
}

/** Builds a circular progress indicator. */
[[nodiscard]] inline ElementPtr ProgressRing(float size = 36.0f, double value = 0.0,
    float thickness = 4.0f, bool indeterminate = false, std::string key = {}) {
  return std::make_shared<ProgressRingElement>(size, value, thickness, indeterminate,
    std::move(key));
}

/** Builds a coloured status dot with a label. */
[[nodiscard]] inline ElementPtr StatusIndicator(std::string label,
    std::shared_ptr<const graphics::Font> font, StatusKind status = StatusKind::Online,
    std::string key = {}) {
  return std::make_shared<StatusIndicatorElement>(std::move(label), std::move(font), status,
    std::move(key));
}

/** Builds a numbered step trail. */
[[nodiscard]] inline ElementPtr StepIndicator(std::vector<std::string> steps,
    std::shared_ptr<const graphics::Font> font, int current = 0,
    StepIndicatorElement::ChangeHandler onChanged = {}, std::string key = {}) {
  return std::make_shared<StepIndicatorElement>(std::move(steps), std::move(font), current,
    std::move(onChanged), std::move(key));
}

/** Builds a label/value settings row. */
[[nodiscard]] inline ElementPtr PropertyRow(std::string label, std::string value,
    std::shared_ptr<const graphics::Font> font, std::string help = {}, float labelWidth = 0.0f,
    std::string key = {}) {
  return std::make_shared<PropertyRowElement>(std::move(label), std::move(value), std::move(font),
    std::move(help), labelWidth, std::move(key));
}

/** Builds a single-line tag list. */
[[nodiscard]] inline ElementPtr TagList(std::vector<std::string> tags,
    std::shared_ptr<const graphics::Font> font, int maxVisible = 4, std::string key = {}) {
  return std::make_shared<TagListElement>(std::move(tags), std::move(font), maxVisible,
    std::move(key));
}
/** Builds an inline text link. */
[[nodiscard]] inline ElementPtr Link(std::string text,
    std::shared_ptr<const graphics::Font> font, std::function<void()> onActivate = {},
    std::string key = {}) {
  return std::make_shared<LinkElement>(std::move(text), std::move(font), std::move(onActivate),
    std::move(key));
}

/** Builds a compact tag/chip. */
[[nodiscard]] inline ElementPtr Chip(std::string text,
    std::shared_ptr<const graphics::Font> font, ChipVariant variant = ChipVariant::Neutral,
    std::shared_ptr<const graphics::Image> icon = {}, bool removable = false, bool selected = false,
    std::function<void()> onRemove = {}, std::string key = {}) {
  return std::make_shared<ChipElement>(std::move(text), std::move(font), variant, std::move(icon),
    removable, selected, std::move(onRemove), std::move(key));
}

/** Builds a star rating. */
[[nodiscard]] inline ElementPtr Rating(int stars = 5, double rating = 0.0, bool halfSteps = false,
    bool readOnly = false, std::function<void(double)> onChanged = {}, std::string key = {}) {
  return std::make_shared<RatingElement>(stars, rating, halfSteps, readOnly, std::move(onChanged),
    std::move(key));
}

/** Builds a numeric stepper. */
[[nodiscard]] inline ElementPtr Stepper(double minimum, double maximum, double value,
    double step = 1.0, std::function<void(double)> onChanged = {}, std::string key = {}) {
  return std::make_shared<StepperElement>(minimum, maximum, value, step, std::move(onChanged),
    std::move(key));
}

/** Builds a two-handle range slider. */
[[nodiscard]] inline ElementPtr RangeSlider(double minimum = 0.0, double maximum = 1.0,
    double low = 0.25, double high = 0.75, double step = 0.01,
    std::function<void(double, double)> onChanged = {}, std::string key = {}) {
  return std::make_shared<RangeSliderElement>(minimum, maximum, low, high, step,
    std::move(onChanged), std::move(key));
}

/** Builds a numbered page selector. */
[[nodiscard]] inline ElementPtr Pagination(std::shared_ptr<const graphics::Font> font,
    int pageCount = 1, int page = 0, std::function<void(int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<PaginationElement>(std::move(font), pageCount, page, std::move(onChanged),
    std::move(key));
}

/** Builds a toggle button. */
[[nodiscard]] inline ElementPtr ToggleButton(std::string label,
    std::shared_ptr<const graphics::Font> font, bool selected = false,
    std::function<void(bool)> onChanged = {}, std::string key = {}) {
  return std::make_shared<ToggleButtonElement>(std::move(label), std::move(font), selected,
    std::move(onChanged), std::move(key));
}

/** Builds a clickable breadcrumb trail. */
[[nodiscard]] inline ElementPtr Breadcrumb(std::vector<std::string> items,
    std::shared_ptr<const graphics::Font> font, std::string separator = "/", int currentIndex = -1,
    BreadcrumbElement::SelectHandler onSelect = {}, std::string key = {}) {
  return std::make_shared<BreadcrumbElement>(std::move(items), std::move(font),
    std::move(separator), currentIndex, std::move(onSelect), std::move(key));
}

/** Builds a severity banner. */
[[nodiscard]] inline ElementPtr Banner(std::string text,
    std::shared_ptr<const graphics::Font> font, BannerSeverity severity = BannerSeverity::Info,
    std::string actionLabel = {}, std::function<void()> onAction = {}, bool dismissible = false,
    std::shared_ptr<const graphics::Image> icon = {}, std::function<void()> onDismiss = {},
    std::string key = {}) {
  return std::make_shared<BannerElement>(std::move(text), std::move(font), severity,
    std::move(actionLabel), std::move(onAction), dismissible, std::move(icon), std::move(onDismiss),
    std::move(key));
}

/** Builds a client-side title bar. */
[[nodiscard]] inline ElementPtr TitleBar(std::string title,
    std::shared_ptr<const graphics::Font> font, std::function<void()> onMinimize = {},
    std::function<void()> onMaximize = {}, std::function<void()> onClose = {},
    bool showMinimize = true, bool showMaximize = true, bool showClose = true,
    std::string key = {}) {
  return std::make_shared<TitleBarElement>(std::move(title), std::move(font),
    std::move(onMinimize), std::move(onMaximize), std::move(onClose), showMinimize, showMaximize,
    showClose, std::move(key));
}

/** Builds a primary action with an options dropdown. */
[[nodiscard]] inline ElementPtr SplitButton(std::string text,
    std::shared_ptr<const graphics::Font> font, std::vector<std::string> options = {},
    std::function<void()> onClick = {}, SplitButtonElement::OptionHandler onOption = {},
    std::string key = {}) {
  return std::make_shared<SplitButtonElement>(std::move(text), std::move(font),
    std::move(options), std::move(onClick), std::move(onOption), std::move(key));
}

/** Builds a drop target. */
[[nodiscard]] inline ElementPtr DropZone(std::string label,
    std::shared_ptr<const graphics::Font> font,
    std::function<void(std::vector<std::string>)> onFilesDropped = {},
    std::function<void(std::string)> onTextDropped = {}, std::string key = {}) {
  return std::make_shared<DropZoneElement>(std::move(label), std::move(font),
    std::move(onFilesDropped), std::move(onTextDropped), std::move(key));
}

/** Builds a month calendar. */
[[nodiscard]] inline ElementPtr Calendar(std::chrono::year_month_day date,
    std::shared_ptr<const graphics::Font> font, int firstWeekday = 0,
    std::function<void(std::chrono::year_month_day)> onSelect = {},
    std::function<void()> onCancel = {}, std::string key = {}) {
  return std::make_shared<CalendarElement>(date, std::move(font), firstWeekday,
    std::move(onSelect), std::move(onCancel), std::move(key));
}
/** Builds a styled rich-text document. */
[[nodiscard]] inline ElementPtr StyledText(TextDocument document = {},
    std::function<void(std::string_view)> onLinkActivated = {}, std::string key = {}) {
  return std::make_shared<StyledTextElement>(std::move(document), std::move(onLinkActivated),
    std::move(key));
}
/** Builds a multi-select toggle group. */
[[nodiscard]] inline ElementPtr ToggleGroup(std::vector<std::string> options,
    std::shared_ptr<const graphics::Font> font, std::vector<int> selected = {},
    ToggleGroupElement::ChangeHandler onChanged = {}, std::string key = {}) {
  return std::make_shared<ToggleGroupElement>(std::move(options), std::move(font),
    std::move(selected), std::move(onChanged), std::move(key));
}

/** Builds an exclusive radio group. */
[[nodiscard]] inline ElementPtr RadioGroup(std::vector<std::string> options,
    std::shared_ptr<const graphics::Font> font, int selected = 0,
    std::function<void(int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<RadioGroupElement>(std::move(options), std::move(font), selected,
    std::move(onChanged), std::move(key));
}

/** Builds a multi-select check box group. */
[[nodiscard]] inline ElementPtr CheckBoxGroup(std::vector<std::string> options,
    std::shared_ptr<const graphics::Font> font, std::vector<int> selected = {},
    CheckBoxGroupElement::ChangeHandler onChanged = {}, std::string key = {}) {
  return std::make_shared<CheckBoxGroupElement>(std::move(options), std::move(font),
    std::move(selected), std::move(onChanged), std::move(key));
}

/** Builds a dropdown multi-select. */
[[nodiscard]] inline ElementPtr MultiSelect(std::vector<std::string> options,
    std::shared_ptr<const graphics::Font> font, std::vector<int> selected = {},
    std::string placeholder = {}, MultiSelectElement::ChangeHandler onChanged = {},
    std::string key = {}) {
  return std::make_shared<MultiSelectElement>(std::move(options), std::move(font),
    std::move(selected), std::move(placeholder), std::move(onChanged), std::move(key));
}
/** Builds a titled card. */
[[nodiscard]] inline ElementPtr Card(std::string title,
    std::shared_ptr<const graphics::Font> font, ElementPtr content = {},
    ElementPtr headerAction = {}, std::string key = {}) {
  return std::make_shared<CardElement>(std::move(title), std::move(font), std::move(content),
    std::move(headerAction), std::move(key));
}

/** Builds a collapsible section. */
[[nodiscard]] inline ElementPtr Expander(std::string title,
    std::shared_ptr<const graphics::Font> font, ElementPtr content = {}, bool expanded = true,
    std::function<void(bool)> onChanged = {}, std::string key = {}) {
  return std::make_shared<ExpanderElement>(std::move(title), std::move(font), std::move(content),
    expanded, std::move(onChanged), std::move(key));
}

/** Builds a labelled form row. */
[[nodiscard]] inline ElementPtr FormField(std::string label,
    std::shared_ptr<const graphics::Font> font, ElementPtr content = {}, std::string help = {},
    std::string error = {}, std::string key = {}) {
  return std::make_shared<FormFieldElement>(std::move(label), std::move(font), std::move(content),
    std::move(help), std::move(error), std::move(key));
}

/** Builds a section header. */
[[nodiscard]] inline ElementPtr SectionHeader(std::string title,
    std::shared_ptr<const graphics::Font> font, std::string subtitle = {}, ElementPtr action = {},
    std::string key = {}) {
  return std::make_shared<SectionHeaderElement>(std::move(title), std::move(font),
    std::move(subtitle), std::move(action), std::move(key));
}

/** Builds an empty-state panel. */
[[nodiscard]] inline ElementPtr EmptyState(std::string title,
    std::shared_ptr<const graphics::Font> font, std::string description = {},
    ElementPtr content = {}, ElementPtr action = {},
    std::shared_ptr<const graphics::Image> icon = {}, std::string key = {}) {
  return std::make_shared<EmptyStateElement>(std::move(title), std::move(font),
    std::move(description), std::move(content), std::move(action), std::move(icon),
    std::move(key));
}
/** Builds a two-pane split view. */
[[nodiscard]] inline ElementPtr SplitView(Axis axis, float fraction, ElementPtr first,
    ElementPtr second, std::function<void(float)> onFractionChanged = {}, std::string key = {}) {
  return std::make_shared<SplitViewElement>(axis, fraction, std::move(first), std::move(second),
    std::move(onFractionChanged), std::move(key));
}
/** Builds a tab bar. */
[[nodiscard]] inline ElementPtr TabBar(std::shared_ptr<const graphics::Font> font,
    std::vector<TabItem> tabs, int active = 0, std::function<void(int)> onChanged = {},
    std::string key = {}) {
  return std::make_shared<TabBarElement>(std::move(font), std::move(tabs), active,
    std::move(onChanged), std::move(key));
}
/** Builds a horizontal toolbar. */
[[nodiscard]] inline ElementPtr Toolbar(std::shared_ptr<const graphics::Font> font,
    std::vector<ToolbarItem> items, float height = 44.0f, std::string key = {}) {
  return std::make_shared<ToolbarElement>(std::move(font), std::move(items), height,
    std::move(key));
}
/** Builds a status bar. */
[[nodiscard]] inline ElementPtr StatusBar(std::shared_ptr<const graphics::Font> font,
    std::string left = {}, std::string right = {}, std::vector<ElementPtr> leading = {},
    std::vector<ElementPtr> trailing = {}, std::string key = {}) {
  return std::make_shared<StatusBarElement>(std::move(font), std::move(left), std::move(right),
    std::move(leading), std::move(trailing), std::move(key));
}
/** Builds a dropdown select. */
[[nodiscard]] inline ElementPtr Select(std::vector<std::string> options,
    std::shared_ptr<const graphics::Font> font, int selected = -1, std::string placeholder = {},
    std::function<void(int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<SelectElement>(std::move(options), std::move(font), selected,
    std::move(placeholder), std::move(onChanged), std::move(key));
}

/** Builds an editable combo box. */
[[nodiscard]] inline ElementPtr ComboBox(std::vector<std::string> options,
    std::shared_ptr<const graphics::Font> font, std::string text = {}, int selected = -1,
    std::string placeholder = {}, ComboBoxElement::ChangeHandler onChanged = {},
    std::string key = {}) {
  return std::make_shared<ComboBoxElement>(std::move(options), std::move(font), std::move(text),
    selected, std::move(placeholder), std::move(onChanged), std::move(key));
}

/** Builds a search field. */
[[nodiscard]] inline ElementPtr SearchField(std::string placeholder,
    std::shared_ptr<const graphics::Font> font, std::string text = {},
    std::function<void(const std::string&)> onChanged = {},
    std::function<void(const std::string&)> onSearch = {}, std::string key = {}) {
  return std::make_shared<SearchFieldElement>(std::move(placeholder), std::move(font),
    std::move(text), std::move(onChanged), std::move(onSearch), std::move(key));
}

/** Builds a masked password field. */
[[nodiscard]] inline ElementPtr PasswordField(std::string placeholder,
    std::shared_ptr<const graphics::Font> font, std::string text = {}, size_t maxLength = 0,
    std::function<void(const std::string&)> onChanged = {}, std::string key = {}) {
  return std::make_shared<PasswordFieldElement>(std::move(placeholder), std::move(font),
    std::move(text), maxLength, std::move(onChanged), std::move(key));
}

/** Builds a numeric field. */
[[nodiscard]] inline ElementPtr NumberField(std::shared_ptr<const graphics::Font> font,
    double minimum, double maximum, double value, double step = 1.0, int decimals = 0,
    std::function<void(double)> onChanged = {}, std::string key = {}) {
  return std::make_shared<NumberFieldElement>(std::move(font), minimum, maximum, value, step,
    decimals, std::move(onChanged), std::move(key));
}

/** Builds a token/tag editor. */
[[nodiscard]] inline ElementPtr TagInput(std::shared_ptr<const graphics::Font> font,
    std::vector<std::string> tokens, std::string placeholder = "Add tag",
    std::function<void(const std::vector<std::string>&)> onChanged = {}, std::string key = {}) {
  return std::make_shared<TagInputElement>(std::move(font), std::move(tokens),
    std::move(placeholder), std::move(onChanged), std::move(key));
}

/** Builds a colour field. */
[[nodiscard]] inline ElementPtr ColorInput(std::shared_ptr<const graphics::Font> font, Color color,
    std::function<void(Color)> onChanged = {}, std::string key = {}) {
  return std::make_shared<ColorInputElement>(std::move(font), color, std::move(onChanged),
    std::move(key));
}
/** Builds a vertical timeline. */
[[nodiscard]] inline ElementPtr Timeline(std::vector<TimelineEntry> entries,
    std::shared_ptr<const graphics::Font> font, std::string key = {}) {
  return std::make_shared<TimelineElement>(std::move(entries), std::move(font), std::move(key));
}

/** Builds a colour picker. */
[[nodiscard]] inline ElementPtr ColorPicker(std::vector<Color> palette, ColorPickerMode mode,
    Color color, float alpha, int selected, std::function<void(Color)> onChanged = {},
    std::string key = {}) {
  return std::make_shared<ColorPickerElement>(std::move(palette), mode, color, alpha, selected,
    std::move(onChanged), std::move(key));
}

/** Builds an IME candidate list. */
[[nodiscard]] inline ElementPtr CandidateListView(std::vector<std::string> candidates,
    size_t selected, std::shared_ptr<const graphics::Font> font, std::string key = {}) {
  return std::make_shared<CandidateListViewElement>(std::move(candidates), selected,
    std::move(font), std::move(key));
}

/** Builds a scrolling table view. */
[[nodiscard]] inline ElementPtr TableView(std::vector<TableColumn> columns,
    std::vector<std::vector<std::shared_ptr<coral::ui::Layer>>> rows,
    std::shared_ptr<const graphics::Font> font, float gap = 0.0f, Insets padding = {},
    int selected = -1,
    std::function<void(int)> onRowActivated = {}, std::string key = {}) {
  return std::make_shared<TableViewElement>(std::move(columns), std::move(rows), std::move(font),
    gap, padding, selected, std::move(onRowActivated), std::move(key));
}
/** Builds a date field. */
[[nodiscard]] inline ElementPtr DateField(std::shared_ptr<const graphics::Font> font,
    std::chrono::year_month_day date = {},
    std::function<void(std::chrono::year_month_day)> onChanged = {}, std::string key = {}) {
  return std::make_shared<DateFieldElement>(std::move(font), date, std::move(onChanged),
    std::move(key));
}

/** Builds a time field. */
[[nodiscard]] inline ElementPtr TimeField(std::shared_ptr<const graphics::Font> font, int hour = 0,
    int minute = 0, std::optional<bool> use24HourClock = std::nullopt,
    std::function<void(int, int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<TimeFieldElement>(std::move(font), hour, minute, use24HourClock,
    std::move(onChanged), std::move(key));
}

/** Builds a combined date + time field. */
[[nodiscard]] inline ElementPtr DateTimeField(std::shared_ptr<const graphics::Font> font,
    std::chrono::year_month_day date = {}, int hour = 0, int minute = 0,
    std::function<void(std::chrono::year_month_day, int, int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<DateTimeFieldElement>(std::move(font), date, hour, minute,
    std::move(onChanged), std::move(key));
}

/** Builds a date range field. */
[[nodiscard]] inline ElementPtr DateRangeField(std::shared_ptr<const graphics::Font> font,
    std::chrono::year_month_day start = {}, std::chrono::year_month_day end = {},
    std::function<void(std::chrono::year_month_day, std::chrono::year_month_day)> onChanged = {},
    std::string key = {}) {
  return std::make_shared<DateRangeFieldElement>(std::move(font), start, end, std::move(onChanged),
    std::move(key));
}

/** Builds a time range field. */
[[nodiscard]] inline ElementPtr TimeRangeField(std::shared_ptr<const graphics::Font> font,
    int startHour = 0, int startMinute = 0, int endHour = 1, int endMinute = 0,
    std::function<void(int, int, int, int)> onChanged = {}, std::string key = {}) {
  return std::make_shared<TimeRangeFieldElement>(std::move(font), startHour, startMinute, endHour,
    endMinute, std::move(onChanged), std::move(key));
}

/** Builds a compact time picker. */
[[nodiscard]] inline ElementPtr TimePicker(std::shared_ptr<const graphics::Font> font, int hour,
    int minute, bool use24HourClock = true, std::function<void(int, int)> onChanged = {},
    std::function<void()> onCancel = {}, std::function<void()> onDone = {}, std::string key = {}) {
  return std::make_shared<TimePickerElement>(std::move(font), hour, minute, use24HourClock,
    std::move(onChanged), std::move(onCancel), std::move(onDone), std::move(key));
}
/** Builds a tree view. */
[[nodiscard]] inline ElementPtr TreeView(std::shared_ptr<const graphics::Font> font,
    std::vector<TreeNodeSpec> roots, float rowHeight = 28.0f,
    std::vector<std::size_t> selected = {}, TreeViewElement::PathHandler onSelected = {},
    TreeViewElement::PathHandler onActivated = {}, std::string key = {}) {
  return std::make_shared<TreeViewElement>(std::move(font), std::move(roots), rowHeight,
    std::move(selected), std::move(onSelected), std::move(onActivated), std::move(key));
}
} // namespace components

} // namespace coral::ui

#endif // CORAL_UI_ELEMENT_H
