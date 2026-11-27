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

/**
 * @file ui_declarative_gallery.cpp
 *
 * A gallery written entirely in the declarative style: the window is one immutable `Element` tree,
 * `State<GalleryState>` holds the model, `ElementHost` coalesces state writes into one reconcile and
 * `Reconciler` patches the mounted `Layer` tree in place.
 *
 * Every page is a plain function from a state snapshot to elements; no page implements a `Layer`
 * subclass. The last page deliberately hosts a few prebuilt widgets through `components::Layer`, and
 * is labelled as the escape hatch, so the boundary of the current declarative slice stays visible.
 *
 * The demo opens a native window by default and renders offscreen with `--headless`, which is how the
 * CTest checks render every page:
 *
 *   ui_declarative_gallery [--headless] [--frames N] [--out FILE] [--section N]
 *                          [--theme dark|light] [--width W] [--height H] [--self-check] [--dump-tree]
 */

#include <ui/Badge.h>
#include <ui/Card.h>
#include <ui/Chip.h>
#include <ui/Element.h>
#include <ui/ElementHost.h>
#include <ui/ElementRegistry.h>
#include <ui/ElementSpec.h>
#include <ui/OverlayHost.h>
#include <ui/Toast.h>
#include <ui/RecyclingListView.h>
#include <ui/State.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <core/observer/Observation.h>

#include <graphics/Bitmap.h>
#include <graphics/Data.h>
#include <graphics/Font.h>
#include <graphics/Image.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace {

using namespace coral::ui;

using FontPtr = std::shared_ptr<const graphics::Font>;

/** The four type sizes the page composition uses; controls keep their own theme sizes. */
struct Fonts {
  FontPtr body;
  FontPtr section;
  FontPtr title;
  FontPtr small;
};

struct GalleryState {
  int page = 0;
  bool dark = true;
  bool notifications = true;
  bool beta = false;
  bool detail = true;
  bool busy = true;
  int plan = 1;
  int density = 1;
  int clicks = 0;
  int selectedTask = 0;
  double volume = 62.0;
  double progress = 0.35;
  std::string name = "Coral";
  std::vector<std::string> tasks{ "Write the docs", "Ship the gallery", "Take a break" };
  std::string lastEvent = "ready";

  bool operator==(const GalleryState&) const = default;
};

struct PageInfo {
  const char* title;
  const char* subtitle;
};

constexpr PageInfo kPages[] = {
  { "Overview", "What the declarative slice is, and what it costs" },
  { "Layout", "Stack, Grid, Dock, Scroll and Props" },
  { "Inputs", "Buttons, text, check boxes, toggles, radios, sliders, segments" },
  { "Display", "Progress, spinners, colour, text styles, tables" },
  { "Lists", "ListView, RecyclingList and keyed data updates" },
  { "State", "When, ForEach, Component, ElementRegistry and ElementSpec" },
  { "Escape hatch", "Widgets that do not have a first-class element yet" },
};
constexpr int kPageCount = static_cast<int>(std::size(kPages));

std::string fontPath() {
  return std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
}

bool writePng(const graphics::Bitmap& bitmap, const std::string& path) {
  if (bitmap.isEmpty()) return false;
  std::string error;
  const graphics::Data encoded = graphics::Image::fromBitmap(bitmap).encode(
    graphics::ImageFormat::PNG, 100, &error);
  if (encoded.size() == 0) {
    std::fprintf(stderr, "could not encode the UI frame: %s\n", error.c_str());
    return false;
  }
  std::ofstream stream(path, std::ios::binary);
  if (!stream.is_open()) return false;
  stream.write(reinterpret_cast<const char*>(encoded.bytes()),
    static_cast<std::streamsize>(encoded.size()));
  return stream.good();
}

std::shared_ptr<const graphics::Image> makeStarIcon() {
  graphics::Bitmap bitmap = graphics::Bitmap::allocate(20, 20);
  graphics::Pixmap pixels = bitmap.lockPixels();
  for (int y = 0; y < 20; ++y) {
    for (int x = 0; x < 20; ++x) {
      const float dx = static_cast<float>(x) - 9.5f;
      const float dy = static_cast<float>(y) - 9.5f;
      const float distance = dx * dx + dy * dy;
      if (distance <= 36.0f && distance >= 9.0f) {
        pixels.setPixelARGB32(x, y, 0xFFFFFFFFu);
      } else if (distance < 9.0f) {
        pixels.setPixelARGB32(x, y, 0xFFD0D8E4u);
      }
    }
  }
  return std::make_shared<const graphics::Image>(graphics::Image::fromBitmap(bitmap));
}

/**
 * The gallery application.
 *
 * State lives in one `State<GalleryState>`; the host observes it, so a control callback only writes a
 * new snapshot and the posted rebuild reconciles once. `build()` is a pure function of that snapshot
 * and the current window theme.
 */
class GalleryApp {
 public:
  GalleryApp(Window& window, const Fonts& fonts, std::shared_ptr<const graphics::Image> icon)
      : mWindow(window), mFonts(fonts), mIcon(std::move(icon)) {
    mDark = Theme::dark();
    mLight = Theme::light();
    for (Theme* theme: { &mDark, &mLight }) {
      theme->body = fonts.body;
      theme->title = fonts.title;
    }
    mCardWidget = std::make_shared<Card>(fonts.section, "Card widget");
    mCardWidget->setContent(std::make_shared<TextLayer>("Hosted through components::Layer",
      fonts.small, Color::MakeRGBA8(148, 163, 184, 255)));
    mChipWidget = std::make_shared<Chip>("Chip", fonts.small);
    mChipWidget->setRemovable(false);
    mBadgeWidget = std::make_shared<Badge>("Badge", fonts.small);
    mOverlayHost = std::make_unique<OverlayHost>(window);
    registerElements();
    applyTheme();
  }

  /** Applies the page/theme request from the command line before the first build. */
  void configure(const int page, const bool dark) {
    GalleryState next = mModel.get();
    next.page = std::clamp(page, 0, kPageCount - 1);
    next.dark = dark;
    mModel.set(std::move(next));
    applyTheme();
  }

  void mount() {
    mHost = std::make_unique<ElementHost>(mWindow, [this] { return build(); });
    mBinding = mHost->bind(mModel.property());
    mHost->setOnUpdated([this] { ++mRebuilds; });
    mHost->mount();
    mWindow.setRoot(mHost->root());
  }

  /** Prints the mounted layer tree with bounds and text, so a declarative build can be compared
   *  against the layers it produced. */
  void dumpTree() const {
    if (mHost == nullptr) return;
    int count = 0;
    dumpLayer(*mHost->root(), 0, count);
  }


  void showToast() {
    auto toast = std::make_shared<Toast>("Saved to the library", mFonts.body);
    toast->showFor("Saved to the library", std::chrono::milliseconds(1800));
    OverlayOptions options;
    options.modal = false;
    options.dismissOnOutsideClick = false;
    options.passThroughPointer = true;
    const Rect area = mWindow.bounds();
    options.anchor = Rect::MakeXYWH(area.getLeft() + 24.0f, area.getBottom() - 96.0f,
      std::max(0.0f, area.getWidth() - 48.0f), 32.0f);
    options.preferred = Placement::Center;
    const uint64_t id = mOverlayHost->show(components::Layer(toast, "gallery.toast"), options);
    toast->setOnDismissed([this, id] {
      if (mOverlayHost != nullptr) mOverlayHost->dismiss(id);
    });
  }
  [[nodiscard]] bool selfCheck() {
    if (mHost == nullptr || !mHost->mounted()) return fail("the host is not mounted");
    const std::shared_ptr<Layer> footerLayer = mFooterRef->layer();
    if (footerLayer == nullptr) return fail("the footer LayerRef was not filled in");
    const auto footer = std::static_pointer_cast<TextLayer>(footerLayer);

    // Every page is a different element subtree; the footer keeps its layer identity because its key
    // and kind do not change. The state writes are coalesced by the host, so a page switch is one
    // reconcile. Drawing itself is covered by the per-section CTest checks (`--section N`).
    for (int page = 0; page < kPageCount; ++page) {
      setPage(page);
      mWindow.pump();
      if (mFooterRef->layer() != footerLayer) {
        return fail("the footer layer identity changed across a page switch");
      }
      if (footer->text().find(kPages[page].title) == std::string::npos) {
        return fail("the footer text did not follow the selected page");
      }
    }

    scrollContent(240.0f);
    setPage(0);
    mWindow.pump();
    if (const auto layer = mContentScrollRef->layer()) {
      if (const auto scroll = std::dynamic_pointer_cast<ScrollView>(layer); scroll->scrollY() != 0.0f) {
        return fail("switching pages did not reset the content scroll offset");
      }
    }

    const uint32_t before = mRebuilds;
    change([](GalleryState& state) {
      state.clicks += 2;
      state.notifications = false;
      state.beta = true;
      state.volume = 80.0;
      state.progress = 0.9;
      state.selectedTask = 1;
      state.tasks.push_back("Self-check task");
      state.lastEvent = "self-check";
    });
    mWindow.pump();
    if (mRebuilds <= before) return fail("a State write did not schedule a rebuild");
    if (footer->text().find("clicks: 2") == std::string::npos) {
      return fail("the footer did not render the new model snapshot");
    }

    // The Lists page: adding rows and scrolling reuses container rows. This is the interactive path
    // that used to leave text at a previous slot's position and re-break row labels.
    setPage(4);
    mWindow.pump();
    change([](GalleryState& state) {
      while (state.tasks.size() < 30) {
        state.tasks.push_back("Task " + std::to_string(state.tasks.size() + 1));
      }
      state.lastEvent = "list stress";
    });
    mWindow.pump();
    // The model is updated; one frame lays the new page out so the list viewport has real bounds.
    mWindow.requestFrame();
    if (!mWindow.renderFrame()) return fail("the list frame was not rendered");
    std::vector<TextLayer*> summaryLabels;
    const auto collectSummary = [&](const auto& self, Layer* layer) -> void {
      if (layer == nullptr) return;
      if (auto* text = dynamic_cast<TextLayer*>(layer);
          text != nullptr && text->text().find(".  ") != std::string::npos) {
        summaryLabels.push_back(text);
      }
      for (const auto& child: layer->children()) self(self, child.get());
    };
    collectSummary(collectSummary, mHost->root().get());
    TextLayer* firstSummary = nullptr;
    TextLayer* secondSummary = nullptr;
    for (TextLayer* text: summaryLabels) {
      if (text->text().rfind("1.", 0) == 0) firstSummary = text;
      if (text->text().rfind("2.", 0) == 0) secondSummary = text;
    }
    if (firstSummary != nullptr && secondSummary != nullptr &&
        secondSummary->bounds().getTop() - firstSummary->bounds().getTop() < 12.0f) {
      return fail("the Plain ForEach rows overlapped after Add task");
    }
    const auto recycleList = std::dynamic_pointer_cast<RecyclingListView>(
      mRecyclingListRef->layer());
    if (recycleList == nullptr) return fail("the recycling list LayerRef was not filled in");
    recycleList->setScrollOffset(0.0f, 84.0f);
    recycleList->layout();
    const GalleryState listed = mModel.get();
    int visibleRows = 0;
    for (const std::string& key: listed.tasks) {
      const std::shared_ptr<Layer> row = recycleList->rowLayer(key);
      if (row == nullptr) continue;
      ++visibleRows;
      for (const std::shared_ptr<Layer>& child: row->children()) {
        if (child->bounds().getTop() < row->bounds().getTop() - 0.5f ||
            child->bounds().getBottom() > row->bounds().getBottom() + 0.5f) {
          return fail("a recycled row kept a child at the previous slot's position");
        }
      }
      const auto label = row->children().empty()
        ? std::shared_ptr<TextLayer>{}
        : std::dynamic_pointer_cast<TextLayer>(row->children().front());
      if (label != nullptr && label->layout().lines.size() != 1u) {
        return fail("a recycled row label re-broke at its measured width");
      }
    }
    if (visibleRows < 2) return fail("the list scroll did not leave rows mounted");

    toggleTheme();
    mWindow.pump();
    if (footer->text().find("theme -> light") == std::string::npos) {
      return fail("the light theme write was not reconciled");
    }
    toggleTheme();
    mWindow.pump();
    if (footer->text().find("theme -> dark") == std::string::npos) {
      return fail("the dark theme write was not reconciled");
    }
    if (mFooterRef->layer() != footerLayer) return fail("the footer did not survive the theme switch");

    std::printf("declarative gallery self-check: %d pages, %u rebuilds, footer identity stable\n",
      kPageCount, static_cast<unsigned>(mRebuilds));
    return true;
  }

   void scrollContent(const float y) {
    if (const auto layer = mContentScrollRef->layer()) {
      if (const auto scroll = std::dynamic_pointer_cast<ScrollView>(layer)) {
        scroll->setScrollOffset(0.0f, y);
        mWindow.pump();
      }
    }
  }
private:
  static bool fail(const char* message) {
    std::fprintf(stderr, "declarative gallery self-check failed: %s\n", message);
    return false;
  }

  static void dumpLayer(Layer& layer, const int depth, int& count) {
    if (count++ > 400) return;
    std::string extra;
    if (const auto text = dynamic_cast<TextLayer*>(&layer)) {
      std::string value = text->text();
      if (value.size() > 48) value = value.substr(0, 48) + "...";
      extra = " text=\"" + value + "\"";
      const TextLayout& layout = text->layout();
      extra += " layout=" + std::to_string(static_cast<int>(layout.size.width)) + "x" +
        std::to_string(static_cast<int>(layout.size.height)) + " lines=" +
        std::to_string(layout.lines.size());
    }
    if (layer.clipRadius() > 0.0f) {
      extra += " clipRadius=" + std::to_string(static_cast<int>(layer.clipRadius())) +
        (layer.clipsToBounds() ? " clip=1" : " clip=0");
    }
    const Rect bounds = layer.bounds();
    std::printf("%*s%-28s (%.0f,%.0f %.0fx%.0f) role=%d%s\n", depth * 2, "",
      typeid(layer).name(), static_cast<double>(bounds.getLeft()),
      static_cast<double>(bounds.getTop()), static_cast<double>(bounds.getWidth()),
      static_cast<double>(bounds.getHeight()), static_cast<int>(layer.backgroundColorRole()),
      extra.c_str());
    for (const auto& child: layer.children()) dumpLayer(*child, depth + 1, count);
  }

  void change(const std::function<void(GalleryState&)>& mutate) {
    GalleryState next = mModel.get();
    mutate(next);
    mModel.set(std::move(next));
  }


  void setPage(const int page) {
    const int clamped = std::clamp(page, 0, kPageCount - 1);
    if (const auto layer = mContentScrollRef->layer()) {
      if (const auto scroll = std::dynamic_pointer_cast<ScrollView>(layer)) {
        scroll->setScrollOffset(0.0f, 0.0f);
      }
    }
    change([clamped](GalleryState& state) {
      state.page = clamped;
      state.lastEvent = std::string("page ->") + kPages[clamped].title;
    });
  }

  void toggleTheme() {
    GalleryState next = mModel.get();
    next.dark = !next.dark;
    next.lastEvent = next.dark ? "theme -> dark" : "theme -> light";
    mModel.set(std::move(next));
    applyTheme();
  }

  void applyTheme() {
    mWindow.setTheme(mModel.get().dark ? mDark : mLight);
  }

  void registerElements() {
    // A registry type is the declarative extension point for widgets that are not built in: the
    // builder returns ordinary elements, so the reconciler keeps its closed patch switch.
    mRegistry.add("Pill", [this](const ElementProps&, std::vector<ElementPtr> children) {
      ElementProps props;
      props.backgroundRole = ThemeColorRole::SurfaceRaised;
      props.clip = false;
      auto stack = std::make_shared<StackElement>(Axis::Row, std::move(children), "pill.stack", 6.0f,
        Insets{}.all(8.0f), Align::Center);
      return components::WithProps(std::move(stack), props, "pill.body");
    });
  }

  // ---------------------------------------------------------------------------------------------
  // Small element helpers. Every helper returns a fresh immutable element.
  // ---------------------------------------------------------------------------------------------
  [[nodiscard]] ElementPtr label(std::string value, const FontPtr& font, const Color color,
      std::string key) const {
    return std::make_shared<TextElement>(std::move(value), font, color, std::move(key));
  }

  [[nodiscard]] ElementPtr row(std::string key, std::vector<ElementPtr> children,
      const float gap = 8.0f, const Align align = Align::Center) const {
    return std::make_shared<StackElement>(Axis::Row, std::move(children), std::move(key), gap,
      Insets{}, align);
  }

  /** A filled rectangle with a fixed measured size. */
  [[nodiscard]] ElementPtr swatch(const Color color, const float width, const float height,
      const float radius, std::string key) const {
    ElementProps props;
    props.bounds = Rect::MakeWH(width, height);
    props.background = color;
    props.cornerRadius = radius;
    props.clip = true;
    props.layout.minWidth = width;
    props.layout.minHeight = height;
    return components::WithProps(
      std::make_shared<ColorElement>(color, key + ".color"), props, std::move(key));
  }

  [[nodiscard]] ElementPtr spacer(const float grow, std::string key) const {
    ElementProps props;
    props.layout.grow = grow;
    props.layout.minHeight = 0.0f;
    return components::WithProps(
      std::make_shared<ColorElement>(Color::MakeRGBA8(0, 0, 0, 0), key + ".color"), props,
      std::move(key));
  }

  /** One surface card: title, optional subtitle and a stretched column body. */
  [[nodiscard]] ElementPtr card(std::string key, std::string title, std::string subtitle,
      std::vector<ElementPtr> body) const {
    const Theme& theme = mWindow.theme();
    std::vector<ElementPtr> children;
    children.push_back(label(std::move(title), mFonts.section, theme.text, key + ".title"));
    if (!subtitle.empty()) {
      children.push_back(label(std::move(subtitle), mFonts.small, theme.textMuted, key + ".subtitle"));
    }
    for (ElementPtr& child: body) children.push_back(std::move(child));
    auto stack = std::make_shared<StackElement>(Axis::Column, std::move(children), key + ".stack",
      8.0f, Insets{}.all(16.0f), Align::Stretch);
    ElementProps props;
    props.backgroundRole = ThemeColorRole::Surface;
    // A card is a masked container; keep it rectangular until composed container masks composite.
    props.clip = false;
    return components::WithProps(std::move(stack), props, std::move(key));
  }

  [[nodiscard]] ElementPtr statCard(std::string key, std::string value, std::string caption) const {
    const Theme& theme = mWindow.theme();
    std::vector<ElementPtr> body;
    body.push_back(label(std::move(value), mFonts.title, theme.accent, key + ".value"));
    body.push_back(label(std::move(caption), mFonts.small, theme.textMuted, key + ".caption"));
    auto stack = std::make_shared<StackElement>(Axis::Column, std::move(body), key + ".stack", 2.0f,
      Insets{}.all(14.0f), Align::Stretch);
    ElementProps props;
    props.backgroundRole = ThemeColorRole::Surface;
    // A card is a masked container; keep it rectangular until composed container masks composite.
    props.clip = false;
    return components::WithProps(std::move(stack), props, std::move(key));
  }

  // ---------------------------------------------------------------------------------------------
  // The tree. `build` is the only thing the host calls; page functions below only return elements.
  // ---------------------------------------------------------------------------------------------
  [[nodiscard]] ElementPtr build() {
    const GalleryState state = mModel.get();
    const Theme& theme = mWindow.theme();

    auto sidebar = buildSidebar(state, theme);
    auto content = buildContent(state, theme);
    std::vector<ElementPtr> rootChildren;
    rootChildren.push_back(std::move(sidebar));
    rootChildren.push_back(std::move(content));
    auto shell = std::make_shared<StackElement>(Axis::Row, std::move(rootChildren), "shell", 0.0f,
      Insets{}, Align::Stretch);
    ElementProps props;
    props.backgroundRole = ThemeColorRole::Background;
    props.clip = true;
    return components::WithProps(std::move(shell), props, "shell-wrap");
  }

  [[nodiscard]] ElementPtr buildSidebar(const GalleryState& state, const Theme& theme) {
    std::vector<ElementPtr> children;
    children.push_back(label("CoralUI", mFonts.title, theme.text, "side.brand"));
    children.push_back(label("declarative gallery", mFonts.small, theme.textMuted, "side.tagline"));
    children.push_back(swatch(theme.accent, 40.0f, 3.0f, 2.0f, "side.rule"));

    for (int index = 0; index < kPageCount; ++index) {
      const bool active = state.page == index;
      const std::string text = std::string(active ? "> " : "   ") + kPages[index].title;
      ElementProps props;
      props.layout.basis = 34.0f;
      props.layout.shrink = 0.0f;
      children.push_back(components::WithProps(
        components::Button(text, mFonts.body,
          [this, index] { setPage(index); }, "side.nav." + std::to_string(index)),
        props, "side.nav." + std::to_string(index) + ".wrap"));
    }

    children.push_back(spacer(1.0f, "side.spacer"));
    children.push_back(label("80 first-class element kinds", mFonts.small, theme.textMuted,
      "side.coverage"));
    children.push_back(label("9 Layer types use the escape hatch", mFonts.small, theme.textMuted,
      "side.coverage.more"));

    auto column = std::make_shared<StackElement>(Axis::Column, std::move(children), "side.column",
      6.0f, Insets{}.all(16.0f), Align::Stretch);
    ElementProps props;
    props.backgroundRole = ThemeColorRole::Surface;
    props.layout.basis = 238.0f;
    props.layout.shrink = 0.0f;
    props.clip = true;
    return components::WithProps(std::move(column), props, "side.shell");
  }

  [[nodiscard]] ElementPtr buildContent(const GalleryState& state, const Theme& theme) {
    std::vector<ElementPtr> headerChildren;
    ElementProps titleProps;
    titleProps.layout.grow = 1.0f;
    titleProps.layout.shrink = 1.0f;
    headerChildren.push_back(components::WithProps(
      label(kPages[state.page].title, mFonts.title, theme.text, "content.page-title"),
      titleProps, "content.page-title-wrap"));
    headerChildren.push_back(components::Button(state.dark ? "Light mode" : "Dark mode",
      mFonts.body, [this] { toggleTheme(); }, "content.theme"));
    auto header = row("content.header", std::move(headerChildren), 12.0f, Align::Center);

    auto body = buildPage(state, theme);
    auto scroll = components::Scroll(std::move(body), 0.0f, "content.scroll");
    ElementProps scrollProps;
    scrollProps.layout.grow = 1.0f;
    scrollProps.layout.shrink = 1.0f;
    scrollProps.layout.minHeight = 0.0f;
    scrollProps.ref = mContentScrollRef;

    std::vector<ElementPtr> columnChildren;
    columnChildren.push_back(std::move(header));
    columnChildren.push_back(components::WithProps(std::move(scroll), scrollProps,
      "content.scroll-wrap"));
    columnChildren.push_back(footerWithRef("page: " + std::string(kPages[state.page].title) +
      "  -  clicks: " + std::to_string(state.clicks) + "  -  last event: " + state.lastEvent, theme));

    auto column = std::make_shared<StackElement>(Axis::Column, std::move(columnChildren),
      "content.column", 14.0f, Insets{}.all(24.0f), Align::Stretch);
    ElementProps props;
    props.layout.grow = 1.0f;
    props.layout.shrink = 1.0f;
    props.layout.minWidth = 0.0f;
    props.backgroundRole = ThemeColorRole::Background;
    return components::WithProps(std::move(column), props, "content.shell");
  }

  /** The footer is the one element with a `LayerRef`, so the self-check can prove layer identity. */
  [[nodiscard]] ElementPtr footerWithRef(const std::string& text, const Theme& theme) {
    ElementProps props;
    props.ref = mFooterRef;
    return components::WithProps(
      label(text, mFonts.small, theme.textMuted, "content.footer"), props, "content.footer-wrap");
  }

  [[nodiscard]] ElementPtr buildPage(const GalleryState& state, const Theme& theme) {
    std::vector<ElementPtr> cards;
    switch (state.page) {
      case 0: cards = overviewPage(state, theme); break;
      case 1: cards = layoutPage(state, theme); break;
      case 2: cards = inputsPage(state, theme); break;
      case 3: cards = displayPage(state, theme); break;
      case 4: cards = listsPage(state, theme); break;
      case 5: cards = statePage(state, theme); break;
      default: cards = escapeHatchPage(state, theme); break;
    }
    return std::make_shared<StackElement>(Axis::Column, std::move(cards),
      "page." + std::to_string(state.page), 14.0f, Insets{}, Align::Stretch);
  }

  [[nodiscard]] std::vector<ElementPtr> overviewPage(const GalleryState& state,
      const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> heroBody;
    heroBody.push_back(label(
      "The window is one immutable element tree. A state write rebuilds the tree, and the "
      "reconciler matches nodes by (kind, key), so every layer below keeps its identity: focus, "
      "scroll position and animations survive.",
      mFonts.body, theme.textMuted, "ov.hero.body"));

    std::vector<ElementPtr> actions;
    actions.push_back(components::Button("Open Inputs", mFonts.body, [this] { setPage(2); },
      "ov.hero.inputs"));
    actions.push_back(components::Button("Toggle theme", mFonts.body, [this] { toggleTheme(); },
      "ov.hero.theme"));
    actions.push_back(components::Button("Show toast", mFonts.body, [this] {
      showToast();
      change([](GalleryState& next) { next.lastEvent = "Toast shown"; });
    }, "ov.hero.toast"));
    actions.push_back(components::CheckBox("Show detail", mFonts.body, state.detail,
      [this](const bool value) {
        change([value](GalleryState& next) {
          next.detail = value;
          next.lastEvent = "detail toggled";
        });
      }, "ov.hero.detail"));
    heroBody.push_back(row("ov.hero.actions", std::move(actions), 10.0f));
    heroBody.push_back(components::When(state.detail,
      label("This branch is a WhenElement. Swap it and the branch layer is replaced while the "
        "surrounding card is patched in place.",
        mFonts.small, theme.accent, "ov.hero.when.then"),
      label("Detail hidden: the When occupies no slot when a branch is null.",
        mFonts.small, theme.textMuted, "ov.hero.when.else"),
      "ov.hero.when"));
    page.push_back(card("ov.hero", "Declarative CoralUI", "Element ->Reconciler ->Layer",
      std::move(heroBody)));

    std::vector<ElementPtr> stats;
    stats.push_back(statCard("ov.stat.kinds", "80", "first-class element kinds"));
    stats.push_back(statCard("ov.stat.pages", std::to_string(kPageCount), "gallery pages, one tree"));
    stats.push_back(statCard("ov.stat.clicks", std::to_string(state.clicks), "state writes observed"));
    page.push_back(std::make_shared<GridElement>(3, std::move(stats), "ov.stats", 12.0f, Insets{}));

    std::vector<std::string> steps{
      "Element tree: rebuilt by the app from the current state snapshot",
      "Reconciler: matches (kind, key) and patches the smallest set of layers",
      "Layer tree: measured, painted and hit-tested exactly as before",
    };
    std::vector<ElementPtr> stepRows;
    for (size_t index = 0; index < steps.size(); ++index) {
      std::vector<ElementPtr> rowChildren;
      rowChildren.push_back(swatch(index == 0 ? theme.accent : theme.controls.border, 10.0f, 10.0f,
        5.0f, "ov.step." + std::to_string(index) + ".dot"));
      rowChildren.push_back(label(steps[index], mFonts.body, theme.text,
        "ov.step." + std::to_string(index) + ".text"));
      stepRows.push_back(row("ov.step." + std::to_string(index), std::move(rowChildren), 8.0f));
    }
    page.push_back(card("ov.map", "The pipeline", "One direction of data flow", std::move(stepRows)));

    std::vector<ElementPtr> coverageBody;
    coverageBody.push_back(label(
      "Only the element kinds in `ui/Element.h` are first-class; the remaining layer types are "
      "reachable through components::Layer or an ElementRegistry type. The last page shows the "
      "boundary on purpose.",
      mFonts.body, theme.textMuted, "ov.coverage.text"));
    coverageBody.push_back(row("ov.coverage.actions", {
      components::Button("See the escape hatch", mFonts.body, [this] { setPage(6); },
        "ov.coverage.open"),
    }, 8.0f));
    page.push_back(card("ov.coverage", "Coverage", "80 of 89 public Layer types", std::move(coverageBody)));
    return page;
  }

  [[nodiscard]] std::vector<ElementPtr> layoutPage(const GalleryState&, const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> cards;

    std::vector<ElementPtr> rowBody;
    rowBody.push_back(label("Axis::Row - gap 8 - fixed-size children", mFonts.small,
      theme.textMuted, "lay.row.caption"));
    std::vector<ElementPtr> blocks;
    blocks.push_back(swatch(theme.accent, 52.0f, 34.0f, 6.0f, "lay.row.a"));
    blocks.push_back(swatch(theme.controls.backgroundHover, 52.0f, 34.0f, 6.0f, "lay.row.b"));
    blocks.push_back(swatch(theme.controls.border, 52.0f, 34.0f, 6.0f, "lay.row.c"));
    rowBody.push_back(row("lay.row.blocks", std::move(blocks), 8.0f));
    cards.push_back(card("lay.row", "Stack - Row", "A one-dimensional flex", std::move(rowBody)));

    std::vector<ElementPtr> columnBody;
    columnBody.push_back(label("Axis::Column - padding 12 - gap 6", mFonts.small, theme.textMuted,
      "lay.column.caption"));
    for (int index = 0; index < 3; ++index) {
      columnBody.push_back(swatch(index == 1 ? theme.accent : theme.controls.track, 120.0f, 8.0f,
        4.0f, "lay.column." + std::to_string(index)));
    }
    cards.push_back(card("lay.column", "Stack - Column", "Content-sized rows", std::move(columnBody)));

    std::vector<ElementPtr> tiles;
    for (int index = 0; index < 6; ++index) {
      tiles.push_back(swatch(index % 2 == 0 ? theme.controls.track : theme.controls.backgroundHover,
        44.0f, 34.0f, 6.0f, "lay.grid." + std::to_string(index)));
    }
    std::vector<ElementPtr> gridBody;
    gridBody.push_back(label("GridElement(3) - six equal cells", mFonts.small, theme.textMuted,
      "lay.grid.caption"));
    gridBody.push_back(std::make_shared<GridElement>(3, std::move(tiles), "lay.grid.grid", 8.0f,
      Insets{}));
    cards.push_back(card("lay.grid", "Grid - 3 columns", "Equal-column layout", std::move(gridBody)));

    auto dockPanel = [this, &theme](std::string key, std::string name, const Color background) {
      auto stack = std::make_shared<StackElement>(Axis::Column,
        std::vector<ElementPtr>{ label(std::move(name), mFonts.small, theme.text, key + ".label") },
        key + ".stack", 0.0f, Insets{}.all(10.0f), Align::Stretch);
      ElementProps props;
      props.background = background;
      props.clip = false;
      return components::WithProps(std::move(stack), props, std::move(key));
    };
    auto dock = std::make_shared<DockElement>(
      dockPanel("lay.dock.center", "center", theme.controls.track),
      dockPanel("lay.dock.left", "left", theme.controls.background),
      dockPanel("lay.dock.right", "right", theme.controls.background),
      ElementPtr{}, 72.0f, 72.0f, 0.0f, "lay.dock.dock");
    ElementProps dockProps;
    dockProps.layout.basis = 170.0f;
    dockProps.layout.shrink = 0.0f;
    dockProps.layout.widthPercent = 1.0f;
    std::vector<ElementPtr> dockBody;
    dockBody.push_back(label("DockElement: left / centre / right regions", mFonts.small,
      theme.textMuted, "lay.dock.caption"));
    dockBody.push_back(components::WithProps(std::move(dock), dockProps, "lay.dock.wrap"));
    cards.push_back(card("lay.dock", "Dock", "A four-region layout", std::move(dockBody)));
    page.push_back(std::make_shared<GridElement>(2, std::move(cards), "lay.cards", 12.0f, Insets{}));

    std::vector<ElementPtr> propsBody;
    propsBody.push_back(label("Props are layout/compositing values applied to the child's layer: "
      "the wrapper itself adds no node.", mFonts.small, theme.textMuted, "lay.props.caption"));
    std::vector<ElementPtr> styled;
    const struct {
      float radius;
      float opacity;
      bool shadow;
    } looks[] = { { 0.0f, 1.0f, false }, { 10.0f, 0.75f, false }, { 999.0f, 1.0f, true } };
    for (int index = 0; index < 3; ++index) {
      ElementProps props;
      props.bounds = Rect::MakeWH(72.0f, 44.0f);
      Color fill = index == 0 ? theme.controls.track : theme.accent;
      props.background = fill;
      props.cornerRadius = looks[index].radius;
      props.opacity = looks[index].opacity;
      props.clip = true;
      if (looks[index].shadow) props.shadow = theme.shadow.sm;
      styled.push_back(components::WithProps(
        std::make_shared<ColorElement>(fill, "lay.props." + std::to_string(index) + ".color"), props,
        "lay.props." + std::to_string(index)));
    }
    propsBody.push_back(row("lay.props.row", std::move(styled), 14.0f));
    page.push_back(card("lay.props", "Props", "opacity - shadow - layout", std::move(propsBody)));

    std::vector<ElementPtr> scrollRows;
    for (int index = 0; index < 20; ++index) {
      scrollRows.push_back(label("Row " + std::to_string(index + 1) + " ->scrolls inside its card",
        mFonts.small, index % 2 == 0 ? theme.text : theme.textMuted,
        "lay.scroll." + std::to_string(index)));
    }
    auto scrollContent = std::make_shared<StackElement>(Axis::Column, std::move(scrollRows),
      "lay.scroll.content", 5.0f, Insets{}.all(2.0f), Align::Stretch);
    std::vector<ElementPtr> scrollBody;
    scrollBody.push_back(label("ScrollElement(20 rows, preferred height 150)", mFonts.small,
      theme.textMuted, "lay.scroll.caption"));
    scrollBody.push_back(components::Scroll(std::move(scrollContent), 150.0f, "lay.scroll.view"));
    std::vector<ElementPtr> containerBody;
    containerBody.push_back(components::Card("CardElement", mFonts.section,
      label("A title, a content slot and a header action.", mFonts.small, theme.textMuted,
        "lay.cont.card.body"),
      components::Link("Edit", mFonts.small, [this] {
        change([](GalleryState& next) { next.lastEvent = "Card action"; });
      }, "lay.cont.card.action"),
      "lay.cont.card"));
    containerBody.push_back(components::SectionHeader("SectionHeader", mFonts.body, "subtitle",
      components::Link("More", mFonts.small, {}, "lay.cont.section.action"), "lay.cont.section"));
    containerBody.push_back(components::FormField("Display name", mFonts.small,
      components::TextInput("Coral", mFonts.body, {}, "lay.cont.field.input"),
      "Shown on your profile", {}, "lay.cont.field"));
    containerBody.push_back(components::Expander("Expander", mFonts.body,
      label("The content slot is mounted only while expanded.", mFonts.small, theme.textMuted,
        "lay.cont.expander.body"), true, [this](const bool expanded) {
        change([expanded](GalleryState& next) {
          next.lastEvent = expanded ? "Expander opened" : "Expander closed";
        });
      }, "lay.cont.expander"));
    containerBody.push_back(components::EmptyState("EmptyState", mFonts.section, "No builds yet",
      {}, components::Button("Create build", mFonts.small, [this] {
        change([](GalleryState& next) { next.lastEvent = "Empty state action"; });
      }, "lay.cont.empty.action"), {}, "lay.cont.empty"));
    page.insert(page.begin(), card("lay.containers", "Content containers",
      "Card - SectionHeader - FormField - Expander - EmptyState", std::move(containerBody)));
    std::vector<ElementPtr> structuredBody;
    structuredBody.push_back(components::TabBar(mFonts.body, {
      TabItem{ "Tab one", label("The TabBar owns one content slot per tab.", mFonts.small,
        theme.textMuted, "lay.struct.tab1"), "one" },
      TabItem{ "Tab two", label("Switching tabs reuses the content layers.", mFonts.small,
        theme.textMuted, "lay.struct.tab2"), "two" },
    }, 0, [this](const int index) {
      change([index](GalleryState& next) {
        next.lastEvent = "Tab " + std::to_string(index);
      });
    }, "lay.struct.tabs"));
    ElementProps splitProps;
    splitProps.layout.minHeight = 120.0f;
    const auto splitPane = [this, &theme](std::string key, std::string title, const Color background) {
      std::vector<ElementPtr> children;
      children.push_back(label(std::move(title), mFonts.small, theme.text, key + ".label"));
      auto pane = std::make_shared<StackElement>(Axis::Column, std::move(children), key + ".stack",
        0.0f, Insets{}.all(12.0f), Align::Stretch);
      ElementProps props;
      props.background = background;
      props.layout.widthPercent = 1.0f;
      props.layout.heightPercent = 1.0f;
      return components::WithProps(std::move(pane), props, key);
    };
    structuredBody.push_back(components::WithProps(components::SplitView(Axis::Row, 0.4f,
      splitPane("lay.struct.left", "left pane", theme.accent),
      splitPane("lay.struct.right", "right pane", theme.surfaceRaised),
      {}, "lay.struct.split"), splitProps, "lay.struct.split.wrap"));
    structuredBody.push_back(components::Toolbar(mFonts.body, {
      ToolbarItem{ components::Link("Open", mFonts.small, {}, "lay.struct.open"), false, false, {},
        {}, "open" },
      ToolbarItem{ {}, false, true, {}, {}, "sep" },
      ToolbarItem{ components::Link("Share", mFonts.small, {}, "lay.struct.share"), false, false,
        {}, {}, "share" },
    }, 40.0f, "lay.struct.toolbar"));
    structuredBody.push_back(components::StatusBar(mFonts.body, "Ready", "UTF-8",
      { components::Link("main", mFonts.small, {}, "lay.struct.branch") },
      { components::Link("Ln 1", mFonts.small, {}, "lay.struct.line") }, "lay.struct.status"));
    page.insert(page.begin() + 1, card("lay.structured", "Structured containers",
      "TabBar - SplitView - Toolbar - StatusBar", std::move(structuredBody)));
    page.push_back(card("lay.scroll", "Scroll", "One clipped viewport", std::move(scrollBody)));
    return page;
  }

  [[nodiscard]] std::vector<ElementPtr> inputsPage(const GalleryState& state,
      const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> cards;

    std::vector<ElementPtr> buttons;
    buttons.push_back(components::Button("Primary", mFonts.body, [this] {
      change([](GalleryState& next) {
        ++next.clicks;
        next.lastEvent = "Button clicked";
      });
    }, "in.btn.primary"));
    buttons.push_back(components::Button("Disabled", mFonts.body, [] {}, "in.btn.disabled", false));
    buttons.push_back(components::IconButton(mIcon, "Star", [this] {
      change([](GalleryState& next) { next.lastEvent = "IconButton clicked"; });
    }, "in.btn.icon"));
    buttons.push_back(components::ColorButton(theme.accent, "Accent", [this] {
      change([](GalleryState& next) { next.lastEvent = "ColorButton picked"; });
    }, "in.btn.color"));
    std::vector<ElementPtr> buttonBody;
    buttonBody.push_back(row("in.btn.row", std::move(buttons), 10.0f));
    buttonBody.push_back(label("ButtonElement - IconButtonElement - ColorButtonElement",
      mFonts.small, theme.textMuted, "in.btn.caption"));
    cards.push_back(card("in.btn", "Buttons", "Click handlers are plain lambdas", std::move(buttonBody)));

    std::vector<ElementPtr> interactive;
    std::vector<ElementPtr> interactiveRow;
    interactiveRow.push_back(components::Link("Documentation", mFonts.body, [this] {
      change([](GalleryState& next) { next.lastEvent = "Link activated"; });
    }, "in.interactive.link"));
    interactiveRow.push_back(components::Chip("alpha", mFonts.small, ChipVariant::Accent, {}, false,
      true, {}, "in.interactive.chip"));
    interactiveRow.push_back(components::ToggleButton("Bold", mFonts.small, true, {}, "in.interactive.toggle"));
    interactiveRow.push_back(components::Rating(5, 3.0, false, false, [this](const double value) {
      change([value](GalleryState& next) {
        next.lastEvent = "Rating " + std::to_string(value);
      });
    }, "in.interactive.rating"));
    interactive.push_back(row("in.interactive.row1", std::move(interactiveRow), 12.0f));
    std::vector<ElementPtr> interactiveRow2;
    interactiveRow2.push_back(components::Stepper(0.0, 10.0, 4.0, 1.0, {}, "in.interactive.stepper"));
    interactiveRow2.push_back(components::Pagination(mFonts.body, 5, 1, {}, "in.interactive.pagination"));
    interactive.push_back(row("in.interactive.row2", std::move(interactiveRow2), 16.0f));
    interactive.push_back(components::RangeSlider(0.0, 100.0, 20.0, 60.0, 1.0, {},
      "in.interactive.range"));
    interactive.push_back(components::Breadcrumb({ "Library", "Widgets", "Inputs" }, mFonts.small,
      "/", 1, {}, "in.interactive.crumbs"));
    interactive.push_back(components::Banner("Draft saved automatically.", mFonts.small,
      BannerSeverity::Success, "Undo", [this] {
        change([](GalleryState& next) { next.lastEvent = "Banner action"; });
      }, true, {}, {}, "in.interactive.banner"));
    interactive.push_back(components::SplitButton("Run", mFonts.small, { "Run all", "Debug" }, {},
      {}, "in.interactive.split"));
    cards.push_back(card("in.interactive", "New interactive leaves", "Self-contained widgets",
      std::move(interactive)));
    std::vector<ElementPtr> groupsBody;
    groupsBody.push_back(components::ToggleGroup({ "Bold", "Italic", "Underline" }, mFonts.small,
      { 0 }, {}, "in.groups.toggle"));
    groupsBody.push_back(components::RadioGroup({ "Free", "Studio", "Team" }, mFonts.small, 1, {},
      "in.groups.radio"));
    groupsBody.push_back(components::CheckBoxGroup({ "Email", "Push", "SMS" }, mFonts.small,
      { 0, 2 }, {}, "in.groups.checks"));
    groupsBody.push_back(components::MultiSelect({ "Alpha", "Beta", "Gamma" }, mFonts.small,
      { 0, 1 }, "Pick channels", {}, "in.groups.multi"));
    cards.push_back(card("in.groups", "Selection groups", "Options + selected indices",
      std::move(groupsBody)));
    std::vector<ElementPtr> editorBody;
    editorBody.push_back(components::Select({ "Free", "Studio", "Team" }, mFonts.small, 1, {},
      [this](const int index) {
        change([index](GalleryState& next) {
          next.lastEvent = "Select " + std::to_string(index);
        });
      }, "in.editors.select"));
    editorBody.push_back(components::ComboBox({ "Coral", "Swift", "C++" }, mFonts.small, "Coral", 0,
      "Type or pick", {}, "in.editors.combo"));
    editorBody.push_back(components::SearchField("Search widgets", mFonts.small, {}, {},
      [this](const std::string& text) {
        change([text](GalleryState& next) { next.lastEvent = "Search " + text; });
      }, "in.editors.search"));
    editorBody.push_back(components::PasswordField("Password", mFonts.small, "secret", 32, {},
      "in.editors.password"));
    editorBody.push_back(components::NumberField(mFonts.small, 0.0, 100.0, 42.0, 1.0, 0, {},
      "in.editors.number"));
    editorBody.push_back(components::TagInput(mFonts.small, { "ui", "declarative" }, "Add tag", {},
      "in.editors.tags"));
    editorBody.push_back(components::ColorInput(mFonts.small, theme.accent, {}, "in.editors.color"));
    cards.push_back(card("in.editors", "Editor fields", "Select - ComboBox - Search - Password - Number - Tags - Color",
      std::move(editorBody)));



    std::vector<ElementPtr> inputBody;
    inputBody.push_back(components::TextInput(state.name, mFonts.body,
      [this](const std::string& value) {
        change([value](GalleryState& next) {
          next.name = value;
          next.lastEvent = "name ->" + value;
        });
      }, "in.text.input"));
    inputBody.push_back(label("Hello, " + state.name + ".", mFonts.small, theme.accent,
      "in.text.greeting"));
    cards.push_back(card("in.text", "TextInput", "onChanged writes the model snapshot",
      std::move(inputBody)));

    std::vector<ElementPtr> checkBody;
    checkBody.push_back(components::CheckBox("Desktop notifications", mFonts.body,
      state.notifications, [this](const bool value) {
        change([value](GalleryState& next) {
          next.notifications = value;
          next.lastEvent = value ? "notifications on" : "notifications off";
        });
      }, "in.check.box"));
    checkBody.push_back(components::Toggle("Beta channel", mFonts.body, state.beta,
      [this](const bool value) {
        change([value](GalleryState& next) {
          next.beta = value;
          next.lastEvent = value ? "beta on" : "beta off";
        });
      }, "in.check.toggle"));
    cards.push_back(card("in.check", "CheckBox & Toggle", "Boolean model fields",
      std::move(checkBody)));

    std::vector<ElementPtr> radioBody;
    const char* plans[] = { "Free", "Studio", "Team" };
    for (int index = 0; index < 3; ++index) {
      const std::string plan = plans[index];
      radioBody.push_back(components::RadioButton(plan, mFonts.body, state.plan == index,
        [this, index, plan] {
          change([index, plan](GalleryState& next) {
            next.plan = index;
            next.lastEvent = "plan -> " + plan;
          });
        }, "in.radio." + std::to_string(index)));
    }
    cards.push_back(card("in.radio", "RadioButton", "The owner keeps the exclusive selection",
      std::move(radioBody)));

    std::vector<ElementPtr> sliderBody;
    sliderBody.push_back(components::Slider("Volume", mFonts.body, 0.0, 100.0, state.volume,
      [this](const double value) {
        change([value](GalleryState& next) {
          next.volume = value;
          next.lastEvent = "volume ->" + std::to_string(static_cast<int>(std::lround(value)));
        });
      }, "in.slider.value",
      [this] { change([](GalleryState& next) { next.lastEvent = "volume edit began"; }); },
      [this] { change([](GalleryState& next) { next.lastEvent = "volume edit ended"; }); }));
    sliderBody.push_back(label("volume: " + std::to_string(static_cast<int>(std::lround(
      state.volume))) + "%", mFonts.small, theme.textMuted, "in.slider.readout"));
    cards.push_back(card("in.slider", "Slider", "Drag or use the arrow keys", std::move(sliderBody)));

    std::vector<ElementPtr> segmentBody;
    segmentBody.push_back(components::SegmentedControl({ "Compact", "Cosy", "Room" }, mFonts.body,
      state.density, [this](const int index) {
        change([index](GalleryState& next) {
          next.density = index;
          next.lastEvent = "density ->" + std::to_string(index);
        });
      }, "in.segment.control"));
    segmentBody.push_back(label(std::string("density: ") +
      (state.density == 0 ? "compact" : (state.density == 1 ? "cosy" : "room")), mFonts.small,
      theme.textMuted, "in.segment.readout"));
    cards.push_back(card("in.segment", "SegmentedControl", "A model index, not view state",
      std::move(segmentBody)));

    page.push_back(std::make_shared<GridElement>(2, std::move(cards), "in.cards", 12.0f, Insets{}));
    return page;
  }

  [[nodiscard]] std::vector<ElementPtr> displayPage(const GalleryState& state,
      const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> cards;

    std::vector<ElementPtr> progressBody;
    progressBody.push_back(components::ProgressBar(mFonts.body, state.progress, "disp.progress.bar"));
    std::vector<ElementPtr> progressButtons;
    progressButtons.push_back(components::Button("-10%", mFonts.body, [this] {
      change([](GalleryState& next) { next.progress = std::max(0.0, next.progress - 0.1); });
    }, "disp.progress.minus"));
    progressButtons.push_back(components::Button("+10%", mFonts.body, [this] {
      change([](GalleryState& next) { next.progress = std::min(1.0, next.progress + 0.1); });
    }, "disp.progress.plus"));
    progressButtons.push_back(components::Button("Reset", mFonts.body, [this] {
      change([](GalleryState& next) { next.progress = 0.35; });
    }, "disp.progress.reset"));
    progressBody.push_back(row("disp.progress.buttons", std::move(progressButtons), 8.0f));
    std::vector<ElementPtr> styledBody;
    TextDocument styledDocument;
    styledDocument.spans.push_back(TextSpan{ "Styled", TextStyle{ mFonts.title, theme.text, false },
      {} });
    styledDocument.spans.push_back(TextSpan{ "Text", TextStyle{ mFonts.title, theme.accent, false },
      {} });
    styledDocument.spans.push_back(TextSpan{ " is a document, not a run list.", TextStyle{
      mFonts.small, theme.textMuted, false }, {} });
    styledDocument.spans.push_back(TextSpan{ " docs", TextStyle{ mFonts.small, theme.accent, true },
      "styled.docs" });
    styledDocument.paragraph.maxLines = 2;
    styledDocument.paragraph.ellipsis = true;
    styledDocument.paragraph.selectable = true;
    styledBody.push_back(components::StyledText(styledDocument, [this](std::string_view id) {
      change([id = std::string(id)](GalleryState& next) { next.lastEvent = "Link " + id; });
    }, "disp.styled.text"));
    styledBody.push_back(label("StyledTextElement: spans, paragraph style, link hit-testing",
      mFonts.small, theme.textMuted, "disp.styled.caption"));
    cards.push_back(card("disp.styled", "StyledText", "R1 rendering + R2 links", std::move(styledBody)));
    std::vector<ElementPtr> moreBody;
    moreBody.push_back(components::Timeline({
      TimelineEntry{ "Build", "all targets", "09:12", TimelineState::Complete },
      TimelineEntry{ "Review", "waiting", "now", TimelineState::Current },
      TimelineEntry{ "Publish", "later", "--", TimelineState::Pending },
    }, mFonts.small, "disp.more.timeline"));
    moreBody.push_back(components::ColorPicker({ theme.accent, theme.controls.backgroundHover,
      theme.controls.border }, ColorPickerMode::Palette, theme.accent, 1.0f, 0, {},
      "disp.more.picker"));
    cards.push_back(card("disp.more", "Timeline & ColorPicker", "Data and palette leaves",
      std::move(moreBody)));
    cards.push_back(card("disp.progress", "ProgressBar", "A model fraction", std::move(progressBody)));

    std::vector<ElementPtr> spinnerRow;
    spinnerRow.push_back(components::Spinner(state.busy, theme.accent, "disp.spinner.spinner"));
    spinnerRow.push_back(label(state.busy ? "spinning" : "stopped", mFonts.body, theme.textMuted,
      "disp.spinner.state"));
    std::vector<ElementPtr> spinnerBody;
    spinnerBody.push_back(row("disp.spinner.row", std::move(spinnerRow), 10.0f));
    spinnerBody.push_back(components::Button("Toggle spinning", mFonts.body, [this] {
      change([](GalleryState& next) { next.busy = !next.busy; });
    }, "disp.spinner.toggle"));
    cards.push_back(card("disp.spinner", "Spinner", "Drives the frame loop while spinning",
      std::move(spinnerBody)));

    std::vector<ElementPtr> colorsBody;
    std::vector<ElementPtr> swatches;
    const Color palette[] = { theme.accent, theme.controls.backgroundHover, theme.controls.border };
    for (int index = 0; index < 3; ++index) {
      swatches.push_back(components::ColorButton(palette[index], "Palette " + std::to_string(index),
        [this] {
          change([](GalleryState& next) { next.lastEvent = "palette swatch picked"; });
        }, "disp.colors." + std::to_string(index)));
    }
    colorsBody.push_back(row("disp.colors.row", std::move(swatches), 10.0f));
    colorsBody.push_back(label("ColourButtonElement: a swatch with a button role", mFonts.small,
      theme.textMuted, "disp.colors.caption"));
    cards.push_back(card("disp.colors", "Colour", "Themed swatches", std::move(colorsBody)));

    std::vector<ElementPtr> typeBody;
    typeBody.push_back(label("Display", mFonts.title, theme.text, "disp.type.display"));
    typeBody.push_back(label("Section title", mFonts.section, theme.text, "disp.type.section"));
    typeBody.push_back(label("Body copy is 14 px with a muted variant below.", mFonts.body,
      theme.text, "disp.type.body"));
    typeBody.push_back(label("Muted caption", mFonts.small, theme.textMuted, "disp.type.small"));
    cards.push_back(card("disp.type", "Text styles", "TextElement carries its own font and colour",
      std::move(typeBody)));

    std::vector<ElementPtr> whenBody;
    whenBody.push_back(components::CheckBox("Show the conditional branch", mFonts.body, state.detail,
      [this](const bool value) {
        change([value](GalleryState& next) { next.detail = value; });
      }, "disp.when.check"));
    whenBody.push_back(components::When(state.detail,
      label("Branch A is mounted.", mFonts.body, theme.accent, "disp.when.then"),
      label("Branch B is mounted.", mFonts.body, theme.textMuted, "disp.when.else"),
      "disp.when.branch"));
    cards.push_back(card("disp.when", "When", "Two branches, two layer identities",
      std::move(whenBody)));

    std::vector<ElementPtr> tableBody;
    const std::vector<TableColumn> columns{ TableColumn{ 0.0f, 1.0f }, TableColumn{ 0.0f, 1.0f },
      TableColumn{ 0.0f, 2.0f } };
    const auto cell = [this](std::string value, const Color color) {
      return std::make_shared<TextLayer>(std::move(value), mFonts.small, color);
    };
    std::vector<std::vector<std::shared_ptr<Layer>>> rows;
    rows.push_back({ cell("Element", theme.accent), cell("Layer", theme.accent),
      cell("Patched in place", theme.accent) });
    rows.push_back({ cell("ButtonElement", theme.text), cell("Button", theme.textMuted),
      cell("label + callback", theme.textMuted) });
    rows.push_back({ cell("SliderElement", theme.text), cell("Slider", theme.textMuted),
      cell("value + callbacks", theme.textMuted) });
    rows.push_back({ cell("TableElement", theme.text), cell("Table", theme.textMuted),
      cell("columns + rows", theme.textMuted) });
    tableBody.push_back(components::Table(columns, rows, 6.0f, Insets{}.all(2.0f), "disp.table"));
    cards.push_back(card("disp.table", "Table", "Cells are live layers, not elements",
      std::move(tableBody)));

    std::vector<ElementPtr> leavesBody;
    std::vector<ElementPtr> leafRow;
    leafRow.push_back(components::Badge("New", mFonts.small, false, {}, "disp.leaves.badge"));
    leafRow.push_back(components::Kbd("Ctrl+K", mFonts.small, "disp.leaves.kbd"));
    leafRow.push_back(components::Avatar("CU", mFonts.small, 28.0f, {}, false, {}, "disp.leaves.avatar"));
    leafRow.push_back(components::AvatarGroup({ "A", "B", "C" }, mFonts.small, 4, 24.0f,
      "disp.leaves.group"));
    leafRow.push_back(components::StatusIndicator("Online", mFonts.small, StatusKind::Online,
      "disp.leaves.status"));
    leavesBody.push_back(row("disp.leaves.row", std::move(leafRow), 10.0f));
    std::vector<ElementPtr> leafMetrics;
    leafMetrics.push_back(components::ProgressRing(32.0f, 0.7, 3.0f, false, "disp.leaves.ring"));
    leafMetrics.push_back(components::Stat("Kinds", "80", mFonts.body, "first-class",
      StatTrend::Up, "disp.leaves.stat"));
    leafMetrics.push_back(components::Skeleton(120.0f, 12.0f, 1, 6.0f, Skeleton::Shape::Rounded,
      false, "disp.leaves.skeleton"));
    leavesBody.push_back(row("disp.leaves.metrics", std::move(leafMetrics), 14.0f));
    leavesBody.push_back(components::Divider(Divider::Orientation::Horizontal, 1.0f, 0.0f, {},
      "disp.leaves.divider"));
    leavesBody.push_back(components::Meter(0.0, 100.0, 64.0, "disp.leaves.meter"));
    leavesBody.push_back(components::StepIndicator({ "Draft", "Review", "Ship" }, mFonts.small, 1,
      {}, "disp.leaves.steps"));
    leavesBody.push_back(components::PropertyRow("Coverage", "80 / 89", mFonts.small,
      "first-class wrappers", 0.0f, "disp.leaves.property"));
    leavesBody.push_back(components::TagList({ "ui", "declarative", "new" }, mFonts.small, 4,
      "disp.leaves.tags"));
    cards.insert(cards.begin(), card("disp.leaves", "New declarative leaves",
      "Display widgets with no escape hatch", std::move(leavesBody)));

    page.push_back(std::make_shared<GridElement>(2, std::move(cards), "disp.cards", 12.0f, Insets{}));
    return page;
  }

  [[nodiscard]] std::vector<ElementPtr> listsPage(const GalleryState& state,
      const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> cards;

    std::vector<ElementPtr> listBody;
    listBody.push_back(components::ListView(state.tasks, mFonts.body, 28.0f,
      [this](const int index, const std::string& item) {
        change([index, item](GalleryState& next) {
          next.selectedTask = index;
          next.lastEvent = "list ->" + item;
        });
      }, state.selectedTask, "lists.listview"));
    listBody.push_back(label("selected index: " + std::to_string(state.selectedTask), mFonts.small,
      theme.textMuted, "lists.listview.readout"));
    cards.push_back(card("lists.listview-card", "ListView", "String rows, virtualized by the layer",
      std::move(listBody)));

    const auto rowBuilder = [this, theme](const std::string& key, const size_t,
        const RecyclingRowState& rowState) -> ElementPtr {
      std::vector<ElementPtr> children;
      children.push_back(swatch(rowState.selected ? theme.accent : theme.controls.border, 8.0f, 8.0f,
        4.0f, "recycle." + key + ".dot"));
      children.push_back(label(key, mFonts.small,
        rowState.selected ? theme.accent : theme.text, "recycle." + key + ".label"));
      return std::make_shared<StackElement>(Axis::Row, std::move(children), "recycle." + key,
        8.0f, Insets{}.all(4.0f), Align::Center);
    };
    auto recycling = components::RecyclingList(state.tasks, rowBuilder, 28.0f, "lists.recycle");
    ElementProps recycleProps;
    recycleProps.layout.basis = 160.0f;
    recycleProps.layout.shrink = 0.0f;
    recycleProps.ref = mRecyclingListRef;
    std::vector<ElementPtr> recycleBody;
    recycleBody.push_back(components::WithProps(std::move(recycling), recycleProps,
      "lists.recycle.wrap"));
    recycleBody.push_back(label("Every row is a real Element subtree returned by the builder",
      mFonts.small, theme.textMuted, "lists.recycle.caption"));
    cards.push_back(card("lists.recycle-card", "RecyclingList", "Keyed, per-row virtualized",
      std::move(recycleBody)));

    std::vector<ElementPtr> mutateBody;
    std::vector<ElementPtr> mutateButtons;
    mutateButtons.push_back(components::Button("Add task", mFonts.body, [this] {
      change([](GalleryState& next) {
        next.tasks.push_back("Task " + std::to_string(next.tasks.size() + 1));
        next.lastEvent = "task added";
      });
    }, "lists.mutate.add"));
    mutateButtons.push_back(components::Button("Remove last", mFonts.body, [this] {
      change([](GalleryState& next) {
        if (next.tasks.size() > 1) next.tasks.pop_back();
        next.selectedTask = std::min(next.selectedTask, static_cast<int>(next.tasks.size()) - 1);
        next.lastEvent = "task removed";
      });
    }, "lists.mutate.remove"));
    mutateButtons.push_back(components::Button("Reverse", mFonts.body, [this] {
      change([](GalleryState& next) {
        std::reverse(next.tasks.begin(), next.tasks.end());
        next.lastEvent = "tasks reversed";
      });
    }, "lists.mutate.reverse"));
    mutateBody.push_back(row("lists.mutate.row", std::move(mutateButtons), 8.0f));
    mutateBody.push_back(label(std::to_string(state.tasks.size()) + " task(s); keyed rows keep "
      "their layers while the list is reordered.", mFonts.small, theme.textMuted, "lists.mutate.caption"));
    cards.push_back(card("lists.mutate-card", "Keyed updates", "The model owns the order",
      std::move(mutateBody)));

    std::vector<ElementPtr> summaryBody;
    std::vector<ElementPtr> taskRows;
    for (size_t index = 0; index < state.tasks.size(); ++index) {
      taskRows.push_back(label(std::to_string(index + 1) + ".  " + state.tasks[index], mFonts.body,
        theme.text, "lists.summary." + std::to_string(index)));
    }
    summaryBody.push_back(std::make_shared<StackElement>(Axis::Column, std::move(taskRows),
      "lists.summary.stack", 4.0f, Insets{}, Align::Stretch));
    cards.push_back(card("lists.summary-card", "Plain ForEach", "The same data without virtualization",
      std::move(summaryBody)));

    page.push_back(std::make_shared<GridElement>(2, std::move(cards), "lists.cards", 12.0f, Insets{}));
    return page;
  }

  [[nodiscard]] std::vector<ElementPtr> statePage(const GalleryState& state,
      const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> cards;

    std::vector<ElementPtr> stateBody;
    stateBody.push_back(label("State<GalleryState> is a value with observers. ElementHost binds it "
      "and coalesces every write in one UI turn into a single reconcile.", mFonts.body,
      theme.textMuted, "state.model.text"));
    stateBody.push_back(label("clicks: " + std::to_string(state.clicks), mFonts.section, theme.text,
      "state.model.count"));
    std::vector<ElementPtr> stateButtons;
    stateButtons.push_back(components::Button("+1 click", mFonts.body, [this] {
      change([](GalleryState& next) {
        ++next.clicks;
        next.lastEvent = "click counted";
      });
    }, "state.model.plus"));
    stateButtons.push_back(components::Button("Reset", mFonts.body, [this] {
      change([](GalleryState& next) {
        next.clicks = 0;
        next.lastEvent = "clicks reset";
      });
    }, "state.model.reset"));
    stateBody.push_back(row("state.model.buttons", std::move(stateButtons), 8.0f));
    stateBody.push_back(label("rebuilds: " + std::to_string(mRebuilds), mFonts.small,
      theme.textMuted, "state.model.rebuilds"));
    cards.push_back(card("state.model", "State + ElementHost", "Model, not view", std::move(stateBody)));

    std::vector<ElementPtr> forEachBody;
    std::vector<ElementPtr> tags;
    for (const std::string& task: state.tasks) {
      auto chip = std::make_shared<StackElement>(Axis::Row,
        std::vector<ElementPtr>{ label(task, mFonts.small, theme.text,
          "state.foreach." + task + ".text") },
        "state.foreach." + task + ".stack", 0.0f, Insets{}.all(8.0f), Align::Center);
      ElementProps tagProps;
      tagProps.backgroundRole = ThemeColorRole::SurfaceRaised;
      tagProps.clip = false;
      tagProps.layout.shrink = 0.0f;
      tags.push_back(components::WithProps(std::move(chip), tagProps, "state.foreach." + task));
    }
    forEachBody.push_back(label("components::ForEach maps the task list to keyed children.",
      mFonts.small, theme.textMuted, "state.foreach.caption"));
    forEachBody.push_back(std::make_shared<StackElement>(Axis::Column, std::move(tags),
      "state.foreach.tags", 4.0f, Insets{}, Align::Start));
    cards.push_back(card("state.foreach", "ForEach", "Keyed mapping helper", std::move(forEachBody)));

    std::vector<ElementPtr> componentBody;
    componentBody.push_back(components::Component([this, state] {
      std::vector<ElementPtr> children;
      children.push_back(label("A ComponentElement rebuilt from the same snapshot.",
        mFonts.small, mWindow.theme().textMuted, "state.component.text"));
      children.push_back(swatch(mWindow.theme().accent, 40.0f + static_cast<float>(state.clicks) * 4.0f,
        10.0f, 5.0f, "state.component.bar"));
      return std::make_shared<StackElement>(Axis::Column, std::move(children), "state.component.stack",
        6.0f, Insets{}, Align::Stretch);
    }, "state.component"));
    cards.push_back(card("state.component", "Component", "A function element with its own identity",
      std::move(componentBody)));

    std::vector<ElementPtr> registryBody;
    std::vector<ElementPtr> pills;
    pills.push_back(mRegistry.build("Pill", {}, {
      label("app-defined type", mFonts.small, theme.text, "state.registry.text"),
    }, "state.registry.pill"));
    registryBody.push_back(label("mRegistry.add(\"Pill\", builder) lets a name resolve to ordinary "
      "elements, so the reconciler's patch switch stays closed.", mFonts.small, theme.textMuted,
      "state.registry.caption"));
    registryBody.push_back(row("state.registry.row", std::move(pills), 6.0f));
    cards.push_back(card("state.registry", "ElementRegistry", "Named extension points",
      std::move(registryBody)));

    std::vector<ElementPtr> specBody;
    ElementSpecOptions specOptions;
    specOptions.font = mFonts.body;
    ElementPtr parsed = parseElementSpec(
      "CheckBox#spec_checkbox text=\"Parsed from a flat spec\" checked=true", mRegistry,
      specOptions);
    if (parsed != nullptr) {
      specBody.push_back(ElementPtr(parsed));
    } else {
      specBody.push_back(label("the spec did not parse", mFonts.small, theme.textMuted,
        "state.spec.error"));
    }
    specBody.push_back(label("parseElementSpec turns text into ordinary elements; the reconciler "
      "never sees the string form.", mFonts.small, theme.textMuted, "state.spec.caption"));
    cards.push_back(card("state.spec", "ElementSpec", "A flat text form (v0)", std::move(specBody)));

    std::vector<ElementPtr> detailBody;
    detailBody.push_back(components::CheckBox("Show the detail branch", mFonts.body, state.detail,
      [this](const bool value) {
        change([value](GalleryState& next) { next.detail = value; });
      }, "state.detail.check"));
    detailBody.push_back(components::When(state.detail,
      label("When(then): this subtree keeps its layer until the condition changes.",
        mFonts.small, theme.accent, "state.detail.then"),
      label("When(else): a different subtree is mounted in the same slot.",
        mFonts.small, theme.textMuted, "state.detail.else"),
      "state.detail.branch"));
    cards.push_back(card("state.detail", "When", "One branch at a time", std::move(detailBody)));

    page.push_back(std::make_shared<GridElement>(2, std::move(cards), "state.cards", 12.0f, Insets{}));
    return page;
  }

  [[nodiscard]] std::vector<ElementPtr> escapeHatchPage(const GalleryState&,
      const Theme& theme) {
    std::vector<ElementPtr> page;
    std::vector<ElementPtr> intro;
    intro.push_back(label(
      "These widgets are full Layer implementations without a first-class Element wrapper yet. "
      "components::Layer hosts a prebuilt layer inside the element tree, so they can be used today; "
      "the trade-off is that the layer owns its state and props, and a new instance replaces the "
      "node instead of patching it.",
      mFonts.body, theme.textMuted, "esc.intro.text"));
    page.push_back(card("esc.intro", "Escape hatch", "components::Layer", std::move(intro)));

    std::vector<ElementPtr> cards;
    std::vector<ElementPtr> cardBody;
    cardBody.push_back(components::Layer(mCardWidget, "esc.card.widget"));
    cardBody.push_back(label("Card also has a declarative wrapper; this page hosts the layer directly.",
      mFonts.small, theme.textMuted, "esc.card.caption"));
    cards.push_back(card("esc.card", "Card", "A real Card layer", std::move(cardBody)));

    std::vector<ElementPtr> chipBody;
    std::vector<ElementPtr> chipRow;
    chipRow.push_back(components::Layer(mChipWidget, "esc.chip.widget"));
    chipRow.push_back(components::Layer(mBadgeWidget, "esc.badge.widget"));
    chipBody.push_back(row("esc.chip.row", std::move(chipRow), 10.0f));
    chipBody.push_back(label("Chip stays a hosted layer; Badge now has a wrapper and can still be hosted.",
      mFonts.small, theme.textMuted, "esc.chip.caption"));
    cards.push_back(card("esc.chip", "Chip & Badge", "Two hosted widgets", std::move(chipBody)));
    page.push_back(std::make_shared<GridElement>(2, std::move(cards), "esc.cards", 12.0f, Insets{}));

    std::vector<ElementPtr> missing;
    missing.push_back(label(
      "Not first-class yet: Dialog, Menu, MenuBar, NativeViewLayer, OverlayLayer, Popup, Toast, "
      "ToastStack and Tooltip.",
      mFonts.small, theme.textMuted, "esc.missing.text"));
    page.push_back(card("esc.missing", "The rest of the catalogue",
      "9 Layer types without an element wrapper", std::move(missing)));
    return page;
  }

  Window& mWindow;
  Fonts mFonts;
  std::shared_ptr<const graphics::Image> mIcon;
  State<GalleryState> mModel;
  Theme mDark;
  Theme mLight;
  ElementRegistry mRegistry;
  std::shared_ptr<LayerRef> mFooterRef = std::make_shared<LayerRef>();
  std::shared_ptr<LayerRef> mContentScrollRef = std::make_shared<LayerRef>();
  std::shared_ptr<LayerRef> mRecyclingListRef = std::make_shared<LayerRef>();
  std::unique_ptr<ElementHost> mHost;
  std::unique_ptr<OverlayHost> mOverlayHost;
  coral::observer::Observation mBinding;
  uint32_t mRebuilds = 0;
  std::shared_ptr<Card> mCardWidget;
  std::shared_ptr<Chip> mChipWidget;
  std::shared_ptr<Badge> mBadgeWidget;
};

} // namespace anonymous

int main(int argc, char** argv) {
  bool headless = false;
  bool selfCheck = false;
  bool dumpTree = false;
  bool toast = false;
  int frames = -1;
  int width = 1180;
  int height = 780;
  int section = 0;
  float scroll = -1.0f;
  std::string output;
  std::string themeName = "dark";

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    const auto next = [&]() -> std::string { return index + 1 < argc ? argv[++index] : std::string(); };
    if (argument == "--headless") {
      headless = true;
    } else if (argument == "--frames") {
      frames = std::max(0, std::atoi(next().c_str()));
    } else if (argument == "--out") {
      output = next();
    } else if (argument == "--width") {
      width = std::max(640, std::atoi(next().c_str()));
    } else if (argument == "--height") {
      height = std::max(480, std::atoi(next().c_str()));
    } else if (argument == "--section") {
      section = std::max(0, std::atoi(next().c_str()));
    } else if (argument == "--scroll") {
      scroll = static_cast<float>(std::atof(next().c_str()));
    } else if (argument == "--theme") {
      themeName = next();
    } else if (argument == "--self-check") {
      selfCheck = true;
    } else if (argument == "--toast") {
      toast = true;
    } else if (argument == "--dump-tree") {
      dumpTree = true;
    } else if (argument == "--help") {
      std::printf("usage: ui_declarative_gallery [--headless] [--frames N] [--out FILE] "
        "[--section N] [--theme dark|light] [--width W] [--height H] [--self-check] [--dump-tree]\n");
      return 0;
    } else {
      std::fprintf(stderr, "unknown argument: %s\n", argument.c_str());
      return 2;
    }
  }

  if (frames < 0) frames = headless ? 2 : 0;   // headless: a couple of frames; native: stay open

  const std::string path = fontPath();
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    std::fprintf(stderr, "could not load the reference font %s\n", path.c_str());
    return 1;
  }
  Fonts fonts;
  fonts.body = graphics::Font::create(typeface, 14.0f);
  fonts.section = graphics::Font::create(typeface, 17.0f);
  fonts.title = graphics::Font::create(typeface, 26.0f);
  fonts.small = graphics::Font::create(typeface, 12.0f);
  if (fonts.body == nullptr || fonts.section == nullptr || fonts.title == nullptr ||
      fonts.small == nullptr) {
    std::fprintf(stderr, "could not create the gallery fonts\n");
    return 1;
  }

  Window::Options options;
  options.width = width;
  options.height = height;
  options.headless = headless;
  options.title = "CoralUI declarative gallery";
  options.frameLimit = headless ? std::max(1, frames) + (selfCheck ? 1 : 0) : frames;
  auto window = Window::create(options);
  if (window == nullptr) {
    std::fprintf(stderr, "the UI window could not be created: %s\n",
      Window::lastCreationError().c_str());
    return 1;
  }

  GalleryApp app(*window, fonts, makeStarIcon());
  app.configure(section, themeName != "light");
  app.mount();
  if (toast) app.showToast();
  if (scroll >= 0.0f) app.scrollContent(scroll);

  if (selfCheck) {
    if (!app.selfCheck()) return 1;
  }

  const int rendered = window->run();
  if (dumpTree) app.dumpTree();
  if (rendered == 0) {
    std::fprintf(stderr, "the UI window never rendered: %s\n", window->diagnostics().c_str());
    return 1;
  }
  if (!output.empty()) {
    graphics::Bitmap bitmap;
    if (!window->snapshot(&bitmap) || !writePng(bitmap, output)) {
      std::fprintf(stderr, "could not write %s\n", output.c_str());
      return 1;
    }
  }
  std::printf("ui_declarative_gallery %s, %d frame(s), start section %d, %s theme\n",
    headless ? "headless" : "window", rendered, std::clamp(section, 0, kPageCount - 1),
    themeName.c_str());
  return 0;
}
