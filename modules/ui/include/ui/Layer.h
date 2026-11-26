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

#ifndef CORAL_UI_LAYER_H
#define CORAL_UI_LAYER_H

#include <ui/Types.h>

#include <ui/DragDrop.h>
#include <ui/Event.h>
#include <ui/Curve.h>
#include <ui/Geometry.h>
#include <ui/RenderContext.h>
#include <ui/Style.h>
#include <ui/Theme.h>


#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace graphics {
class Bitmap;
}

namespace coral::ui {

class Window;

namespace detail {
struct LayerRecording;
}

struct Theme;

/** The vocabulary a platform accessibility bridge needs; deliberately small and explicit. */
enum class AccessibilityRole : uint8_t {
  Generic,
  Button,
  CheckBox,
  Radio,
  Slider,
  ComboBox,
  ColorPicker,
  SpinButton,
  Calendar,
  ProgressIndicator,
  TabList,
  TextField,
  Label,
  Link,
  List,
  Tree,
  Menu,
  Table,
  ListItem,
  ScrollView,
  Dialog,
  Group,
};

/** How a child aligns itself on the cross axis, overriding the container's rule. */
enum class AlignSelf : uint8_t {
  Auto,
  Start,
  Center,
  End,
  Stretch,
  /** Align the child's text baseline with the other baseline-aligned children. */
  Baseline,
};

/** Whether a child takes part in its parent's flow or is positioned against its bounds. */
enum class Position : uint8_t {
  Flow,
  Absolute,
};

/**
 * Optional sizing hints a layout container reads for this child.
 *
 * `basis < 0` and the percentage fields `< 0` mean "use the child's measured size". Grow/shrink are
 * weights, not pixels; margins are outside the child's bounds. `max*` values of 0 mean unlimited.
 *
 * An `Absolute` child is laid out against the parent's inner rectangle instead of the flow: the
 * `hasLeft`/`hasTop`/`hasRight`/`hasBottom` flags say which edges are specified, and a pair of
 * opposite edges (left and right, or top and bottom) also decides the size when no percentage does.
 */
struct LayoutParams {
  float grow = 0.0f;
  float shrink = 1.0f;
  float basis = -1.0f;
  float widthPercent = -1.0f;
  float heightPercent = -1.0f;
  float minWidth = 0.0f;
  float minHeight = 0.0f;
  float maxWidth = 0.0f;
  float maxHeight = 0.0f;
  /** Width / height; > 0 derives the cross axis from the resolved main axis in a Flex. */
  float aspectRatio = 0.0f;
  /** Size the main axis from the child's intrinsic width (a Row) or height (a Column). */
  bool fitContentMain = false;
  /** Size the cross axis from the child's intrinsic size. */
  bool fitContentCross = false;
  /** Measure the child with an unbounded main axis so it reports its natural size. */
  bool unboundedMain = false;
  /** Measure the child with an unbounded cross axis. */
  bool unboundedCross = false;
  Insets margin{};
  AlignSelf alignSelf = AlignSelf::Auto;
  Position position = Position::Flow;
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  bool hasLeft = false;
  bool hasTop = false;
  bool hasRight = false;
  bool hasBottom = false;
};

/**
 * Value equality of two layout parameter sets.
 *
 * This is the canonical comparison: a setter that writes layout parameters uses it to decide whether
 * anything actually changed, and the element layer uses it to decide whether a props wrapper is
 * measure-equivalent. Keeping one definition is what stops the two from drifting apart (a stale
 * comparison here used to mean "every rebuild dirties the root and relayouts the window").
 */
[[nodiscard]] inline bool sameLayoutParams(const LayoutParams& left, const LayoutParams& right) noexcept {
  return left.grow == right.grow && left.shrink == right.shrink && left.basis == right.basis &&
    left.widthPercent == right.widthPercent && left.heightPercent == right.heightPercent &&
    left.minWidth == right.minWidth && left.minHeight == right.minHeight &&
    left.maxWidth == right.maxWidth && left.maxHeight == right.maxHeight &&
    left.aspectRatio == right.aspectRatio && left.fitContentMain == right.fitContentMain &&
    left.fitContentCross == right.fitContentCross && left.unboundedMain == right.unboundedMain &&
    left.unboundedCross == right.unboundedCross && left.margin == right.margin &&
    left.alignSelf == right.alignSelf && left.position == right.position &&
    left.left == right.left && left.top == right.top && left.right == right.right &&
    left.bottom == right.bottom && left.hasLeft == right.hasLeft && left.hasTop == right.hasTop &&
    left.hasRight == right.hasRight && left.hasBottom == right.hasBottom;
}


/** Text and layout direction; an RTL layout mirrors the horizontal main axis. */
enum class Direction : uint8_t {
  Ltr,
  Rtl,
};

/** The default duration/easing a layer uses when a compositing setter is called. */
struct TransitionSpec {
  std::chrono::milliseconds duration{ 0 };
  Easing easing = Easing::EaseOut;

  [[nodiscard]] bool active() const noexcept {
    return duration.count() > 0;
  }
};

/**
 * One node of the UI tree.
 *
 * Bounds are **logical units** in the window's coordinate space and are absolute: a parent lays a
 * child out by assigning its rectangle, and hit testing compares a logical point against every node it
 * meets. `Window::renderFrame` applies one `uiScale` to the canvas before drawing the tree, so a
 * control never converts to device pixels itself (text is the exception: `TextPainter` rasterises at
 * `size * uiScale`). Translation/scale/rotation and the clip path are compositing properties on the
 * layer, applied around `transformOrigin`; hit testing uses the same inverse transform.
 *
 * A layer draws itself and then its children in order. Input walks the tree from the topmost child and
 * bubbles back up until a layer reports the event handled, so a label inside a button can ignore a
 * click and have the button receive it.
 *
 * Thread-affine: the tree is owned by the UI thread, the same one that owns the window.
 */
class Layer {
public:
  Layer() = default;
  virtual ~Layer();

  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;

  void setBounds(const Rect& bounds);

  [[nodiscard]] const Rect& bounds() const noexcept {
    return mBounds;
  }
  /**
   * Bounds in window coordinates after this layer's and every ancestor's translation.
   *
   * Composite controls attach popups to the root overlay, which expects window coordinates; their
   * own bounds are in the page's untranslated coordinate space.
   */
  [[nodiscard]] Rect boundsInWindow() const;

  void setVisible(const bool visible);

  [[nodiscard]] bool visible() const noexcept {
    return mVisible;
  }

  /**
   * Clips this layer's own content and its children to its bounds.
   *
   * Changing it repaints: the clip participates in the recorded command stream, so a layer that only
   * toggles clipping still has to be re-recorded. It used to be an inline no-op on the dirty flags,
   * which made a lone `clip` change paint nothing.
   */
  void setClip(bool clip);

  [[nodiscard]] bool clips() const noexcept {
    return mClip;
  }

  /** Group opacity. 1 is opaque, 0 fully transparent; children composite inside one layer. */
  void setOpacity(float opacity);
  /** Explicit transition, independent of the layer's default `setTransition`. */
  void setOpacity(float opacity, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);

  [[nodiscard]] float opacity() const noexcept {
    return mOpacity;
  }

  /**
   * Sets the layout direction for this subtree.
   *
   * The value inherits: a layer without an explicit direction uses the nearest ancestor's, defaulting
   * to left-to-right. A row Flex mirrors its main axis (and swaps horizontal padding) in RTL; text
   * shaping and other controls keep their own direction semantics.
   */
  void setDirection(Direction direction);
  [[nodiscard]] Direction direction() const noexcept;
  [[nodiscard]] bool hasExplicitDirection() const noexcept {
    return mDirectionSet;
  }

  /** Makes the ordinary compositing setters animate instead of snapping. */
  void setTransition(TransitionSpec transition) noexcept {
    mTransition = transition;
  }

  void setTransition(std::chrono::milliseconds duration, Easing easing = Easing::EaseOut) noexcept {
    mTransition = { duration, easing };
  }

  [[nodiscard]] const TransitionSpec& transition() const noexcept {
    return mTransition;
  }

  /**
   * Animates the background colour to @p target; a zero duration snaps.
   *
   * `setBackgroundColor` uses this automatically when `setTransition` configured a duration, so a
   * control that changes colour on hover/disabled does not need to call it.
   */
  void animateBackgroundColor(Color target, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);
  [[nodiscard]] bool hasBackgroundAnimation() const noexcept {
    return mColorAnimation.active;
  }

  /** Translation in logical units, applied around the layer's centre with scale/rotation. */
  void setTranslation(const Point& translation);
  void setTranslation(const Point& translation, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);

  [[nodiscard]] const Point& translation() const noexcept {
    return mTranslation;
  }

  void setScale(float scale);
  void setScale(float scale, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);
  void setScale(float scaleX, float scaleY);
  void setScale(float scaleX, float scaleY, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);

  [[nodiscard]] float scaleX() const noexcept {
    return mScaleX;
  }

  [[nodiscard]] float scaleY() const noexcept {
    return mScaleY;
  }

  /** Clockwise rotation in degrees around the layer's centre. */
  void setRotation(float degrees);
  void setRotation(float degrees, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);

  [[nodiscard]] float rotation() const noexcept {
    return mRotation;
  }

  /** Rounded clip radius in logical units; 0 keeps the rectangular clip. */
  void setClipRadius(float radius);
  [[nodiscard]] float clipRadius() const noexcept { return mClipRadius; }

  /**
   * Clips this layer (and its children) to @p path, in the layer's logical space.
   *
   * `mClip` must also be true for a path clip to apply, matching the rectangle semantics. The clip is
   * rendered through the layer-mask route (`Canvas::maskLayer`), so it works on both the GPU and the
   * raster backend; hit testing uses the same path. An empty path clears it.
   */
  void setClipPath(Path path);
  [[nodiscard]] const Path& clipPath() const noexcept {
    return mClipPath;
  }
  void clearClipPath();

  /**
   * How this layer's group meets what is underneath it.
   *
   * The mode is applied to the layer as a **group** (`saveLayer`), not to each child: two overlapping
   * children must be composited once. `kSrcOver` is the default and costs no layer. A mode the
   * renderer does not support is ignored (the group is drawn source-over) rather than approximated.
   */
  void setBlendMode(BlendMode mode);
  [[nodiscard]] BlendMode blendMode() const noexcept {
    return mBlendMode;
  }

  /** Draws a soft shadow behind this layer's bounds before its own content. */
  void setShadow(Shadow shadow, float cornerRadius = 4.0f);
  void clearShadow();
  [[nodiscard]] bool hasShadow() const noexcept { return mShadow.has_value(); }
  [[nodiscard]] const Shadow& shadow() const noexcept {
    static const Shadow empty;
    return mShadow.has_value() ? *mShadow : empty;
  }

  /**
   * Whether this layer's own paint is confined to `paintBounds()`.
   *
   * The draw pass skips a child whose paint does not intersect the clip, which is what keeps a long
   * scrolled list from painting every row it has. A layer that paints outside its bounds -- a custom
   * `drawSelf`, a decoration that overhangs, a shadow drawn by hand -- must say so, or it will vanish
   * the moment the edge of its bounds leaves the clip. `setShadow` is accounted for automatically;
   * this is the escape hatch for everything the framework cannot know.
   */
  void setClipsToBounds(const bool clipsToBounds) noexcept {
    mClipsToBounds = clipsToBounds;
  }
  [[nodiscard]] bool clipsToBounds() const noexcept {
    return mClipsToBounds;
  }
  /**
   * Extra logical pixels around the bounds this layer may paint into.
   *
   * The default covers antialiasing, glyph ink overhang and a thin stroke on the edge. A layer that
   * needs a wider halo (a manually drawn glow, a drop shadow on a child, an italic face with a large
   * overhang) raises it instead of turning culling off for the whole subtree.
   */
  void setPaintBoundsInflation(const float inflation) noexcept {
    mPaintBoundsInflation = std::max(0.0f, inflation);
  }
  [[nodiscard]] float paintBoundsInflation() const noexcept {
    return mPaintBoundsInflation;
  }
  /** The rectangle this layer may paint into, in its parent's coordinates (transform applied). */
  [[nodiscard]] Rect paintBounds() const;
  /**
   * True when this layer's paint reaches a clip given in this layer's **parent** coordinates.
   *
   * The draw pass converts the device clip once per container (`scale`+`translate` inverts exactly) and
   * then calls this per child; that is why the check is four adds and four compares for a plain layer
   * instead of a matrix multiply and a rectangle construction.
   */
  [[nodiscard]] bool intersectsClip(float left, float top, float right, float bottom) const;

  /**
   * Draw culling switch.
   *
   * Off is only useful for an A/B pixel comparison: every layer is drawn even when it is outside the
   * clip, so two captures of the same scene must be identical.
   */
  static void setDrawCullingEnabled(bool enabled) noexcept;
  [[nodiscard]] static bool drawCullingEnabled() noexcept;
  /** Children skipped by the draw pass since the counter was reset (one frame's worth). */
  static uint32_t culledLayers() noexcept;
  static void resetCulledLayers() noexcept;

  /**
   * Records that the parent's layout assigned this layer's size on both axes.
   *
   * A container calls this after positioning a child whose size it derived from the available space
   * rather than from the child's measurement (a grow item, a percentage, a stretched cross axis).
   * That is the precondition for the child being a layout boundary: content changes inside it cannot
   * change the size it was given. A fit-content child must be left false.
   */
  void noteParentAssignedSize(const bool assigned) noexcept {
    mParentAssignedSize = assigned;
  }
  [[nodiscard]] bool parentAssignedSize() const noexcept {
    return mParentAssignedSize;
  }

  /**
   * The normalized point (0..1 in each axis) transformations rotate and scale around.
   *
   * `{0.5, 0.5}` is the centre, which is the historical behaviour; `{0, 0}` is the top-left corner,
   * which is what a progress fill or an anchored scale usually wants. Hit testing uses the same
   * origin, so the visible shape and the clickable shape stay one thing.
   */
  void setTransformOrigin(const Point& origin);
  [[nodiscard]] const Point& transformOrigin() const noexcept {
    return mTransformOrigin;
  }

  /**
   * Animates a compositing property until @p duration has elapsed.
   *
   * The window's `onFrame` drives the animation; a live window already calls it once per frame.
   * Calling a setter for the same property cancels its animation.
   */
  void animateOpacity(float target, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);
  void animateTranslation(const Point& target, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);
  void animateScale(float target, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);
  void animateRotation(float target, std::chrono::milliseconds duration,
    Easing easing = Easing::EaseOut);

  /**
   * Animates the layer's rectangle to @p target.
   *
   * This is the layout-animation primitive: a container that moves/resizes a child calls this instead
   * of `setBounds`, and the frame loop interpolates all four edges. A zero duration snaps.
   */
  void animateBounds(const Rect& target, std::chrono::milliseconds duration,
    std::shared_ptr<const Curve> curve = {});
  [[nodiscard]] bool animatingBounds() const noexcept {
    return mBoundsAnimation.active;
  }

  /**
   * The paint and hit-test order relative to the siblings of this layer.
   *
   * Layout and measure still follow insertion order; only painting and hit testing are sorted, so a
   * popup can be raised above its siblings without reworking the layout tree.
   */
  void setZIndex(int zIndex) noexcept {
    mZIndex = zIndex;
    setNeedsPaint();
  }

  [[nodiscard]] int zIndex() const noexcept {
    return mZIndex;
  }

  /**
   * The largest area, in device pixels squared, whose subtree is recorded and replayed instead of
   * redrawn every frame.
   *
   * A layer with children that does not animate can be recorded once and appended to the next frame's
   * list, which trades the cost of redrawing its children for the cost of replaying one list. That
   * trade only wins for small subtrees: a measured A/B on the editor scene showed that raising the
   * limit from 512^2 to 2048^2 makes steady-state frames slower (p50 0.96 -> 1.09 ms), because
   * replaying a large list costs more per frame than re-recording its children.
   *
   * The setter exists so a benchmark can compare the two sides of that trade on one scene without a
   * rebuild (`frame_benchmark --cache-limit`); the default is the measured value and a product should
   * not change it without the same measurement.
   */
  static void setRecordingCacheAreaLimit(float areaPixelsSquared) noexcept;
  [[nodiscard]] static float recordingCacheAreaLimit() noexcept;

  /**
   * Marks this layer as a repaint boundary.
   *
   * A boundary keeps its own recording and its ancestors stop caching across it: they draw live and
   * append the boundary's current list, so a change inside one boundary never invalidates (or replays
   * stale) content outside it. Without boundaries nothing changes — the whole-tree recording path is
   * still the default.
   *
   * This is the seam the compositor stages build on: a boundary is the unit that can later be
   * replayed, moved or re-composited without touching the rest of the frame.
   */
  void setRepaintBoundary(bool boundary);
  [[nodiscard]] bool isRepaintBoundary() const noexcept {
    return mRepaintBoundary;
  }
  /** True when a descendant is a boundary; such a layer cannot record across it. */
  [[nodiscard]] bool hasRepaintBoundaryDescendant() const;
  /**
   * True when this layer or a descendant is a boundary.
   *
   * Maintained by the setters and by child insertion/removal, so the frame assembly can ask "does this
   * tree have any boundary" without walking it every frame.
   */
  [[nodiscard]] bool subtreeHasBoundary() const noexcept {
    return mSubtreeHasBoundary;
  }
  /** True when a repaint boundary lives somewhere below this layer (not counting this layer). */
  [[nodiscard]] bool hasBoundaryDescendant() const noexcept {
    return mHasBoundaryDescendant;
  }
  /** Used by the ancestry refresh in Layer.cpp; not part of the layout or paint contract. */
  void noteSubtreeHasBoundary(bool has) noexcept {
    mSubtreeHasBoundary = has;
  }
  void noteHasBoundaryDescendant(bool has) noexcept {
    mHasBoundaryDescendant = has;
  }

  /**
   * The memory budget for recorded display lists, in bytes.
   *
   * The area limit above is a per-subtree trade (replaying a very large list can cost more than
   * re-recording its children); the budget is the aggregate bound that area cannot express: many small
   * subtrees stay under the per-layer area yet together hold megabytes of commands. When a recording
   * pushes the total over the budget, the least recently used recordings are dropped and re-recorded on
   * demand, so the budget is a ceiling on memory rather than a hard failure.
   *
   * Zero disables recording entirely. The default is 64 MiB.
   */
  static void setRecordingCacheBudgetBytes(std::size_t bytes) noexcept;
  [[nodiscard]] static std::size_t recordingCacheBudgetBytes() noexcept;
  /** Approximate bytes held by recorded subtrees right now; see the per-list estimate. */
  [[nodiscard]] static std::size_t recordingCacheBytesInUse() noexcept;
  /** How many subtrees hold a recording right now. */
  [[nodiscard]] static std::size_t recordingCacheEntryCount() noexcept;
  /**
   * How many child segments have been re-recorded in place instead of dropping the whole recording.
   *
   * This is the property-level invalidation counter: a change inside one child of a recorded container
   * should bump this by one, not throw away the container list.
   */
  [[nodiscard]] static std::size_t recordingSegmentRebuildCount() noexcept;

  /**
   * Whether a hit inside this layer's bounds reports the layer itself.
   *
   * Children are still hit-tested when this is false: a transparent panel can pass clicks through to
   * its own children while not swallowing clicks on its empty area.
   */
  void setHitTestable(const bool hitTestable) noexcept {
    mHitTestable = hitTestable;
  }

  [[nodiscard]] bool hitTestable() const noexcept {
    return mHitTestable;
  }

  /**
   * Makes this subtree a focus scope: while it is visible, Tab and click focus stay inside it.
   *
   * Dialogs and modal panels use this. It is a modal constraint, not just an ordering hint: the
   * topmost visible scope wins, and a click outside it cannot move focus out of the scope.
   */
  void setFocusScope(const bool scope) noexcept {
    mFocusScope = scope;
  }

  [[nodiscard]] bool isFocusScope() const noexcept {
    return mFocusScope;
  }

  /**
   * Priority among sibling/overlapping focus scopes.
   *
   * Window picks the highest priority first, then the deeper scope, then paint order. The default 0
   * preserves the historical "topmost visible scope" behavior; a modal can raise itself without
   * relying on z-order.
   */
  void setFocusScopePriority(int priority) noexcept {
    mFocusScopePriority = priority;
  }
  [[nodiscard]] int focusScopePriority() const noexcept {
    return mFocusScopePriority;
  }

  /**
   * Explicit Tab order within a focus scope. Lower values come first; 0 is the default and keeps
   * paint order. This is independent of the layer's DOM/child order.
   */
  void setTabOrder(int order) noexcept {
    mTabOrder = order;
  }
  [[nodiscard]] int tabOrder() const noexcept {
    return mTabOrder;
  }

  /**
   * The window this layer is attached to, or null.
   *
   * Set by `Window::setRoot` and inherited by children, so a control can reach window services
   * (posting, drag sessions, clipboard routing) without the caller threading a pointer through every
   * constructor.
   */
  [[nodiscard]] Window* window() const noexcept {
    return mWindow;
  }
  /** Sets the window back-pointer for this layer and its subtree; called by the window itself. */
  void attachToWindow(Window* window);
  /** Called when the window back-pointer changes, including detach to null. */
  virtual void onWindowChanged(Window* previous, Window* current) {
    (void)previous;
    (void)current;
  }

  void addChild(std::shared_ptr<Layer> child);
  /**
   * Inserts @p child at @p index (clamped to the child count).
   *
   * `addChild` is the append form. The reconciler uses this to reorder keyed children with a move
   * instead of detaching and re-attaching the whole tail, which would drop focus/scroll/animation
   * state for children that did not actually change.
   */
  void insertChild(size_t index, std::shared_ptr<Layer> child);
  void removeChild(const std::shared_ptr<Layer>& child);

  void setLayoutParams(const LayoutParams& params);

  [[nodiscard]] const LayoutParams& layoutParams() const noexcept {
    return mLayoutParams;
  }

  /**
   * Applies a theme to this subtree.
   *
   * The pointer is shared so a window can replace its theme without invalidating nodes that are
   * still using it. Each layer receives `applyTheme`, and children inherit the same pointer when
   * they are added later.
   */
  void setTheme(std::shared_ptr<const Theme> theme);

  /** The declarative style sheet this layer resolves `styleName` against; null uses the theme copy. */
  void setStyleSheet(std::shared_ptr<const StyleSheet> sheet);
  /** Propagates a sheet to this layer and its non-explicit descendants (see `setStyleSheet`). */
  void inheritStyleSheet(std::shared_ptr<const StyleSheet> sheet);
  [[nodiscard]] const std::shared_ptr<const StyleSheet>& styleSheet() const noexcept {
    return mStyleSheet;
  }
  void setStyleName(std::string name);
  [[nodiscard]] const std::string& styleName() const noexcept {
    return mStyleName;
  }

  /**
   * Interaction states this layer reports to the style resolver.
   *
   * The base is empty: a control that can be hovered/pressed/focused/disabled overrides it so the
   * same `Style` variants drive every control without a per-control style implementation.
   */
  [[nodiscard]] virtual StateMask styleStates() const;
  /**
   * Enables or disables this subtree.
   *
   * A disabled layer keeps painting (usually in its disabled style) but takes no pointer or key input,
   * and neither do its children: a disabled panel does not leave one live button inside it. The state
   * is reported through `styleStates()` so a sheet's `disabled` variant applies without the control
   * doing anything.
   */
  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const noexcept {
    return mEnabled;
  }
  /** True when this layer and every ancestor is enabled. */
  [[nodiscard]] bool inputEnabled() const noexcept;
  /** Called after `setEnabled` flipped the flag; controls drop transient state here (pressed, ...). */
  virtual void onEnabledChanged(bool enabled) {
    (void)enabled;
  }

  /**
   * The name a sheet rule can address this control by when the caller set no explicit name.
   *
   * A control returns its kind (`"button"`, `"checkbox"`, ...) so a sheet can style every button
   * without the page naming them one by one; a layer that returns nullptr is only reachable through
   * `setStyleName`. An explicit name always wins.
   */
  [[nodiscard]] virtual const char* defaultStyleName() const {
    return nullptr;
  }
  /** Resolves the style for the current states; null when no sheet or no name is configured. */
  [[nodiscard]] std::optional<ResolvedStyle> resolvedStyle() const;

  [[nodiscard]] const Theme* theme() const noexcept {
    return mTheme.get();
  }

  /**
   * Translates @p key through the theme's translator, falling back to @p fallback.
   *
   * A control should not build user-visible text from a bare literal: the theme carries the
   * translator, so `tr("ui.cut", "Cut")` works for any control that has a theme, including one whose
   * caller never configured i18n.
   */
  [[nodiscard]] std::string tr(std::string_view key, std::string_view fallback) const {
    static const Theme fallbackTheme = Theme::dark();
    return (mTheme != nullptr ? *mTheme : fallbackTheme).tr(key, fallback);
  }

  [[nodiscard]] float focusRingWidth() const noexcept {
    return mFocusRingWidth;
  }

  /**
   * The children in paint order: `zIndex` ascending, ties broken by insertion order.
   *
   * The single source of that order: recording, hit testing and frame assembly all read it, so a
   * container cannot paint in one order and be hit-tested in another.
   */
  [[nodiscard]] std::vector<Layer*> paintOrderedChildren() const;

  [[nodiscard]] const std::vector<std::shared_ptr<Layer>>& children() const noexcept {
    return mChildren;
  }

  /** The parent layer, or null for a root/window layer. */
  [[nodiscard]] Layer* parent() const noexcept {
    return mParent;
  }

  /**
   * How many times this layer has been attached or detached from a parent.
   *
   * Diagnostics for reconciler/container churn: a keyed reorder that preserves a child must not
   * change its count, while a replaced child starts at zero.
   */
  [[nodiscard]] uint64_t reparentCount() const noexcept { return mReparents; }

  /**
   * Measures this layer for @p available device pixels and returns its preferred size.
   *
   * A container measures its children; a leaf returns the size its content needs. The result is also
   * stored as `measuredSize`, which the parent's layout pass reads.
   */
  virtual Size measure(const Size& available);
  /**
   * Measures against a constraint box.
   *
   * The default adapts to `measure(const Size&)` so every existing container keeps working; a container
   * that wants the minimums or the explicit "unbounded" overrides this one. The name is not an overload
   * of `measure` on purpose: `measure({})` is used throughout the layout code and a `Constraints`
   * overload would make every braced argument ambiguous.
   */
  virtual Size measureWithConstraints(const Constraints& constraints);

  /**
   * The layer's preferred content size, independent of the offered width; {-1,-1} means unknown.
   *
   * A text layer reports its single-line size, a Flex its children's intrinsic sum. Layouts use it
   * when they need "fit the content" instead of "fill the slot"; a container that cannot derive a
   * definite value answers {-1,-1} rather than guessing.
   */
  [[nodiscard]] virtual Size intrinsicSize() const;

  /** Distance from the layer top to its first text baseline, or -1 when it has none. */
  [[nodiscard]] virtual float baselineOffset() const;

  /**
   * Measures with a one-entry cache keyed by the available size.
   *
   * Containers call this instead of `measure` while laying out; the cache is invalidated by
   * `setNeedsLayout` and by a bounds change, which is where a control announces that its content or
   * width-dependent layout changed. The base `measure` stays virtual and uncached so a subclass keeps
   * full control when it needs to recompute.
   */
  [[nodiscard]] Size measureCached(const Size& available);
  [[nodiscard]] Size measureCachedWithConstraints(const Constraints& constraints);
  void invalidateMeasureCache();

  /** Arranges children inside this layer's bounds. Parents call it after assigning the bounds. */
  virtual void layout();

  /**
   * A solid fill painted behind this layer's own content and children.
   *
   * The default is transparent. It is the primitive an Element style maps to; it participates in the
   * layer's transform and clip like the content does.
   */
  void setBackgroundColor(Color color);
  [[nodiscard]] Color backgroundColor() const noexcept { return mBackground; }

  /**
   * Paints the background from a theme token instead of a fixed colour.
   *
   * The role is re-resolved by `applyTheme`, so a light/dark switch updates the layer in place.
   * Setting an explicit background colour clears the role.
   */
  void setBackgroundColorRole(ThemeColorRole role);
  [[nodiscard]] ThemeColorRole backgroundColorRole() const noexcept { return mBackgroundRole; }

  /** Draws this layer and its children into the frame. */
  virtual void draw(RenderContext& context);

  /**
   * Called once per frame before drawing, with the frame's wall-clock time.
   *
   * This is the hook a transient layer (Toast) or a future animation uses; the default forwards to
   * visible children. It runs on the UI thread, like the rest of the tree.
   */
  virtual void onFrame(std::chrono::steady_clock::time_point now);

  /**
   * Called when this layer's own visibility changes.
   *
   * A content view uses it to tell its producer to stop (or resume) rendering: a preview that is
   * hidden behind a tab, collapsed or scrolled out of its viewport should not keep decoding. Note
   * that this is the layer's *own* flag; an invisible ancestor does not call it.
   */
  virtual void onVisibilityChanged(bool visible);

  /**
   * Called after a device loss and surface recreate.
   *
   * A layer that owns device-local resources drops them here; the default invalidates the cached
   * clip mask and recordings and forwards to children. A `ContentView` also clears the published
   * texture, because the old handle belongs to the lost device.
   */
  virtual void onDeviceRecovered();

  /** Advances this layer's property animations; called by `onFrame`. */
  void updateAnimations(std::chrono::steady_clock::time_point now);

  /** True when this layer or a visible descendant has a property animation in flight. */
  /**
   * True when this layer or any visible descendant is animating.
   *
   * The answer is memoised per draw pass: `cacheWhole` asks this for every layer it considers, and an
   * unmemoised walk makes a frame pay the subtree size once per nesting level. `beginAnimationFrame()`
   * opens a new pass; the window calls it right before drawing.
   */
  [[nodiscard]] virtual bool hasRunningAnimation() const;
  /** Starts a new memoisation pass for `hasRunningAnimation` (called by the window before `draw`). */
  static void beginAnimationFrame() noexcept;

  /** @return the topmost visible layer at @p point, or null when the point is outside this subtree. */
  [[nodiscard]] Layer* hitTest(const Point& point);

  virtual bool onPointerDown(const PointerEvent& event);
  virtual bool onPointerUp(const PointerEvent& event);
  virtual bool onPointerMove(const PointerEvent& event);
  virtual bool onPointerLeave();
  virtual bool onKeyDown(const KeyEvent& event);
  virtual bool onKeyUp(const KeyEvent& event);
  /** Wheel/trackpad scroll at @p event's position. */
  virtual bool onScroll(const ScrollEvent& event);
  /** Trackpad magnification/rotation/swipe; the first layer that handles it consumes the gesture. */
  virtual bool onGesture(const GestureEvent& event);
  /** A file drop at @p event's position; return true when the layer consumed it. */
  virtual bool onDrop(const DropEvent& event);
  /**
   * The pointer entered this layer while a drag is in progress.
   *
   * Returning `DragEffect::None` means "not a target": the window then offers the drag to the
   * parent. A layer that returns an effect becomes the drag target and receives `onDragOver` for
   * subsequent moves and `onDrop` on release.
   */
  virtual DragEffect onDragEnter(const DragEvent& event);
  /** The pointer moved over the current drag target; return the effect it would accept. */
  virtual DragEffect onDragOver(const DragEvent& event);
  /** The drag left this layer, moved to another target, or was cancelled. */
  virtual void onDragLeave();

  /** The payload a platform drag-out would carry, or nothing when this press is not a drag-out. */
  using DragOutProvider = std::function<std::optional<DragPayload>()>;
  /**
   * Makes a press on this layer the start of a platform drag-out.
   *
   * The window asks the provider once, right after the press is dispatched; returning nothing means
   * "handle the press normally" (a text field that only drags when the press lands inside an existing
   * selection does exactly that). When it returns a payload and the pointer then moves past a small
   * threshold, the window calls `Window::beginPlatformDrag` instead of routing that move to the layer.
   * Platforms without a drag-out entry point leave the gesture to the layer, so the widget keeps its
   * normal selection behaviour.
   */
  void setDragOutProvider(DragOutProvider provider);
  [[nodiscard]] const DragOutProvider& dragOutProvider() const noexcept {
    return mDragOutProvider;
  }
  /** A character from the platform's text input (WM_CHAR, Wayland text-input, IME commit). */
  virtual bool onTextInput(char32_t character);
  /** An IME composition string (UTF-8), shown but not yet part of the text. */
  virtual bool onTextComposition(const std::string& text);
  /** An IME commit string (UTF-8). */
  virtual bool onTextCommit(const std::string& text);
  /** An IME deletion around the cursor: byte offset (negative = before) and byte length. */
  virtual bool onTextDelete(int32_t index, uint32_t length);
  /** True when this layer owns the platform text-input/IME focus (TextField, future editor). */
  [[nodiscard]] virtual bool acceptsTextInput() const;
  /** True for a transient overlay that should receive keys before the normal focus chain. */
  [[nodiscard]] virtual bool isPopup() const {
    return false;
  }
  /** True for the window's overlay host: it owns transient surfaces and may draw a modal scrim. */
  [[nodiscard]] virtual bool isOverlay() const {
    return false;
  }
  /** The IME candidate anchor in logical coordinates; an empty rect means "not available". */
  [[nodiscard]] virtual Rect imeCursorRect() const {
    return {};
  }

  /** The platform-facing name; an empty name means the layer is not exposed by name. */
  void setAccessibleName(std::string name) {
    mAccessibleName = std::move(name);
  }
  [[nodiscard]] const std::string& accessibleName() const noexcept {
    return mAccessibleName;
  }
  void setAccessibleDescription(std::string description) {
    mAccessibleDescription = std::move(description);
  }
  [[nodiscard]] const std::string& accessibleDescription() const noexcept {
    return mAccessibleDescription;
  }
  void setAccessibleRole(AccessibilityRole role) noexcept {
    mAccessibleRole = role;
  }
  [[nodiscard]] AccessibilityRole accessibleRole() const noexcept {
    return mAccessibleRole;
  }

  /**
   * The node value a bridge reports: text field contents, a slider reading, the state of a toggle.
   *
   * A platform bridge maps it onto the accessibility value property; an empty string means the node
   * has no value, which is different from a value that happens to be empty (a text field) and is why
   * `accessibleEditable` exists separately.
   */
  void setAccessibleValue(std::string value) {
    mAccessibleValue = std::move(value);
  }
  [[nodiscard]] const std::string& accessibleValue() const noexcept {
    return mAccessibleValue;
  }
  /** Selection state for list items, table rows and toggles. */
  void setAccessibleSelected(const bool selected) noexcept {
    mAccessibleSelected = selected;
  }
  [[nodiscard]] bool accessibleSelected() const noexcept {
    return mAccessibleSelected;
  }
  /** Whether the node accepts edits; a text field is editable even when its value is empty. */
  void setAccessibleEditable(const bool editable) noexcept {
    mAccessibleEditable = editable;
  }
  [[nodiscard]] bool accessibleEditable() const noexcept {
    return mAccessibleEditable;
  }

  /**
   * Performs an accessibility action on this layer, as a platform bridge would.
   *
   * A bridge cannot know how a control works, so a control that exposes a pattern implements this:
   * `Invoke` for a button, `Toggle` for a checkbox, `SetValue` with the text for a text field. The
   * default refuses, so the bridge can return "no pattern" honestly instead of pretending.
   */
  virtual bool performAccessibilityAction(AccessibilityAction action, std::string_view value);

  /**
   * The numeric range this control exposes, if any (a slider, a progress bar).
   *
   * A platform bridge uses it to implement its range pattern; the default is "no range".
   */
  [[nodiscard]] virtual std::optional<AccessibleRange> accessibleRange() const {
    return std::nullopt;
  }
  /** The scrollable viewport this control exposes, if any (a scroll view). */
  [[nodiscard]] virtual std::optional<AccessibleScroll> accessibleScroll() const {
    return std::nullopt;
  }
  /** The grid shape this control exposes, if any (a table, a virtualised list). */
  [[nodiscard]] virtual std::optional<AccessibleGrid> accessibleGrid() const {
    return std::nullopt;
  }
  /** The text selection of an editable control, in UTF-16 code units. */
  [[nodiscard]] virtual std::optional<AccessibleTextSelection> accessibleTextSelection() const {
    return std::nullopt;
  }

  /**
   * What `Select` does for this layer, when it represents a selectable row.
   *
   * A list or table owns the selection, so the row cannot toggle itself; it installs a callback that
   * asks its owner to select it, and the bridge only has to route the action.
   */
  void setAccessibleOnSelect(std::function<void()> callback) {
    mAccessibleOnSelect = std::move(callback);
  }

  /** True when a click on this layer should take keyboard focus. */
  [[nodiscard]] virtual bool focusable() const {
    return false;
  }

  /**
   * True when a press that this layer handles should capture the pointer until release.
   *
   * Sliders and split panes use it so a drag keeps receiving move/up events even after the pointer
   * leaves their bounds. The window owns the capture lifetime, not the layer.
   */
  [[nodiscard]] virtual bool capturesPointerOnPress() const {
    return false;
  }

  /** Called when this layer gains or loses focus, after `onPointerDown` decides the hit. */
  virtual void onFocusChanged(bool focused);

  /**
   * Monotonic revision of the layer's font/style inputs.
   *
   * Text-bearing controls bump this from setFont; a recycling container can hash it across a row
   * subtree and re-measure when a layer's font changed outside the Element builder.
   */
  [[nodiscard]] virtual uint64_t fontRevision() const noexcept {
    return mFontRevision;
  }

  /** Bumps the base font revision; custom font-bearing layers call this from setFont. */
  void markFontChanged() noexcept {
    ++mFontRevision;
  }

  /** Drops the recording (if any) and returns its bytes to the budget. */
  void releaseRecording();
  /**
   * Takes ownership of a freshly recorded list and registers it in the budget, evicting the least
   * recently used recordings first when it does not fit. Returns false when the list must be replayed
   * directly instead of cached (disabled, larger than the whole budget, or empty).
   */
  bool cacheRecording(std::shared_ptr<detail::LayerRecording> recording, std::size_t bytes);
  /** Marks the segment that belongs to @p child dirty; false when this layer cannot patch it. */
  bool markSegmentDirty(const Layer* child);
  /** Adds @p delta to the accounted bytes of the current recording (the budget stays accurate). */
  void adjustRecordingBytes(std::ptrdiff_t delta);

  void setNeedsLayout();
  void setNeedsPaint();

  /**
   * Called by a parent that skipped this layer's draw because it is outside the clip.
   *
   * A dirty layer that was skipped must not stay "painted": its recording is dropped and its flags are
   * cleared (so the frame can still be clean), and the next time it is inside the clip it re-records
   * itself. A clean layer is left alone -- its recording is still valid.
   */
  void deferPaint();
  /**
   * Drops the memoised `hasRunningAnimation` answer for this layer and its ancestors.
   *
   * Starting an animation between two draw passes (a test, an event handler) must not be hidden by a
   * value cached for the pass that already ended.
   */
  void invalidateAnimationCache();

  [[nodiscard]] bool needsLayout() const noexcept {
    return mNeedsLayout;
  }
  /**
   * True when some descendant needs a layout pass, even though this layer's own arrangement is fine.
   *
   * The flag lets the window run a layout pass without treating the whole tree as dirty; a layout
   * boundary stops the "my arrangement is stale" flag from climbing further.
   */
  [[nodiscard]] bool subtreeNeedsLayout() const noexcept {
    return mSubtreeNeedsLayout;
  }
  /**
   * Marks this layer as a layout boundary: a change inside it cannot change its own size or the
   * position of anything outside it.
   *
   * A scroll viewport qualifies (its size comes from its bounds, and its content is positioned by the
   * scroll offsets), which is why a text edit inside a long page no longer re-measures the page above
   * it. Containers whose arrangement depends on their children's sizes must not claim this.
   */
  [[nodiscard]] virtual bool isLayoutBoundary() const {
    return false;
  }
  /**
   * True when this layer contributes an item to a plan-mode frame itself instead of being lent as one
   * opaque list.
   *
   * A scroll viewport is the case: it lends its content with the scroll offset as a translation, so the
   * frame has to let it draw (and register that item) rather than replay a list recorded at the old offset.
   */
  [[nodiscard]] virtual bool plansItsOwnContent() const {
    return false;
  }

  /**
   * True when the parent has already fixed both of this layer's dimensions.
   *
   * That is the precondition for being a layout boundary: if the parent assigned the size (a pair of
   * percentages, or both edges of an absolute box), nothing this layer measures about its own children
   * can change its size, so an edit inside it cannot invalidate the parent. A `basis` alone is not
   * enough -- it pins the main axis while the cross axis still comes from the line.
   */
  [[nodiscard]] bool hasFixedParentSize() const noexcept {
    // A container that assigned both axes from the space it had (grow, percentage, stretch) fixed this
    // layer's size just as firmly as a pair of percentages does; the percentage-only test below missed
    // every layout whose size comes from the parent's distribution, which is the common case.
    if (mParentAssignedSize) return true;
    const LayoutParams& params = mLayoutParams;
    if (params.position == Position::Absolute) {
      const bool widthFixed = params.widthPercent >= 0.0f || (params.hasLeft && params.hasRight);
      const bool heightFixed = params.heightPercent >= 0.0f || (params.hasTop && params.hasBottom);
      return widthFixed && heightFixed;
    }
    return params.widthPercent >= 0.0f && params.heightPercent >= 0.0f;
  }

  [[nodiscard]] bool needsPaint() const noexcept {
    return mNeedsPaint;
  }

  /**
   * @return true when this layer or any descendant asked for a repaint since the last `draw`.
   *
   * `needsPaint` on its own only answers for this layer, which is not the question a window asks
   * before reusing a retained recording: a child's change has to bubble up. `setNeedsPaint` sets this
   * flag on the layer and every ancestor; `draw` clears it after the subtree has been painted.
   */
  [[nodiscard]] bool subtreeNeedsPaint() const noexcept {
    return mSubtreeNeedsPaint;
  }

  /** @return true when a leaf's `drawSelf` has a valid device-space recording. */
  /**
   * Records this subtree into a fresh list whose origin is this layer's top-left corner.
   *
   * This is the recording primitive a composited container needs: a scroll viewport has to own its
   * content's list at a *fixed* origin before it can lend it with a translation, whereas drawing into the
   * frame's canvas bakes the current offset into the geometry. The list is immutable and independent of
   * any canvas, so it can be submitted later, on this thread or another.
   *
   * @param size  device-pixel size the recording is made for (clips are resolved against it)
   * @param scale UI scale the canvas applies, normally `Window::scale()`
   * @return the list, or null when the device cannot record
   */
  /**
   * Records this subtree into a fresh list, placing its top-left corner at @p origin in device pixels.
   *
   * The default (an empty origin) places it at the corner's own device position, which is what
   * `recordInto(size, scale)` has always done. A caller that wants a *whole-pixel* replay translation
   * passes the corner rounded down, so the fractional part is baked into the list instead of the
   * translation: the atlas places coverage on whole pixels, so a fractional translation cannot be
   * expressed.
   *
   * @param clip  Optional logical-space rectangle (same space as @p origin). The recording is clipped
   *              to it *before* drawing, so the draw pass's culling skips everything outside and the
   *              list only holds what the clip shows. A tiled viewport records one band per tile this
   *              way; an empty rectangle records the whole subtree.
   */
  std::shared_ptr<const graphics::gpu::DrawList> recordInto(Size size, float scale, Point origin = {},
    Rect clip = {});

  /**
   * Makes @p list this layer's recording from now on.
   *
   * Used by a container that records its child elsewhere (`recordInto`) and then hands the result back:
   * the child's recording is in the container's fixed-origin coordinates, so it must not be re-recorded
   * by the normal draw path until something marks it dirty.
   */
  void adoptRecording(std::shared_ptr<const graphics::gpu::DrawList> list);

  /** Repaint boundaries that rebuilt their list since the counter was reset (one frame's worth). */
  static uint32_t repaintedBoundaries() noexcept;
  static void resetRepaintedBoundaries() noexcept;
  /**
   * Bytes of recorded lists copied into the frame since the counter was reset.
   *
   * Every appended list is a copy; R3's "borrow the boundary list instead of copying it" would drive
   * this to zero for a static frame, which is what makes it the payoff metric for that stage.
   */
  static std::size_t frameListBytes() noexcept;
  static void resetFrameListBytes() noexcept;

  /**
   * Bytes of recorded lists lent to the frame instead of copied into it, since the counter was reset.
   *
   * The complement of `frameListBytes`: this one is what says the frame borrowed a boundary's recording
   * rather than re-recording it, and it is zero whenever nothing was lent.
   */
  static std::size_t lentListBytes() noexcept;
  static void resetLentListBytes() noexcept;

  /**
   * The immutable lists this layer has recorded, ready to be lent to a frame.
   *
   * Segment 0 is the layer's own paint and segments 1..N are its children in paint order, so handing the
   * vector to `Surface::submitLists` preserves the order the tree would have painted. The pointers are
   * immutable: a later change replaces a segment instead of mutating one a frame may still hold.
   * Empty when the layer has no valid recording (or only a leaf's single self list).
   */
  [[nodiscard]] std::vector<std::shared_ptr<const graphics::gpu::DrawList>> borrowRecording() const;

  [[nodiscard]] bool hasCachedRecording() const noexcept {
    return mRecordingValid;
  }

  /**
   * Collects the rectangles changed since the last `clearDamage`, in window coordinates.
   *
   * @return false when a transform in the subtree makes the accumulated local rectangles unsafe to
   *         use (a scale/rotation can move content outside its new bounds); the caller falls back to
   *         a full-window repaint.
   */
  [[nodiscard]] bool collectDamage(Rect& out, Point parentOffset = {}, Rect clip = {}) const;

  /**
   * Counts the nodes that currently want a repaint or a layout pass.
   *
   * This is the cheap observability hook the retained-mode migration measures itself with: a change
   * that only touches one control should make exactly that control paint-dirty, and a value change
   * that cannot affect geometry should leave the layout-dirty count at zero. It walks the tree once
   * and allocates nothing.
   */
  void collectDirtyCounts(uint32_t& paintDirty, uint32_t& layoutDirty) const;
  /** Clears damage in this subtree after a presented frame. */
  void clearDamage();

  [[nodiscard]] const Size& measuredSize() const noexcept {
    return mMeasured;
  }

protected:
  /**
   * @return true when this layer's `drawSelf` may be cached as a device-space DrawList.
   *
   * The cached commands already carry the transform and clip in force at record time, so the cache is
   * keyed on both and is replayed only while they match. Text opts in: its painter bakes the device
   * scale into the glyph positions and applies an inverse local scale, which is exactly what a
   * device-space list replays.
   */
  [[nodiscard]] virtual bool cachesOwnPaint() const { return false; }

  /** @return true when children are clipped to this layer's bounds for damage purposes. */
  [[nodiscard]] virtual bool clipsChildren() const { return mClip; }

  /** Draws this layer's own content. The default draws nothing. */
  virtual void drawSelf(RenderContext& context);

  /** Copies the tokens this layer uses. Controls override it; the default does nothing. */
  virtual void applyTheme(const Theme& theme);

  void setMeasuredSize(const Size& size) noexcept {
    mMeasured = size;
  }

  /** A container calls this when its layout pass has arranged the children. */
  void clearNeedsLayout() noexcept {
    mNeedsLayout = false;
    mSubtreeNeedsLayout = false;
  }

  /**
   * Called after `setBounds` changed this layer's width or height.
   *
   * A layer that lays itself out from its own size — text that wraps, for instance — needs this
   * because `measure()` is asked with the parent's *available* size, while the parent may assign a
   * different rectangle afterwards. Overrides must not call `setBounds` (that would recurse) and
   * should invalidate what they cached for the old size.
   */
  virtual void onBoundsSizeChanged() {}

  /**
   * Clears the paint dirty flags after a `draw` override has painted this subtree.
   *
   * A layer that overrides `draw` (instead of `drawSelf`) owns the whole paint path, so it also owns
   * clearing these flags: without it the layer stays dirty forever, keeps its ancestors' retained
   * frame short-circuit from engaging, and reports a permanent dirty node. `drawSelf` subclasses do not
   * need this — `Layer::draw` clears the flags for them.
   */
  void clearPaintFlags() noexcept {
    mNeedsPaint = false;
    mSubtreeNeedsPaint = false;
  }

  void drawChildren(RenderContext& context);
  /** The uncached body, used directly or inside a whole-subtree recording scope. */
  void drawUncached(RenderContext& context);
  /** Caches a leaf's `drawSelf` as a device-space DrawList and appends it while the state matches. */
  void drawSelfCached(RenderContext& context);
  void addDamage(const Rect& localBounds);

private:
  std::string mAccessibleName;
  std::string mAccessibleDescription;
  std::string mAccessibleValue;
  bool mAccessibleSelected = false;
  bool mAccessibleEditable = false;
  std::function<void()> mAccessibleOnSelect;
  AccessibilityRole mAccessibleRole = AccessibilityRole::Generic;
  Rect mBounds{};
  Size mMeasured{};
  std::vector<std::shared_ptr<Layer>> mChildren;
  std::shared_ptr<detail::LayerRecording> mRecording;
  Matrix mRecordingTransform;
  Rect mRecordingClip;
  /** Bounds at record time; a device-space list is stale after a move even when the transform is not. */
  Rect mRecordingBounds;
  bool mRecordingValid = false;
  Rect mDamage {};
  bool mHasDamage = false;
  bool mDamageUnbounded = false;

  enum class AnimationTarget : uint8_t {
    Opacity,
    TranslationX,
    TranslationY,
    ScaleX,
    ScaleY,
    Rotation,
  };

  struct BoundsAnimation {
    Rect from{};
    Rect to{};
    std::chrono::steady_clock::time_point start{};
    std::chrono::milliseconds duration{ 0 };
    std::shared_ptr<const Curve> curve;
    bool active = false;
  };

  struct Animation {
    AnimationTarget target = AnimationTarget::Opacity;
    float from = 0.0f;
    float to = 0.0f;
    std::chrono::steady_clock::time_point start{};
    std::chrono::milliseconds duration{ 0 };
    Easing easing = Easing::Linear;
  };

  /**
   * A background-colour animation.
   *
   * Colour needs its own record because the value is not a scalar: interpolating the four channels of
   * two colours is a different operation from lerping a float, and the implicit `setTransition` path
   * has to be able to start one from a setter.
   */
  struct ColorAnimation {
    Color from{};
    Color to{};
    std::chrono::steady_clock::time_point start{};
    std::chrono::milliseconds duration{ 0 };
    Easing easing = Easing::EaseOut;
    bool active = false;
  };



  void animate(AnimationTarget target, float from, float to, std::chrono::milliseconds duration,
    Easing easing);
  void cancelAnimation(AnimationTarget target);
  void invalidateClipMask();
  [[nodiscard]] Bitmap* clipMaskFor(RenderContext& context);

  ColorAnimation mColorAnimation;
  std::shared_ptr<const Theme> mTheme;
  std::shared_ptr<const StyleSheet> mStyleSheet;
  std::string mStyleName;
  float mFocusRingWidth = 1.5f;
  LayoutParams mLayoutParams;
  TransitionSpec mTransition;
  std::vector<Animation> mAnimations;
  BoundsAnimation mBoundsAnimation;
  Direction mDirection = Direction::Ltr;
  bool mDirectionSet = false;
  Size mMeasureCacheAvailable{};
  Constraints mMeasureCacheConstraints{};
  bool mMeasureCacheFromConstraints = false;
  Size mMeasureCacheResult{};
  bool mMeasureCacheValid = false;
  mutable std::unique_ptr<Bitmap> mClipMask;
  mutable Size mClipMaskDeviceSize{};
  mutable Rect mClipMaskDeviceBounds{};
  mutable float mClipMaskRadius = -1.0f;
  mutable uint32_t mClipMaskRevision = 0;
  uint32_t mClipPathRevision = 0;
  Layer* mParent = nullptr;
  uint64_t mReparents = 0;
  bool mVisible = true;
  bool mClip = false;
  float mOpacity = 1.0f;
  std::optional<Shadow> mShadow;
  float mShadowRadius = 4.0f;
  /** Halo the draw culling must honour around `mBounds`; see `setPaintBoundsInflation`. */
  float mPaintBoundsInflation = 2.0f;
  /** False means this layer may paint outside `paintBounds()` and is never culled. */
  bool mClipsToBounds = true;
  /** Set when a parent skipped this layer's draw: the next visible draw must re-record. */
  bool mPaintDeferred = false;
  /** Set by the parent's layout when it assigned both axes without consulting this layer's measure. */
  bool mParentAssignedSize = false;
  /** Memoised `hasRunningAnimation` result for the current draw pass. */
  mutable uint32_t mAnimationFrameId = 0;
  mutable bool mAnimationRunning = false;
  Point mTranslation{};
  float mScaleX = 1.0f;
  float mScaleY = 1.0f;
  float mRotation = 0.0f;
  float mClipRadius = 0.0f;
  Path mClipPath;
  Point mTransformOrigin{ 0.5f, 0.5f };
  BlendMode mBlendMode = BlendMode::kSrcOver;
  bool mFocusScope = false;
  int mFocusScopePriority = 0;
  int mTabOrder = 0;
  uint64_t mFontRevision = 0;
  bool mHitTestable = true;
  int mZIndex = 0;
  bool mEnabled = true;
  bool mRepaintBoundary = false;
  bool mSubtreeHasBoundary = false;
  /** A boundary below this layer; the O(1) form of the old recursive scan in cacheWhole. */
  bool mHasBoundaryDescendant = false;
  /** True when the layer got a sheet of its own, so parent propagation must not overwrite it. */
  bool mStyleSheetExplicit = false;
  bool mNeedsLayout = true;
  bool mSubtreeNeedsLayout = false;
  bool mNeedsPaint = true;
  Color mBackground{};
  ThemeColorRole mBackgroundRole = ThemeColorRole::None;
  Window* mWindow = nullptr;
  bool mSubtreeNeedsPaint = true;
  DragOutProvider mDragOutProvider;
};

} // namespace coral::ui

#endif // CORAL_UI_LAYER_H
