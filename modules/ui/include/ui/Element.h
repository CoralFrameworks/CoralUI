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

#include <ui/Flex.h>
#include <ui/Table.h>
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

} // namespace components

} // namespace coral::ui

#endif // CORAL_UI_ELEMENT_H
