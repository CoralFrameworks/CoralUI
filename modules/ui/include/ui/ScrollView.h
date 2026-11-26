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

#ifndef CORAL_UI_SCROLLVIEW_H
#define CORAL_UI_SCROLLVIEW_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace coral::ui {

struct Theme;

/**
 * A clipped viewport over one content layer.
 *
 * The content keeps its natural height and is offset by the scroll position; the view clips to its own
 * bounds and draws a slim scrollbar when the content is taller. Wheel events bubble to it and are
 * clamped, so a nested list does not scroll past either end.
 */
class ScrollView : public Layer {
 public:
  ScrollView();
  ~ScrollView() override;

  void setContent(std::shared_ptr<Layer> content);
  [[nodiscard]] const std::shared_ptr<Layer>& content() const noexcept {
    return mContent;
  }

  void setScrollOffset(float x, float y);
  /**
   * The fill behind the content. A scroll viewport is often wider than its content after a resize;
   * this keeps the empty area the same colour as the content instead of exposing the window clear.
   */
  void setBackgroundColor(Color color) {
    mBackground = color;
    setNeedsPaint();
  }
  /** The preferred viewport height; 0 means "fit the content up to the available height". */
  void setPreferredHeight(float height) {
    mPreferredHeight = std::max(0.0f, height);
    setNeedsLayout();
  }
  [[nodiscard]] float preferredHeight() const noexcept {
    return mPreferredHeight;
  }
  [[nodiscard]] float scrollY() const noexcept {
    return mScrollY;
  }
  [[nodiscard]] float scrollX() const noexcept {
    return mScrollX;
  }

  [[nodiscard]] Size contentSize() const noexcept {
    return mContentSize;
  }

  /**
   * Smoothly scrolls to @p y over @p duration.
   *
   * The animation is time-based and advanced from the frame loop; a wheel event or another
   * `animateScrollTo`/`setScrollOffset` cancels it. A zero duration snaps.
   */
  void animateScrollTo(float y, std::chrono::milliseconds duration,
    std::shared_ptr<const Curve> curve = {});
  void stopScrollAnimation();
  [[nodiscard]] bool scrollAnimating() const noexcept {
    return mScrollAnimation.active;
  }

  /**
   * Starts a momentum fling at @p velocity logical units per second.
   *
   * The frame loop integrates it with exponential friction and stops at a clamp edge, so a trackpad
   * or touch gesture can hand its release velocity here. A wheel event or `setScrollOffset` cancels
   * the fling, consistent with the explicit animation.
   */
  /**
   * Enables rubber-band overscroll on the vertical axis.
   *
   * A wheel (or the fling hand-off) may push the offset past the clamp by up to `setBounceLimit`
   * units; the next frame springs back to the edge, so the gesture is not clipped abruptly.
   */
  void setBounceEnabled(bool enabled);
  [[nodiscard]] bool bounceEnabled() const noexcept {
    return mBounceEnabled;
  }
  void setBounceLimit(float limit);

  void fling(float velocity);
  [[nodiscard]] bool scrollFlinging() const noexcept {
    return mFlingVelocity != 0.0f;
  }
  /** Bytes the viewport's band lists may hold; least-recently-used bands are dropped beyond it. */
  void setBandCacheBudgetBytes(const std::size_t bytes) noexcept {
    mBandBudgetBytes = std::max<std::size_t>(1, bytes);
  }
  /** Slices the content is cut into for the plan path (0 until a plan frame has run). */
  [[nodiscard]] std::size_t bandCount() const noexcept {
    return mBands.size();
  }

  /**
   * Supplies the rows of a lazily built content, for a list too large to materialise.
   *
   * A virtual viewport keeps only the rows near what it shows: `realize` adds, reuses and positions the
   * children of the content host for a logical range, and `contentSize` reports the size the scrollbar
   * and the banding work against. The provider owns its recycling; the viewport only says which range
   * it needs (the viewport plus one viewport of slack on each side) and when to let go.
   */
  class VirtualContent {
   public:
    virtual ~VirtualContent() = default;
    /** The logical size of the whole content; called from measure and layout. */
    [[nodiscard]] virtual Size contentSize(const Size& viewport) = 0;
    /**
     * Ensures the rows overlapping the logical range `[top, bottom)` exist as children of @p host.
     *
     * `host.bounds()` is the content's origin in window space, so a row whose logical top is `rowTop`
     * belongs at `host.bounds().getTop() + rowTop`. Children outside the range should be hidden or
     * pooled; the viewport does not look at them.
     */
    virtual void realize(Layer& host, float top, float bottom) = 0;
    /** The viewport is going away or switching content; release pooled layers here. */
    virtual void detach(Layer& host) {
      (void)host;
    }
  };
  /** Installs a lazily built content. Replaces the current content with a host layer the provider fills. */
  void setVirtualContent(std::shared_ptr<VirtualContent> content);
  [[nodiscard]] bool hasVirtualContent() const noexcept {
    return mVirtualContent != nullptr;
  }
  [[nodiscard]] float flingVelocity() const noexcept {
    return mFlingVelocity;
  }
  void onFrame(std::chrono::steady_clock::time_point now) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  [[nodiscard]] bool capturesPointerOnPress() const override {
    return mDraggingThumb;
  }
  /** The scrollbar thumb rectangle in window coordinates; empty when there is nothing to scroll. */
  [[nodiscard]] Rect scrollbarThumbBounds() const;
  /** The horizontal scrollbar thumb; empty when the content fits horizontally. */
  [[nodiscard]] Rect horizontalScrollbarThumbBounds() const;

  /**
   * Whether this view paints its built-in slim scrollbar. Callers that draw their own thumb (a themed
   * sidebar, for example) can turn the built-in one off without disabling scrolling.
   */
  void setScrollbarVisible(bool visible);
  [[nodiscard]] bool scrollbarVisible() const noexcept { return mScrollbarVisible; }
  [[nodiscard]] bool hasRunningAnimation() const override;

  Size measure(const Size& available) override;
  void layout() override;
  void draw(RenderContext& context) override;
  bool onScroll(const ScrollEvent& event) override;
  [[nodiscard]] bool clipsChildren() const override { return true; }

  /**
   * A viewport is a boundary once its parent fixed its size: its preferred height is content-driven, so
   * an unfixed viewport still has to tell the parent that its measurement may have changed.
   */
  [[nodiscard]] bool isLayoutBoundary() const override {
    return hasFixedParentSize();
  }
  /** A viewport lends its content with the scroll offset as a translation, so it plans its own item. */
  [[nodiscard]] bool plansItsOwnContent() const override {
    return mContent != nullptr;
  }
  /** The viewport state a bridge exposes as its scroll pattern. */
  [[nodiscard]] std::optional<AccessibleScroll> accessibleScroll() const override;
  /**
   * Scrolling for assistive technology: `lineUp|lineDown|lineLeft|lineRight|pageUp|pageDown|pageLeft|pageRight`
   * for relative moves, or `set:<horizontal 0..1>,<vertical 0..1>` for an absolute position.
   */
  bool performAccessibilityAction(AccessibilityAction action, std::string_view value) override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void clampScroll();
  void setScrollOffsetUnclamped(float x, float y);
  void advanceFling(std::chrono::steady_clock::time_point now);
  void updateContentBounds();

  std::shared_ptr<Layer> mContent;
  Size mContentSize{};
  float mPreferredHeight = 0.0f;
  Color mBackground = Color::MakeRGBA8(0, 0, 0, 0);
  Color mThumbColor = Color::MakeRGBA8(148, 163, 184, 180);
  bool mScrollbarVisible = true;
  float mScrollX = 0.0f;
  float mScrollY = 0.0f;
  /** Plan-mode frames position the content with an item translation instead of with its bounds. */
  bool mCompositeScroll = false;
  /** The device scale of the last draw, so a scroll offset can be snapped to the device grid. */
  float mLastScale = 0.0f;
  /**
   * One horizontal slice of the content, recorded at the content's unscrolled origin.
   *
   * A plan frame submits only the slices the viewport overlaps, each with its own translation and the
   * viewport as its clip. The list holds only what its slice shows (the recording clip culls the rest),
   * so a frame's cost is the viewport's, not the content's -- the problem a single whole-content list
   * had: it carried every row of a 2400-row list into the device every frame.
   */
  struct Band {
    float top = 0.0f;
    std::shared_ptr<const graphics::gpu::DrawList> list;
    std::size_t bytes = 0;
    std::uint64_t lastUsed = 0;
  };
  /** Drops every band list; the next frame re-records what it shows. */
  void invalidateBands();
  /**
   * Asks the provider for the rows the viewport can reach and drops the bands when it changed.
   *
   * Called from layout and from both draw paths, because a composite viewport scrolls without a layout.
   */
  void realizeVirtualContent();
  /** Drops least-recently-used bands until the budget holds, never touching @p keep. */
  void trimBandCache(std::size_t keep);
  /**
   * Rebuilds the band geometry when the content, the viewport or the scale changed.
   *
   * Band boundaries are whole *device* pixels relative to the content origin, so two adjacent bands
   * meet exactly and a row straddling the boundary is split without a seam.
   */
  void resizeBandsIfNeeded(float scale);

  std::vector<Band> mBands;
  /**
   * Distance between adjacent bands, one viewport of whole device pixels.
   *
   * Bands are twice that tall and overlap by one stride, so *every* viewport is inside exactly one band
   * (a band covers `[k*stride, (k+2)*stride)` and `scrollY` is in `[k*stride, (k+1)*stride)`). One band
   * per frame is one list per frame, which is the frame's fixed cost; scrolling re-records only when it
   * crosses a stride.
   */
  float mBandStride = 0.0f;
  /** Logical height of one band: two strides. */
  float mBandHeight = 0.0f;
  float mBandScale = 0.0f;
  Size mBandContentSize{};
  float mBandContentTop = 0.0f;
  std::size_t mBandBytes = 0;
  std::uint64_t mBandClock = 0;
  std::size_t mBandBudgetBytes = 32u * 1024u * 1024u;
  /** The lazy content, when one is installed; `mContent` is its host layer. */
  std::shared_ptr<VirtualContent> mVirtualContent;
  /** The logical range the provider was last asked for; `mRealizedBottom < mRealizedTop` means none. */
  float mRealizedTop = 0.0f;
  float mRealizedBottom = -1.0f;
  struct ScrollAnimation {
    float from = 0.0f;
    float to = 0.0f;
    std::chrono::steady_clock::time_point start{};
    std::chrono::milliseconds duration{ 0 };
    std::shared_ptr<const Curve> curve;
    bool active = false;
  };
  ScrollAnimation mScrollAnimation;
  float mFlingVelocity = 0.0f;
  bool mBounceEnabled = false;
  float mBounceLimit = 72.0f;
  bool mBounceReturnPending = false;
  bool mDraggingThumb = false;
  bool mDraggingHorizontalThumb = false;
  float mThumbDragStartY = 0.0f;
  float mThumbDragStartScroll = 0.0f;
  std::chrono::steady_clock::time_point mLastFrame{};
  bool mHasLastFrame = false;
};

} // namespace coral::ui

#endif // CORAL_UI_SCROLLVIEW_H
