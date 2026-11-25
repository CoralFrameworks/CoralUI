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

#include "gallery_sections.h"

#include <ui/Avatar.h>
#include <ui/Table.h>
#include <ui/ListView.h>
#include <ui/TreeView.h>
#include <ui/Tooltip.h>
#include <ui/Popup.h>
#include <ui/Dialog.h>
#include <ui/Toast.h>
#include <ui/TabBar.h>
#include <ui/StatusBar.h>
#include <ui/SplitView.h>
#include <ui/Skeleton.h>
#include <ui/SegmentedControl.h>
#include <ui/NumberField.h>
#include <ui/IconButton.h>
#include <ui/ColorPicker.h>
#include <ui/ColorInput.h>
#include <ui/ColorButton.h>
#include <ui/Breadcrumb.h>
#include <ui/Binding.h>
#include <ui/mvvm/DataContext.h>
#include <mvvm/ObservableObject.h>
#include <ui/AvatarGroup.h>
#include <ui/Badge.h>
#include <ui/Banner.h>
#include <ui/Button.h>
#include <ui/Calendar.h>
#include <ui/Card.h>
#include <ui/CheckBox.h>
#include <ui/CheckBoxGroup.h>
#include <ui/Chip.h>
#include <ui/ColorLayer.h>
#include <ui/ComboBox.h>
#include <ui/DateTimeField.h>
#include <ui/DateField.h>
#include <ui/DateRangeField.h>
#include <ui/Divider.h>
#include <ui/DropZone.h>
#include <ui/Element.h>
#include <ui/EmptyState.h>
#include <ui/Expander.h>
#include <ui/Flex.h>
#include <ui/FormField.h>
#include <ui/Icon.h>
#include <ui/Layer.h>
#include <ui/Kbd.h>
#include <ui/Link.h>
#include <ui/Menu.h>
#include <ui/MenuBar.h>
#include <ui/Meter.h>
#include <ui/NativeAlert.h>
#include <ui/FileDialog.h>
#include <ui/DatePickerPanel.h>
#include <ui/MultiSelect.h>
#include <ui/Pagination.h>
#include <ui/PasswordField.h>
#include <ui/ProgressBar.h>
#include <ui/ProgressRing.h>
#include <ui/PropertyRow.h>
#include <ui/RadioGroup.h>
#include <ui/RecyclingListView.h>
#include <ui/RangeSlider.h>
#include <ui/Rating.h>
#include <ui/SearchField.h>
#include <ui/ScrollView.h>
#include <ui/SectionHeader.h>
#include <ui/Select.h>
#include <ui/Slider.h>
#include <ui/Spinner.h>
#include <ui/SplitButton.h>
#include <ui/Stat.h>
#include <ui/StepIndicator.h>
#include <ui/StatusIndicator.h>
#include <ui/Stepper.h>
#include <ui/TagInput.h>
#include <ui/TagList.h>
#include <ui/CandidateListView.h>
#include <ui/RichTextLayer.h>
#include <ui/TableView.h>
#include <ui/TimePicker.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>
#include <ui/Theme.h>
#include <ui/TimeField.h>
#include <ui/Timeline.h>
#include <ui/TimeRangeField.h>
#include <ui/ToastStack.h>
#include <ui/Toggle.h>
#include <ui/ToggleButton.h>
#include <ui/ToggleGroup.h>
#include <ui/Toolbar.h>

#if defined(__APPLE__)
#include <ui/macos/NativeButton.h>
#include <ui/macos/NativePopUpButton.h>
#include <ui/macos/NativeProgressIndicator.h>
#include <ui/macos/NativeSearchField.h>
#include <ui/macos/NativeStepper.h>
#include <ui/macos/NativeSwitch.h>
#endif

#include <graphics/Bitmap.h>
#include <graphics/Image.h>
#include <graphics/Pixmap.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using namespace coral::ui;
namespace observer = coral::observer;

// The gallery page lays its right panel out in absolute window coordinates. Controls (and their
// popup anchors) compare PointerEvent positions against their bounds, so a parent translation would
// make every hit test use the wrong origin.
constexpr float kContentX = 220.0f;
constexpr float kBodyY = 96.0f;

/** The gallery's model state: the selected section and, when applicable, its control row. */
struct GallerySelection {
  int section = 0;
  std::string control;

  bool operator==(const GallerySelection& other) const {
    return section == other.section && control == other.control;
  }
};

/** Observable state lives in the view model; the list rows and page are projections of it. */
class GalleryViewModel final : public coral::mvvm::ObservableObject {
 public:
  observer::Property<GallerySelection> selection{GallerySelection{}};
  observer::Property<std::string> event{"Interact with a control"};
  observer::Property<std::string> value{"—"};
  observer::Property<int> clicks{0};
  observer::Property<bool> toggle{true};
  observer::Property<bool> checked{true};
  observer::Property<bool> toggleButton{true};
  observer::Property<int> radio{0};
  observer::Property<std::vector<int>> checks{{0, 2}};
  observer::Property<std::vector<int>> toggles{{0}};
  observer::Property<double> rating{3.5};
  observer::Property<double> slider{60.0};
  observer::Property<double> rangeLow{20.0};
  observer::Property<double> rangeHigh{80.0};
  observer::Property<double> stepper{4.0};
  observer::Property<double> number{1.5};
  observer::Property<int> select{0};
  observer::Property<int> combo{0};
  observer::Property<std::vector<int>> multi{{0, 2}};
  observer::Property<int> page{2};
  observer::Property<int> currentStep{1};
  observer::Property<int> segmented{0};
  observer::Property<bool> expanded{true};
  observer::Property<std::string> text{""};
  observer::Property<std::string> search{""};
  observer::Property<std::string> dropped{""};
};

std::shared_ptr<const graphics::Image> galleryIcon() {
  graphics::Bitmap bitmap = graphics::Bitmap::allocate(16, 16);
  graphics::Pixmap pixels = bitmap.lockPixels();
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const float dx = static_cast<float>(x) - 7.5f;
      const float dy = static_cast<float>(y) - 7.5f;
      if (dx * dx + dy * dy <= 7.0f * 7.0f) {
        pixels.setPixelARGB32(x, y, 0xFFFFFFFFu);
      }
    }
  }
  return std::make_shared<const graphics::Image>(graphics::Image::fromBitmap(bitmap));
}

void addLabel(Layer& parent, const Theme& theme, const std::string& text,
    const float x, const float y, const float width = 240.0f) {
  auto label = std::make_shared<TextLayer>(text, theme.body, theme.textMuted);
  label->setBounds(Rect::MakeXYWH(kContentX + x, kBodyY + y, width, 18.0f));
  parent.addChild(label);
}

void addControl(Layer& parent, const std::shared_ptr<Layer>& control, const Rect& bounds) {
  control->setBounds(Rect::MakeXYWH(kContentX + bounds.getLeft(), kBodyY + bounds.getTop(),
    bounds.getWidth(), bounds.getHeight()));
  parent.addChild(control);
}

std::shared_ptr<Layer> makeListContent(const Theme& theme, const int rows,
    const std::string& prefix, const float width) {
  auto content = std::make_shared<Flex>(Axis::Column);
  content->setGap(4.0f);
  content->setCrossAlign(Align::Stretch);
  for (int index = 0; index < rows; ++index) {
    auto row = std::make_shared<Button>(prefix + " " + std::to_string(index + 1), theme.body);
    row->setBounds(Rect::MakeWH(width, 28.0f));
    content->addChild(row);
  }
  return content;
}

class DemoFrame final : public Layer {
 public:
  explicit DemoFrame(const Theme& theme)
      : mBackground(theme.surface), mBorder(theme.controls.accent) {}

  void drawSelf(RenderContext& context) override {
    auto& canvas = context.canvas();
    graphics::Paint fill;
    fill.color = mBackground;
    fill.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 8.0f), fill);
    graphics::Paint border;
    border.color = mBorder;
    border.style = graphics::Paint::Style::kStroke;
    border.strokeWidth = 1.5f;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 8.0f), border);
  }

 private:
  Color mBackground;
  Color mBorder;
};

class SidebarRow final : public Layer {
 public:
  SidebarRow(std::string label, std::shared_ptr<const Font> font, Color color,
      Color selectedColor, bool selected, const Theme& theme, const float indent)
      : mLabel(std::move(label)), mSelected(selected), mIndent(indent),
        mSelectedBackground(theme.controls.accent) {
    setAccessibleRole(AccessibilityRole::Button);
    setAccessibleName(mLabel);
    setAccessibleSelected(mSelected);
    mText = std::make_shared<TextLayer>(mLabel, std::move(font),
      mSelected ? selectedColor : color);
    addChild(mText);
  }

  void update(const std::string& label, const bool selected, const Color color,
      const Color selectedColor, const float indent) {
    mLabel = label;
    mSelected = selected;
    mIndent = indent;
    setAccessibleName(mLabel);
    setAccessibleSelected(selected);
    if (mText != nullptr) {
      mText->setText(mLabel);
      mText->setColor(selected ? selectedColor : color);
      mText->layout();
    }
    setNeedsLayout();
    setNeedsPaint();
  }

  Size measure(const Size& available) override {
    setMeasuredSize({ available.width, 26.0f });
    return measuredSize();
  }

  void onBoundsSizeChanged() override { layout(); }

  void positionText() {
    if (mText == nullptr) return;
    mText->setBounds(Rect::MakeXYWH(bounds().getLeft() + 8.0f + mIndent,
      bounds().getTop() + (bounds().getHeight() - 18.0f) * 0.5f,
      std::max(0.0f, bounds().getWidth() - 16.0f - mIndent), 18.0f));
    mText->layout();
  }

  void layout() override {
    positionText();
    Layer::layout();
  }

  void drawSelf(RenderContext& context) override {
    // Recycling reattaches a cached row at a new y. The size does not change, so the text child would
    // keep its old bounds without this per-paint position update.
    positionText();
    if (!mSelected) return;
    auto& canvas = context.canvas();
    graphics::Paint fill;
    fill.color = mSelectedBackground;
    fill.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 4.0f), fill);
  }

 private:
  std::string mLabel;
  std::shared_ptr<TextLayer> mText;
  bool mSelected = false;
  float mIndent = 0.0f;
  Color mSelectedBackground;
};

class SidebarScrollbar final : public Layer {
 public:
  explicit SidebarScrollbar(std::shared_ptr<RecyclingListView> list)
      : mList(std::move(list)) {
    setAccessibleRole(AccessibilityRole::Generic);
    setAccessibleName("Section list scrollbar");
  }

  bool onPointerDown(const PointerEvent& event) override {
    if (mList == nullptr) return false;
    const Rect thumb = mList->scrollbarThumbBounds();
    if (thumb.isEmpty()) return false;
    mStartY = event.position.y;
    mStartScroll = mList->scrollY();
    if (!thumb.contains(event.position)) {
      const float track = mList->bounds().getHeight();
      const float maxScroll = std::max(0.0f, mList->contentSize().height - track);
      const float travel = std::max(1.0f, track - thumb.getHeight());
      const float target = std::clamp((event.position.y - mList->bounds().getTop() -
        thumb.getHeight() * 0.5f) / travel, 0.0f, 1.0f) * maxScroll;
      mList->setScrollOffset(0.0f, target);
      mStartScroll = target;
    }
    mDragging = true;
    return true;
  }

  bool onPointerMove(const PointerEvent& event) override {
    if (!mDragging || mList == nullptr) return false;
    const Rect thumb = mList->scrollbarThumbBounds();
    if (thumb.isEmpty()) return true;
    const float track = mList->bounds().getHeight();
    const float travel = std::max(1.0f, track - thumb.getHeight());
    const float maxScroll = std::max(0.0f, mList->contentSize().height - track);
    mList->setScrollOffset(0.0f, mStartScroll +
      (event.position.y - mStartY) * (maxScroll / travel));
    return true;
  }

  bool onPointerUp(const PointerEvent&) override {
    mDragging = false;
    return true;
  }

 private:
  std::shared_ptr<RecyclingListView> mList;
  float mStartY = 0.0f;
  float mStartScroll = 0.0f;
  bool mDragging = false;
};

class SectionedGallery final : public Layer {
 public:
  SectionedGallery(const Theme& theme, const bool nativeDisabled,
      std::shared_ptr<TextInput>& inputField, const float width, const float height,
      const int initialSection, std::string initialControl)
      : mTheme(theme), mNativeDisabled(nativeDisabled), mInputField(inputField) {
    setAccessibleRole(AccessibilityRole::Group);
    setAccessibleName("CoralUI control gallery");
    setBounds(Rect::MakeWH(width, height));

    mSidebar = std::make_shared<ColorLayer>(mTheme.surface);
    addChild(mSidebar);
    mContent = std::make_shared<ColorLayer>(mTheme.background);
    addChild(mContent);

    mDivider = std::make_shared<ColorLayer>(mTheme.controls.border);
    addChild(mDivider);

    mTitle = std::make_shared<TextLayer>("CoralUI", mTheme.title, mTheme.controls.foreground);
    mSidebar->addChild(mTitle);
    mSubtitle = std::make_shared<TextLayer>("Control gallery", mTheme.body, mTheme.textMuted);
    mSidebar->addChild(mSubtitle);

    mPageTitle = std::make_shared<TextLayer>("", mTheme.title, mTheme.controls.foreground);
    mContent->addChild(mPageTitle);
    mPageDescription = std::make_shared<TextLayer>("", mTheme.body, mTheme.textMuted);
    mContent->addChild(mPageDescription);
    mPageControls = std::make_shared<TextLayer>("", mTheme.body, mTheme.textMuted);
    mContent->addChild(mPageControls);

    mBody = std::make_shared<Layer>();
    mContent->addChild(mBody);

    buildSections();
    mExpanded.assign(mSections.size(), true);
    buildSidebar();
    const int safeSection = std::clamp(initialSection, 0,
      std::max(0, static_cast<int>(mSections.size()) - 1));
    // The DataContext is the scoped model container; the Property is the single source of truth for
    // selection. Selecting a row only writes the model; the binding schedules the page update.
    mContext.set(mModel);
    mModel = mContext.get<GalleryViewModel>();
    mSelectionObservation = bindProperty(mModel->selection, *this,
      [this](const GallerySelection& value) { queueSelection(value); }, nullptr, false);
    int selectedSection = safeSection;
    if (!initialControl.empty()) {
      for (std::size_t index = 0; index < mSections.size(); ++index) {
        const std::string& controls = mSections[index].controls;
        std::size_t begin = 0;
        while (begin < controls.size()) {
          const std::size_t separator = controls.find(" / ", begin);
          const std::string name = controls.substr(begin, separator == std::string::npos
            ? std::string::npos : separator - begin);
          if (name == initialControl) {
            selectedSection = static_cast<int>(index);
            break;
          }
          if (separator == std::string::npos) break;
          begin = separator + 3;
        }
        if (selectedSection != safeSection) break;
      }
    }
    mModel->selection.set(GallerySelection{ selectedSection, std::move(initialControl) });
    mPendingSelection = mModel->selection.get();
    mNeedApplySelection = true;
    layout();
  }

  void layout() override {
    if (mNeedApplySelection) {
      mNeedApplySelection = false;
      applySelectionNow(mPendingSelection);
    }
    const float width = bounds().getWidth();
    const float height = bounds().getHeight();
    const float sidebarWidth = 220.0f;
    mSidebar->setBounds(Rect::MakeWH(sidebarWidth, height));
    mContent->setBounds(Rect::MakeXYWH(sidebarWidth, 0.0f,
      std::max(320.0f, width - sidebarWidth), height));
    mDivider->setBounds(Rect::MakeXYWH(sidebarWidth - 1.0f, 0.0f, 1.0f, height));
    mTitle->setBounds(Rect::MakeXYWH(18.0f, 14.0f, sidebarWidth - 36.0f, 30.0f));
    mSubtitle->setBounds(Rect::MakeXYWH(18.0f, 40.0f, sidebarWidth - 36.0f, 18.0f));
    mNavList->setBounds(Rect::MakeXYWH(0.0f, 64.0f, sidebarWidth,
      std::max(80.0f, height - 64.0f)));
    mNavList->layout();
    if (mNeedReveal) {
      mNeedReveal = false;
      const GallerySelection selection = mModel->selection.get();
      std::string revealKey = "s:" + std::to_string(selection.section);
      if (!selection.control.empty()) {
        for (const auto& row: mNavRows) {
          if (row.section == static_cast<std::size_t>(selection.section) &&
              row.label == selection.control) {
            revealKey = row.key;
            break;
          }
        }
      }
      mNavList->scrollToKey(revealKey);
    }
    mScrollbar->setBounds(Rect::MakeXYWH(sidebarWidth - 12.0f, 64.0f, 12.0f,
      std::max(80.0f, height - 64.0f)));
    mPageTitle->setBounds(Rect::MakeXYWH(sidebarWidth + 24.0f, 16.0f,
      std::max(200.0f, width - sidebarWidth - 48.0f), 30.0f));
    mPageDescription->setBounds(Rect::MakeXYWH(sidebarWidth + 24.0f, 46.0f,
      std::max(200.0f, width - sidebarWidth - 48.0f), 18.0f));
    mPageControls->setBounds(Rect::MakeXYWH(sidebarWidth + 24.0f, 68.0f,
      std::max(200.0f, width - sidebarWidth - 48.0f), 18.0f));
    mBody->setBounds(Rect::MakeXYWH(sidebarWidth, kBodyY,
      std::max(320.0f, width - sidebarWidth), std::max(120.0f, height - kBodyY)));
    Layer::layout();
  }

 private:
  struct Section {
    std::string title;
    std::string description;
    std::string controls;
    std::function<void(Layer&, const Theme&)> build;
  };

  struct NavRow {
    std::string key;
    std::string label;
    std::size_t section = 0;
    int item = -1;
    bool header = false;
  };

  void buildSections();
  void buildSidebar();
  void rebuildNavRows(const std::string& selectedKey);
  void onNavSelected(const std::string& key);
  void queueSelection(const GallerySelection& selection);
  void applySelectionNow(const GallerySelection& selection);
  void showSection(std::size_t index, std::string control = {});
  ElementPtr rowElement(const std::string& key, const RecyclingRowState& state);

  void buildInputs(Layer& body, const Theme& theme);
  void buildSelection(Layer& body, const Theme& theme);
  void buildDateTime(Layer& body, const Theme& theme);
  void buildFeedback(Layer& body, const Theme& theme);
  void buildData(Layer& body, const Theme& theme);
  void buildLayout(Layer& body, const Theme& theme);
  void buildLists(Layer& body, const Theme& theme);
  void buildNative(Layer& body, const Theme& theme);
  void buildMore(Layer& body, const Theme& theme);
  void buildControl(Layer& body, const Theme& theme, const std::string& control);

  Theme mTheme;
  bool mNativeDisabled = false;
  std::shared_ptr<TextInput>& mInputField;
  std::shared_ptr<Layer> mSidebar;
  std::shared_ptr<Layer> mContent;
  std::shared_ptr<Layer> mBody;
  std::shared_ptr<GalleryViewModel> mModel = std::make_shared<GalleryViewModel>();
  coral::ui::mvvm::DataContext mContext;
  observer::Observation mSelectionObservation;
  std::shared_ptr<RecyclingListView> mNavList;
  std::shared_ptr<SidebarScrollbar> mScrollbar;
  std::unordered_map<std::string, std::shared_ptr<SidebarRow>> mRowLayers;
  std::vector<observer::Observation> mPageObservations;
  std::shared_ptr<Layer> mDivider;
  std::shared_ptr<TextLayer> mTitle;
  std::shared_ptr<TextLayer> mSubtitle;
  std::shared_ptr<TextLayer> mPageTitle;
  std::shared_ptr<TextLayer> mPageDescription;
  std::shared_ptr<TextLayer> mPageControls;
  std::vector<Section> mSections;
  std::vector<NavRow> mNavRows;
  std::vector<bool> mExpanded;
  bool mUpdatingNav = false;
  bool mNeedReveal = false;
  bool mNeedApplySelection = false;
  GallerySelection mPendingSelection;
};

void SectionedGallery::buildSections() {
  mSections = {
    { "Inputs", "Buttons, text fields and pickers.", "Button / SplitButton / SearchField / TextInput / PasswordField / Select / ComboBox / MultiSelect",
      [this](Layer& body, const Theme& theme) { buildInputs(body, theme); } },
    { "Selection", "Toggle, check, radio and slider controls.", "Toggle / CheckBox / RadioGroup / CheckBoxGroup / ToggleGroup / Rating / Slider / RangeSlider / Stepper",
      [this](Layer& body, const Theme& theme) { buildSelection(body, theme); } },
    { "Date & Time", "Date, time and range fields.", "DateField / TimeField / DateTimeField / DateRangeField / TimeRangeField / Calendar",
      [this](Layer& body, const Theme& theme) { buildDateTime(body, theme); } },
    { "Feedback", "Progress, banners, toasts and empty states.", "ProgressBar / ProgressRing / Meter / Spinner / Banner / ToastStack / EmptyState / StatusIndicator / Pagination",
      [this](Layer& body, const Theme& theme) { buildFeedback(body, theme); } },
    { "Data", "Cards, avatars, tags and metadata.", "Card / AvatarGroup / Badge / TagList / Link / Kbd / Stat / Timeline / PropertyRow",
      [this](Layer& body, const Theme& theme) { buildData(body, theme); } },
    { "Layout", "Containers, headers and drop targets.", "SectionHeader / FormField / Expander / Toolbar / Divider / DropZone / StepIndicator",
      [this](Layer& body, const Theme& theme) { buildLayout(body, theme); } },
    { "Lists", "Scrollable lists, including a nested ScrollView.", "ScrollView / List rows / Nested ScrollView",
      [this](Layer& body, const Theme& theme) { buildLists(body, theme); } },
    { "Native macOS", "AppKit controls hosted as native views.", "NativeButton / NativeSwitch / NativeSearchField / NativeStepper / NativeProgressIndicator / NativePopUpButton",
      [this](Layer& body, const Theme& theme) { buildNative(body, theme); } },
    { "More", "Additional framework controls.", "IconButton / ToggleButton / NumberField / SegmentedControl / ColorInput / ColorButton / ColorPicker / Avatar / Icon / Breadcrumb / Chip / TagInput / Toast / Skeleton / StatusBar / TabBar / SplitView / ListView / TreeView / Table / TableView / RichText / TimePicker / CandidateListView / RecyclingListView / Menu / Tooltip / Popup / Dialog / MenuBar / NativeAlert / FileDialog / DatePickerPanel",
      [this](Layer& body, const Theme& theme) { buildMore(body, theme); } },
  };
}

void SectionedGallery::buildSidebar() {
  mNavList = std::make_shared<RecyclingListView>(
    [this](const std::string& key, std::size_t, const RecyclingRowState& state) {
      return rowElement(key, state);
    }, 30.0f);
  mNavList->setBackgroundColor(mTheme.surface);
  mNavList->setBounceEnabled(true);
  mNavList->setAutoRowHeight(false);
  mNavList->setRowHeightProvider([](const std::string& key, std::size_t) {
    return key.rfind("s:", 0) == 0 ? 32.0f : 26.0f;
  });
  mNavList->setOnSelectedChanged([this](const std::string& key) { onNavSelected(key); });
  mSidebar->addChild(mNavList);
  mScrollbar = std::make_shared<SidebarScrollbar>(mNavList);
  mSidebar->addChild(mScrollbar);
  rebuildNavRows({});
}

ElementPtr SectionedGallery::rowElement(const std::string& key,
    const RecyclingRowState&) {
  const GallerySelection selection = mModel->selection.get();
  for (const auto& row: mNavRows) {
    if (row.key != key) continue;
    std::string label;
    bool selected = false;
    float indent = 0.0f;
    if (row.header) {
      const bool expanded = mExpanded[row.section];
      label = std::string(expanded ? "[-]  " : "[+]  ") + row.label;
      selected = selection.section == static_cast<int>(row.section) && selection.control.empty();
    } else {
      label = row.label;
      selected = selection.section == static_cast<int>(row.section) &&
        selection.control == row.label;
      indent = 18.0f;
    }
    auto& layer = mRowLayers[key];
    if (layer == nullptr) {
      layer = std::make_shared<SidebarRow>(label, mTheme.body,
        row.header ? mTheme.controls.foreground : mTheme.textMuted,
        Color::MakeRGBA8(255, 255, 255, 255), selected, mTheme, indent);
    } else {
      layer->update(label, selected, row.header ? mTheme.controls.foreground : mTheme.textMuted,
        Color::MakeRGBA8(255, 255, 255, 255), indent);
    }
    return std::make_shared<LayerElement>(layer, key);
  }
  return nullptr;
}

void SectionedGallery::rebuildNavRows(const std::string& selectedKey) {
  mNavRows.clear();
  std::vector<std::string> keys;
  for (std::size_t section = 0; section < mSections.size(); ++section) {
    NavRow header;
    header.key = "s:" + std::to_string(section);
    header.label = mSections[section].title;
    header.section = section;
    header.header = true;
    mNavRows.push_back(header);
    keys.push_back(header.key);
    if (!mExpanded[section]) continue;
    std::vector<std::string> names;
    const std::string& controls = mSections[section].controls;
    std::size_t begin = 0;
    while (begin < controls.size()) {
      const std::size_t separator = controls.find(" / ", begin);
      names.push_back(controls.substr(begin, separator == std::string::npos
        ? std::string::npos : separator - begin));
      if (separator == std::string::npos) break;
      begin = separator + 3;
    }
    for (std::size_t item = 0; item < names.size(); ++item) {
      NavRow row;
      row.key = "i:" + std::to_string(section) + ":" + std::to_string(item);
      row.label = names[item];
      row.section = section;
      row.item = static_cast<int>(item);
      mNavRows.push_back(row);
      keys.push_back(row.key);
    }
  }
  mUpdatingNav = true;
  mNavList->setRows(std::move(keys));
  mNavList->setSelectedKey(selectedKey);
  mUpdatingNav = false;
}

void SectionedGallery::onNavSelected(const std::string& key) {
  if (mUpdatingNav) return;
  for (const auto& row: mNavRows) {
    if (row.key != key) continue;
    if (row.header) {
      mExpanded[row.section] = !mExpanded[row.section];
      const GallerySelection next{ static_cast<int>(row.section), {} };
      const GallerySelection previous = mModel->selection.get();
      mNeedReveal = true;
      mModel->selection.set(next);
      if (previous == next) queueSelection(next);
    } else {
      mModel->selection.set(GallerySelection{ static_cast<int>(row.section), row.label });
    }
    return;
  }
}

void SectionedGallery::queueSelection(const GallerySelection& selection) {
  mPendingSelection = selection;
  mNeedApplySelection = true;
  setNeedsLayout();
}

void SectionedGallery::applySelectionNow(const GallerySelection& selection) {
  if (selection.section < 0 || selection.section >= static_cast<int>(mSections.size())) return;
  const auto values = mNavList->rows();
  mUpdatingNav = true;
  mNavList->setRows(values);
  mNavList->setSelectedKey({});
  mUpdatingNav = false;
  mNeedReveal = true;
  showSection(static_cast<std::size_t>(selection.section), selection.control);
}

void SectionedGallery::showSection(const std::size_t index, std::string control) {
  if (index >= mSections.size()) return;
  const GallerySelection selection = mModel->selection.get();
  mPageTitle->setText(selection.control.empty()
    ? mSections[index].title
    : mSections[index].title + " · " + selection.control);
  mPageDescription->setText(mSections[index].description);
  mPageControls->setText(mSections[index].controls);
  mPageObservations.clear();
  const auto oldChildren = mBody->children();
  for (const auto& child: oldChildren) mBody->removeChild(child);
  if (selection.control.empty()) {
    mSections[index].build(*mBody, mTheme);
  } else {
    buildControl(*mBody, mTheme, selection.control);
  }
  mBody->setNeedsLayout();
  mNeedReveal = true;
  setNeedsLayout();
}

void SectionedGallery::buildMore(Layer& body, const Theme& theme) {
  addLabel(body, theme, "Additional framework controls", 20.0f, 8.0f);
  addLabel(body, theme, "Choose a control in the sidebar to open its standalone demo.", 20.0f, 36.0f,
    620.0f);
  addControl(body, std::make_shared<IconButton>(galleryIcon(), "Icon button"),
    Rect::MakeXYWH(20.0f, 78.0f, 36.0f, 36.0f));
  addControl(body, std::make_shared<SegmentedControl>(
    std::vector<std::string>{ "Grid", "List" }, theme.body),
    Rect::MakeXYWH(76.0f, 80.0f, 220.0f, 32.0f));
  addControl(body, std::make_shared<Skeleton>(220.0f, 16.0f),
    Rect::MakeXYWH(320.0f, 88.0f, 220.0f, 16.0f));
  addControl(body, std::make_shared<Breadcrumb>(
    std::vector<std::string>{ "Home", "Library", "Document" }, theme.body),
    Rect::MakeXYWH(20.0f, 140.0f, 420.0f, 28.0f));
  addControl(body, std::make_shared<StatusBar>(theme.body, "Ready", "gallery"),
    Rect::MakeXYWH(20.0f, 190.0f, 620.0f, 28.0f));
}

void SectionedGallery::buildControl(Layer& body, const Theme& theme,
    const std::string& control) {
  mModel->event.set("Interact with " + control);
  mModel->value.set("—");
  auto frame = std::make_shared<DemoFrame>(theme);
  frame->setBounds(Rect::MakeXYWH(kContentX + 20.0f, kBodyY + 20.0f, 680.0f, 360.0f));
  body.addChild(frame);
  auto eventLabel = std::make_shared<TextLayer>("", theme.body, theme.controls.foreground);
  eventLabel->setBounds(Rect::MakeXYWH(kContentX + 44.0f, kBodyY + 300.0f, 620.0f, 18.0f));
  body.addChild(eventLabel);
  mPageObservations.push_back(bindProperty(mModel->event, *eventLabel,
    [eventLabel](const std::string& value) { eventLabel->setText(value); }));
  auto valueLabel = std::make_shared<TextLayer>("", theme.body, theme.textMuted);
  valueLabel->setBounds(Rect::MakeXYWH(kContentX + 44.0f, kBodyY + 324.0f, 620.0f, 18.0f));
  body.addChild(valueLabel);
  mPageObservations.push_back(bindProperty(mModel->value, *valueLabel,
    [valueLabel](const std::string& value) { valueLabel->setText("Model: " + value); }));
  const auto notify = [this](const std::string& event, const std::string& value) {
    mModel->event.set(event);
    mModel->value.set(value);
  };

  auto text = [&](const std::string& value, float x, float y, Color color = {}) {
    auto label = std::make_shared<TextLayer>(value, theme.body,
      color == Color{} ? theme.controls.foreground : color);
    label->setBounds(Rect::MakeXYWH(kContentX + x, kBodyY + y, 600.0f, 20.0f));
    body.addChild(label);
  };
  auto add = [&](const std::shared_ptr<Layer>& layer, const Rect& rect) {
    addControl(body, layer, rect);
  };
  auto nativeFallback = [&](const std::string& name) {
    text(name + " is shown as a live NSView in a window.", 44.0f, 72.0f, theme.textMuted);
  };
  (void)nativeFallback;

  if (control == "Button") {
    auto button = std::make_shared<Button>("Primary", theme.body);
    button->setOnClick([this, notify] {
      const int next = mModel->clicks.get() + 1;
      mModel->clicks.set(next);
      notify("Button clicked", "clicks = " + std::to_string(next));
    });
    add(button, Rect::MakeXYWH(44, 72, 160, 40));
  } else if (control == "SplitButton") {
    auto split = std::make_shared<SplitButton>("Save", theme.body);
    split->setOnClick([this, notify] { notify("SplitButton primary", "save"); });
    split->setOnOption([this, notify](int index, std::string_view option) {
      notify("SplitButton option", std::to_string(index) + " · " + std::string(option));
    });
    add(split, Rect::MakeXYWH(44, 72, 200, 40));
  } else if (control == "SearchField") {
    auto search = std::make_shared<SearchField>("Search", theme.body);
    search->setOnChanged([this, notify](const std::string& text) {
      mModel->search.set(text);
      notify("Search changed", text);
    });
    add(search, Rect::MakeXYWH(44, 72, 360, 36));
  } else if (control == "TextInput") {
    auto input = std::make_shared<TextInput>(mModel->text.get(), theme.body);
    input->setMultiline(true);
    input->setPlaceholder("Type here; Enter adds a line");
    input->setOnSubmit([this, notify](const std::string& text) {
      mModel->text.set(text);
      notify("TextInput submitted", text);
    });
    add(input, Rect::MakeXYWH(44, 72, 420, 100));
  } else if (control == "PasswordField") {
    auto password = std::make_shared<PasswordField>("Password", theme.body);
    password->setOnChanged([this, notify](const std::string& text) {
      notify("Password changed", std::to_string(text.size()) + " characters");
    });
    add(password, Rect::MakeXYWH(44, 72, 320, 36));
  } else if (control == "Select") {
    auto select = std::make_shared<Select>(
      std::vector<std::string>{ "Alpha", "Beta", "Gamma" }, theme.body);
    select->setSelectedIndex(mModel->select.get());
    select->setOnChanged([this, notify](int index) {
      mModel->select.set(index);
      notify("Select changed", std::to_string(index));
    });
    add(select, Rect::MakeXYWH(44, 72, 180, 36));
  } else if (control == "ComboBox") {
    auto combo = std::make_shared<ComboBox>(
      std::vector<std::string>{ "One", "Two", "Three" }, theme.body);
    combo->setOnChanged([this, notify](int index, std::string_view text) {
      mModel->combo.set(index);
      notify("ComboBox changed", std::to_string(index) + " · " + std::string(text));
    });
    add(combo, Rect::MakeXYWH(44, 72, 240, 36));
  } else if (control == "MultiSelect") {
    auto multi = std::make_shared<MultiSelect>(
      std::vector<std::string>{ "Alpha", "Beta", "Gamma" }, theme.body);
    multi->setSelectedIndices(mModel->multi.get());
    multi->setOnChanged([this, notify](std::vector<int> indices) {
      mModel->multi.set(indices);
      std::string state;
      for (int index: indices) state += std::to_string(index) + " ";
      notify("MultiSelect changed", state);
    });
    add(multi, Rect::MakeXYWH(44, 72, 240, 36));
  } else if (control == "Toggle") {
    auto toggle = std::make_shared<Toggle>("Enabled", theme.body, mModel->toggle.get());
    toggle->setOnChanged([this, notify](bool on) {
      mModel->toggle.set(on);
      notify("Toggle changed", on ? "on" : "off");
    });
    add(toggle, Rect::MakeXYWH(44, 72, 180, 32));
  } else if (control == "CheckBox") {
    auto check = std::make_shared<CheckBox>("Remember", theme.body, mModel->checked.get());
    check->setOnChanged([this, notify](bool on) {
      mModel->checked.set(on);
      notify("CheckBox changed", on ? "checked" : "unchecked");
    });
    add(check, Rect::MakeXYWH(44, 72, 200, 32));
  } else if (control == "RadioGroup") {
    auto group = std::make_shared<RadioGroup>(
      std::vector<std::string>{ "Ascending", "Descending" }, theme.body, mModel->radio.get());
    group->setOnChanged([this, notify](int index) {
      mModel->radio.set(index);
      notify("RadioGroup changed", std::to_string(index));
    });
    add(group, Rect::MakeXYWH(44, 72, 240, 72));
  } else if (control == "CheckBoxGroup") {
    auto group = std::make_shared<CheckBoxGroup>(
      std::vector<std::string>{ "Photos", "Videos", "Docs" }, theme.body);
    group->setSelectedIndices(mModel->checks.get());
    group->setOnChanged([this, notify](std::vector<int> indices) {
      mModel->checks.set(indices);
      notify("CheckBoxGroup changed", std::to_string(indices.size()) + " selected");
    });
    add(group, Rect::MakeXYWH(44, 72, 240, 104));
  } else if (control == "ToggleGroup") {
    auto group = std::make_shared<ToggleGroup>(
      std::vector<std::string>{ "Bold", "Italic", "Underline" }, theme.body);
    group->setSelectedIndices(mModel->toggles.get());
    group->setOnChanged([this, notify](std::vector<int> indices) {
      mModel->toggles.set(indices);
      notify("ToggleGroup changed", std::to_string(indices.size()) + " selected");
    });
    add(group, Rect::MakeXYWH(44, 72, 300, 36));
  } else if (control == "Rating") {
    auto rating = std::make_shared<Rating>(5, 3);
    rating->setRating(mModel->rating.get());
    rating->setOnRatingChanged([this, notify](double value) {
      mModel->rating.set(value);
      notify("Rating changed", std::to_string(value));
    });
    add(rating, Rect::MakeXYWH(44, 72, 180, 32));
  } else if (control == "Slider") {
    auto slider = std::make_shared<Slider>("Volume", theme.body, 0.0, 100.0,
      mModel->slider.get());
    slider->setOnChanged([this, notify](double value) {
      mModel->slider.set(value);
      notify("Slider changed", std::to_string(static_cast<int>(value)));
    });
    add(slider, Rect::MakeXYWH(44, 72, 360, 34));
  } else if (control == "RangeSlider") {
    auto range = std::make_shared<RangeSlider>(0.0, 100.0);
    range->setLow(mModel->rangeLow.get());
    range->setHigh(mModel->rangeHigh.get());
    range->setOnChanged([this, notify](double low, double high) {
      mModel->rangeLow.set(low);
      mModel->rangeHigh.set(high);
      notify("RangeSlider changed", std::to_string(low) + " – " + std::to_string(high));
    });
    add(range, Rect::MakeXYWH(44, 72, 360, 28));
  } else if (control == "Stepper") {
    auto stepper = std::make_shared<Stepper>(0.0, 10.0, mModel->stepper.get());
    stepper->setOnChanged([this, notify](double value) {
      mModel->stepper.set(value);
      notify("Stepper changed", std::to_string(value));
    });
    add(stepper, Rect::MakeXYWH(44, 72, 130, 30));
  } else if (control == "DateField") {
    auto field = std::make_shared<DateField>(theme.body, std::chrono::year_month_day{
      std::chrono::year{2026}, std::chrono::November, std::chrono::day{24} });
    field->setOnChanged([this, field, notify](std::chrono::year_month_day) {
      notify("DateField changed", field->text());
    });
    add(field, Rect::MakeXYWH(44, 72, 220, 38));
  } else if (control == "TimeField") {
    auto field = std::make_shared<TimeField>(theme.body, 13, 45);
    field->setOnChanged([this, field, notify](int, int) {
      notify("TimeField changed", field->text());
    });
    add(field, Rect::MakeXYWH(44, 72, 180, 38));
  } else if (control == "DateTimeField") {
    auto field = std::make_shared<DateTimeField>(theme.body, std::chrono::year_month_day{
      std::chrono::year{2026}, std::chrono::November, std::chrono::day{24} }, 13, 45);
    field->setOnChanged([this, field, notify](std::chrono::year_month_day, int, int) {
      notify("DateTimeField changed", std::to_string(field->hour()) + ":" +
        std::to_string(field->minute()));
    });
    add(field, Rect::MakeXYWH(44, 72, 320, 38));
  } else if (control == "DateRangeField") {
    auto field = std::make_shared<DateRangeField>(theme.body, std::chrono::year_month_day{
      std::chrono::year{2026}, std::chrono::November, std::chrono::day{24} },
      std::chrono::year_month_day{ std::chrono::year{2026}, std::chrono::November,
        std::chrono::day{30} });
    field->setOnChanged([this, notify](std::chrono::year_month_day start,
        std::chrono::year_month_day end) {
      notify("DateRangeField changed", "start/end updated");
      (void)start;
      (void)end;
    });
    add(field, Rect::MakeXYWH(44, 72, 360, 38));
  } else if (control == "TimeRangeField") {
    auto field = std::make_shared<TimeRangeField>(theme.body, 9, 0, 17, 0);
    field->setOnChanged([this, notify](int, int, int, int) {
      notify("TimeRangeField changed", "start/end updated");
    });
    add(field, Rect::MakeXYWH(44, 72, 320, 38));
  } else if (control == "Calendar") {
    add(std::make_shared<Calendar>(std::chrono::year_month_day{
      std::chrono::year{2026}, std::chrono::November, std::chrono::day{24} }, theme.body),
      Rect::MakeXYWH(44, 56, 300, 220));
  } else if (control == "ProgressBar") {
    add(std::make_shared<ProgressBar>(theme.body, 0.62), Rect::MakeXYWH(44, 72, 360, 20));
  } else if (control == "ProgressRing") {
    add(std::make_shared<ProgressRing>(56.0f, 0.72), Rect::MakeXYWH(44, 72, 56, 56));
  } else if (control == "Meter") {
    add(std::make_shared<Meter>(0.0, 1.0, 0.55), Rect::MakeXYWH(44, 72, 360, 20));
  } else if (control == "Spinner") {
    add(std::make_shared<Spinner>(), Rect::MakeXYWH(44, 72, 24, 24));
  } else if (control == "Banner") {
    auto banner = std::make_shared<Banner>("Saved successfully", theme.body,
      BannerSeverity::Success);
    banner->setIcon(galleryIcon());
    banner->setAction("Undo", [notify] { notify("Banner action", "Undo"); });
    banner->setOnDismiss([notify] { notify("Banner dismissed", "hidden"); });
    add(banner, Rect::MakeXYWH(44, 72, 560, 40));
  } else if (control == "ToastStack") {
    auto toasts = std::make_shared<ToastStack>(theme.body, 2);
    toasts->show("Saved successfully", std::chrono::seconds(30));
    add(toasts, Rect::MakeXYWH(44, 72, 420, 36));
  } else if (control == "EmptyState") {
    auto empty = std::make_shared<EmptyState>(theme.body, "No projects",
      "Create your first project");
    empty->setIcon(galleryIcon());
    auto create = std::make_shared<Button>("Create", theme.body);
    create->setOnClick([notify] { notify("EmptyState action", "Create"); });
    empty->setAction(create);
    add(empty, Rect::MakeXYWH(44, 56, 280, 190));
  } else if (control == "StatusIndicator") {
    add(std::make_shared<StatusIndicator>("Online", theme.body, StatusKind::Online),
      Rect::MakeXYWH(44, 72, 140, 22));
  } else if (control == "Pagination") {
    auto pagination = std::make_shared<Pagination>(theme.body, 10, mModel->page.get());
    pagination->setOnChanged([this, notify](int page) {
      mModel->page.set(page);
      notify("Pagination changed", std::to_string(page));
    });
    add(pagination, Rect::MakeXYWH(44, 72, 300, 32));
  } else if (control == "Card") {
    auto empty = std::make_shared<EmptyState>(theme.body, "No projects",
      "Create your first project");
    empty->setIcon(galleryIcon());
    auto card = std::make_shared<Card>(theme.body, "Card surface", empty);
    auto edit = std::make_shared<Button>("Edit", theme.body);
    edit->setOnClick([notify] { notify("Card action", "Edit"); });
    card->setHeaderAction(edit);
    add(card, Rect::MakeXYWH(44, 56, 380, 230));
  } else if (control == "AvatarGroup") {
    add(std::make_shared<AvatarGroup>(std::vector<std::string>{ "AL", "MK", "TS" },
      theme.body, 2, 32.0f), Rect::MakeXYWH(44, 72, 220, 32));
  } else if (control == "Badge") {
    add(std::make_shared<Badge>("12", theme.body), Rect::MakeXYWH(44, 72, 52, 24));
  } else if (control == "TagList") {
    add(std::make_shared<TagList>(std::vector<std::string>{ "design", "ui", "macOS",
      "release" }, theme.body, 3), Rect::MakeXYWH(44, 72, 300, 28));
  } else if (control == "Link") {
    auto link = std::make_shared<Link>("Documentation", theme.body);
    link->setOnActivate([notify] { notify("Link activated", "Documentation"); });
    add(link, Rect::MakeXYWH(44, 72, 180, 22));
  } else if (control == "Kbd") {
    add(std::make_shared<Kbd>("⌘K", theme.body), Rect::MakeXYWH(44, 72, 44, 24));
  } else if (control == "Stat") {
    add(std::make_shared<Stat>("Users", "1,240", theme.body, "+12%", StatTrend::Up),
      Rect::MakeXYWH(44, 72, 240, 72));
  } else if (control == "Timeline") {
    add(std::make_shared<Timeline>(std::vector<TimelineEntry>{
      { "Created", "Project created", "09:00", TimelineState::Complete },
      { "Review", "Waiting for review", "10:30", TimelineState::Current },
      { "Ship", "Not started", "Later", TimelineState::Pending },
    }, theme.body), Rect::MakeXYWH(44, 56, 320, 140));
  } else if (control == "PropertyRow") {
    add(std::make_shared<PropertyRow>("Version", "1.4.2", theme.body),
      Rect::MakeXYWH(44, 72, 360, 20));
  } else if (control == "SectionHeader") {
    add(std::make_shared<SectionHeader>("SectionHeader", theme.body, "Section subtitle",
      std::make_shared<Button>("Action", theme.body)), Rect::MakeXYWH(44, 56, 560, 40));
  } else if (control == "FormField") {
    auto field = std::make_shared<FormField>("Email",
      std::make_shared<TextInput>("", theme.body), theme.body);
    field->setHelpText("We never share it");
    add(field, Rect::MakeXYWH(44, 56, 360, 78));
  } else if (control == "Expander") {
    auto toolbar = std::make_shared<Toolbar>(theme.body);
    toolbar->addItem(std::make_shared<Button>("One", theme.body));
    toolbar->addSeparator();
    toolbar->addItem(std::make_shared<Button>("Two", theme.body));
    auto expander = std::make_shared<Expander>("Details", theme.body, toolbar);
    expander->setOnChanged([this, notify](bool expanded) {
      mModel->expanded.set(expanded);
      notify("Expander changed", expanded ? "expanded" : "collapsed");
    });
    add(expander, Rect::MakeXYWH(44, 56, 420, 150));
  } else if (control == "Toolbar") {
    auto toolbar = std::make_shared<Toolbar>(theme.body);
    toolbar->addItem(std::make_shared<Button>("One", theme.body));
    toolbar->addSeparator();
    toolbar->addItem(std::make_shared<Button>("Two", theme.body));
    add(toolbar, Rect::MakeXYWH(44, 72, 420, 40));
  } else if (control == "Divider") {
    add(std::make_shared<Divider>(Divider::Orientation::Horizontal),
      Rect::MakeXYWH(44, 72, 500, 1));
  } else if (control == "DropZone") {
    auto drop = std::make_shared<DropZone>("Drop files here", theme.body);
    drop->setOnFilesDropped([this, notify](std::vector<std::string> paths) {
      mModel->dropped.set(paths.empty() ? std::string() : paths.front());
      notify("Files dropped", std::to_string(paths.size()) + " file(s)");
    });
    drop->setOnTextDropped([this, notify](std::string text) {
      mModel->dropped.set(text);
      notify("Text dropped", text);
    });
    add(drop, Rect::MakeXYWH(44, 72, 300, 90));
  } else if (control == "StepIndicator") {
    auto steps = std::make_shared<StepIndicator>(
      std::vector<std::string>{ "Account", "Profile", "Done" }, theme.body,
      mModel->currentStep.get());
    steps->setOnChanged([this, notify](int index) {
      mModel->currentStep.set(index);
      notify("StepIndicator changed", std::to_string(index));
    });
    add(steps, Rect::MakeXYWH(44, 72, 420, 48));
  } else if (control == "ScrollView" || control == "List rows") {
    auto list = std::make_shared<ScrollView>();
    list->setBackgroundColor(theme.surfaceRaised);
    list->setContent(makeListContent(theme, 18, "Row", 360.0f));
    add(list, Rect::MakeXYWH(44, 56, 400, 260));
  } else if (control == "Nested ScrollView") {
    auto outer = std::make_shared<ScrollView>();
    outer->setBackgroundColor(theme.surfaceRaised);
    auto content = std::make_shared<Flex>(Axis::Column);
    content->setGap(8.0f);
    content->setPadding(Insets{}.all(12.0f));
    content->setCrossAlign(Align::Start);
    auto inner = std::make_shared<ScrollView>();
    inner->setBackgroundColor(theme.background);
    inner->setContent(makeListContent(theme, 14, "Inner row", 320.0f));
    inner->setBounds(Rect::MakeWH(340.0f, 200.0f));
    inner->setPreferredHeight(200.0f);
    content->addChild(inner);
    outer->setContent(content);
    add(outer, Rect::MakeXYWH(44, 56, 420, 260));
  } else if (control == "RecyclingListView") {
    auto list = std::make_shared<RecyclingListView>(
      [theme](const std::string& key, std::size_t, const RecyclingRowState& state) {
        return std::make_shared<TextElement>(key, theme.body,
          state.selected ? theme.accent : theme.text, key);
      }, 30.0f);
    list->setRows({ "row-1", "row-2", "row-3", "row-4", "row-5", "row-6", "row-7", "row-8" });
    list->setSelectedKey("row-2");
    list->setOnSelectedChanged([notify](const std::string& key) {
      notify("RecyclingListView selected", key);
    });
    add(list, Rect::MakeXYWH(44, 56, 360, 220));
  } else if (control == "Menu") {
    auto button = std::make_shared<Button>("Open menu", theme.body);
    auto menu = std::make_shared<Menu>(theme.body);
    menu->addItem("First", [notify] { notify("Menu item", "First"); });
    menu->addItem("Second", [notify] { notify("Menu item", "Second"); });
    menu->addSeparator();
    menu->addItem("Disabled", [] {}, false);
    button->setOnClick([menu, &body] {
      if (body.window() == nullptr) return;
      if (menu->parent() != body.window()->root().get()) body.window()->root()->addChild(menu);
      menu->showAt(Rect::MakeXYWH(kContentX + 44.0f, kBodyY + 116.0f, 160.0f, 32.0f));
    });
    add(button, Rect::MakeXYWH(44, 72, 160, 32));
  } else if (control == "NativeAlert") {
    auto button = std::make_shared<Button>("Show native alert", theme.body);
    button->setOnClick([notify, &body] {
      NativeAlertOptions options;
      options.title = "CoralUI";
      options.message = "This is a native platform alert.";
      options.buttons = { "Cancel", "OK" };
      options.defaultButton = 1;
      const int result = showNativeAlert(options, body.window());
      notify("NativeAlert result", std::to_string(result));
    });
    add(button, Rect::MakeXYWH(44, 72, 200, 32));
  } else if (control == "FileDialog") {
    auto button = std::make_shared<Button>("Open file", theme.body);
    button->setOnClick([notify, &body] {
      FileDialogOptions options;
      options.title = "Choose a file";
      // Run the validation panel app-modally. A sheet attached from a custom Cocoa event loop can
      // leave its OK button unresponsive on some macOS versions; the result path is what this demo
      // is exercising, not the sheet presentation.
      options.parent = nullptr;
      const FileDialogResult result = showFileDialog(FileDialogKind::OpenFile, options);
      notify("FileDialog result", result.accepted ? result.paths.front() : "cancelled");
    });
    add(button, Rect::MakeXYWH(44, 72, 160, 32));
  } else if (control == "DatePickerPanel") {
    auto button = std::make_shared<Button>("Open date picker", theme.body);
    button->setOnClick([notify] {
      const bool shown = showNativeDatePicker(
        std::chrono::year_month_day{ std::chrono::year{2026}, std::chrono::November,
          std::chrono::day{24} },
        [notify](std::chrono::year_month_day date) {
          notify("DatePickerPanel selected", formatDate(date, dateFormatIso()));
        });
      if (!shown) notify("DatePickerPanel unavailable", "platform fallback");
    });
    add(button, Rect::MakeXYWH(44, 72, 200, 32));
  } else if (control == "IconButton") {
    auto button = std::make_shared<IconButton>(galleryIcon(), "Icon button");
    button->setOnClick([notify] { notify("IconButton clicked", "icon"); });
    add(button, Rect::MakeXYWH(44, 72, 36, 36));
  } else if (control == "ToggleButton") {
    auto button = std::make_shared<ToggleButton>("Bold", theme.body, mModel->toggleButton.get());
    button->setOnChanged([this, notify](bool selected) {
      mModel->toggleButton.set(selected);
      notify("ToggleButton changed", selected ? "selected" : "unselected");
    });
    add(button, Rect::MakeXYWH(44, 72, 160, 36));
  } else if (control == "NumberField") {
    auto number = std::make_shared<NumberField>(theme.body, 0.0, 100.0, mModel->number.get());
    number->setOnChanged([this, notify](double value) {
      mModel->number.set(value);
      notify("NumberField changed", std::to_string(value));
    });
    add(number, Rect::MakeXYWH(44, 72, 180, 36));
  } else if (control == "SegmentedControl") {
    auto segmented = std::make_shared<SegmentedControl>(
      std::vector<std::string>{ "Grid", "List", "Details" }, theme.body);
    segmented->setSelectedIndex(mModel->segmented.get());
    segmented->setOnChanged([this, notify](int index) {
      mModel->segmented.set(index);
      notify("SegmentedControl changed", std::to_string(index));
    });
    add(segmented, Rect::MakeXYWH(44, 72, 300, 32));
  } else if (control == "ColorInput") {
    auto input = std::make_shared<ColorInput>(theme.body,
      Color::MakeRGBA8(86, 156, 255, 255));
    input->setOnChanged([notify](Color color) {
      notify("ColorInput changed", std::to_string(static_cast<int>(color.red * 255)) + ", " +
        std::to_string(static_cast<int>(color.green * 255)) + ", " +
        std::to_string(static_cast<int>(color.blue * 255)));
    });
    add(input, Rect::MakeXYWH(44, 56, 300, 116));
  } else if (control == "ColorButton") {
    auto button = std::make_shared<ColorButton>(Color::MakeRGBA8(86, 156, 255, 255), "Color");
    button->setPickerEnabled(true);
    button->setOnPick([notify](Color color) {
      notify("ColorButton picked", std::to_string(static_cast<int>(color.red * 255)) + ", " +
        std::to_string(static_cast<int>(color.green * 255)) + ", " +
        std::to_string(static_cast<int>(color.blue * 255)));
    });
    add(button, Rect::MakeXYWH(44, 72, 36, 36));
  } else if (control == "ColorPicker") {
    auto picker = std::make_shared<ColorPicker>();
    picker->setOnChanged([notify](Color color) {
      notify("ColorPicker changed", std::to_string(static_cast<int>(color.red * 255)) + ", " +
        std::to_string(static_cast<int>(color.green * 255)) + ", " +
        std::to_string(static_cast<int>(color.blue * 255)));
    });
    add(picker, Rect::MakeXYWH(44, 56, 260, 100));
  } else if (control == "Chip") {
    auto chip = std::make_shared<Chip>("design", theme.body);
    chip->setVariant(ChipVariant::Accent);
    chip->setOnRemove([notify] { notify("Chip removed", "design"); });
    add(chip, Rect::MakeXYWH(44, 72, 112, 28));
  } else if (control == "TagInput") {
    auto tags = std::make_shared<TagInput>(theme.body, "Add tag");
    tags->setTokens({ "design", "ui" });
    tags->setOnChanged([notify](const std::vector<std::string>& tokens) {
      notify("Tags changed", std::to_string(tokens.size()));
    });
    add(tags, Rect::MakeXYWH(44, 56, 360, 44));
  } else if (control == "TimePicker") {
    auto picker = std::make_shared<TimePicker>(theme.body, 9, 30, true);
    picker->setOnChanged([notify](int hour, int minute) {
      char text[16] = {};
      std::snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
      notify("TimePicker changed", text);
    });
    add(picker, Rect::MakeXYWH(44, 56, 300, 220));
  } else if (control == "TableView") {
    auto table = std::make_shared<TableView>();
    table->setColumns({ TableColumn{ 160.0f, 0.0f, 0.0f },
      TableColumn{ 0.0f, 1.0f, 0.0f }, TableColumn{ 100.0f, 0.0f, 0.0f } });
    std::vector<std::vector<std::shared_ptr<Layer>>> rows;
    for (const auto& [name, state, size]: std::vector<std::tuple<std::string, std::string, std::string>>{
           { "document.txt", "Ready", "12 KB" }, { "photo.png", "Uploading", "2.4 MB" },
           { "archive.zip", "Queued", "18 MB" }, { "notes.md", "Ready", "4 KB" } }) {
      rows.push_back({ std::make_shared<TextLayer>(name, theme.body),
        std::make_shared<TextLayer>(state, theme.body),
        std::make_shared<TextLayer>(size, theme.body) });
    }
    table->setRows(std::move(rows));
    table->setOnRowActivated([notify](int row) {
      notify("TableView activated", std::to_string(row));
    });
    add(table, Rect::MakeXYWH(44, 56, 420, 240));
  } else if (control == "RichText") {
    std::vector<RichTextRun> runs;
    runs.push_back(RichTextRun{ "CoralUI ", theme.body, nullptr, theme.accent, false, {} });
    runs.push_back(RichTextRun{ "rich text ", theme.body, nullptr, theme.text, true, {} });
    runs.push_back(RichTextRun{ "renders multiple styles, wrapping and selection.",
      theme.body, nullptr, theme.textMuted, false, {} });
    auto rich = std::make_shared<RichTextLayer>(std::move(runs));
    rich->setWrapWidth(480.0f);
    rich->setSelectable(true);
    add(rich, Rect::MakeXYWH(44, 56, 520, 120));
  } else if (control == "CandidateListView") {
    auto candidates = std::make_shared<CandidateListView>(theme.body);
    candidates->setCandidates({ "co", "coral", "core", "color" }, 1);
    add(candidates, Rect::MakeXYWH(44, 56, 260, 132));
  } else if (control == "Avatar") {
    add(std::make_shared<Avatar>("AL", theme.body, 40.0f), Rect::MakeXYWH(44, 72, 40, 40));
  } else if (control == "Icon") {
    add(std::make_shared<Icon>(galleryIcon(), 28.0f), Rect::MakeXYWH(44, 72, 28, 28));
  } else if (control == "Breadcrumb") {
    auto crumb = std::make_shared<Breadcrumb>(
      std::vector<std::string>{ "Home", "Library", "Document" }, theme.body);
    crumb->setOnSelect([notify](int index, std::string_view text) {
      notify("Breadcrumb selected", std::to_string(index) + " · " + std::string(text));
    });
    add(crumb, Rect::MakeXYWH(44, 72, 420, 28));
  } else if (control == "Toast") {
    auto toast = std::make_shared<Toast>("Saved successfully", theme.body);
    toast->show("Saved successfully");
    add(toast, Rect::MakeXYWH(44, 72, 360, 36));
  } else if (control == "Tooltip") {
    auto tip = std::make_shared<Tooltip>("Helpful hint", theme.body);
    tip->showAt(Point{ kContentX + 44.0f, kBodyY + 72.0f });
    body.addChild(tip);
    notify("Tooltip shown", "hover the control");
  } else if (control == "Popup") {
    auto popup = std::make_shared<Popup>(std::make_shared<TextLayer>(
      "Popup content", theme.body));
    popup->showAt(Rect::MakeXYWH(kContentX + 44.0f, kBodyY + 72.0f, 220.0f, 80.0f));
    body.addChild(popup);
    notify("Popup shown", "floating panel");
  } else if (control == "Dialog") {
    auto dialog = std::make_shared<Dialog>("Demo dialog", theme.body);
    dialog->setMessage("A modal dialog with a default action.");
    dialog->addButton("Cancel", [notify] { notify("Dialog result", "cancel"); });
    dialog->addButton("OK", [notify] { notify("Dialog result", "ok"); });
    dialog->setDefaultButton(1);
    add(dialog, Rect::MakeXYWH(20, 20, 680, 360));
    dialog->show();
    notify("Dialog shown", "modal");
  } else if (control == "Skeleton") {
    add(std::make_shared<Skeleton>(320.0f, 18.0f), Rect::MakeXYWH(44, 72, 320, 18));
  } else if (control == "StatusBar") {
    add(std::make_shared<StatusBar>(theme.body, "Ready", "gallery"),
      Rect::MakeXYWH(44, 72, 560, 28));
  } else if (control == "TabBar") {
    auto tabs = std::make_shared<TabBar>(theme.body);
    tabs->addTab("One", std::make_shared<TextLayer>("First tab", theme.body));
    tabs->addTab("Two", std::make_shared<TextLayer>("Second tab", theme.body));
    tabs->addTab("Three", std::make_shared<TextLayer>("Third tab", theme.body));
    tabs->setActiveTab(0);
    tabs->setOnChanged([notify](int index) {
      notify("TabBar changed", std::to_string(index));
    });
    add(tabs, Rect::MakeXYWH(44, 56, 420, 180));
  } else if (control == "SplitView") {
    auto split = std::make_shared<SplitView>(Axis::Row, 0.5f);
    split->setPanes(std::make_shared<ColorLayer>(theme.surfaceRaised),
      std::make_shared<ColorLayer>(theme.background));
    split->setOnFractionChanged([notify](float fraction) {
      notify("SplitView changed", std::to_string(fraction));
    });
    add(split, Rect::MakeXYWH(44, 56, 420, 180));
  } else if (control == "ListView") {
    auto list = std::make_shared<ListView>(theme.body, 30.0f);
    list->setItems({ "Alpha", "Beta", "Gamma", "Delta", "Epsilon" });
    list->setSelectedIndex(1);
    list->setOnSelection([notify](int index, const std::string& text) {
      notify("ListView selected", std::to_string(index) + " · " + text);
    });
    add(list, Rect::MakeXYWH(44, 56, 320, 200));
  } else if (control == "TreeView") {
    auto tree = std::make_shared<TreeView>(theme.body, 28.0f);
    const int root = tree->addRoot("Project", true);
    tree->addChild(root, "src", true);
    tree->addChild(root, "README.md", false);
    tree->setOnSelection([notify](int index, const std::string& text) {
      notify("TreeView selected", std::to_string(index) + " · " + text);
    });
    add(tree, Rect::MakeXYWH(44, 56, 320, 200));
  } else if (control == "MenuBar") {
    auto menu = std::make_shared<Menu>(theme.body);
    menu->addItem("Item one", [notify] { notify("Menu item", "one"); });
    menu->addItem("Item two", [notify] { notify("Menu item", "two"); });
    auto bar = std::make_shared<MenuBar>(theme.body);
    bar->addMenu("File", menu);
    add(bar, Rect::MakeXYWH(44, 72, 400, 28));
  } else if (control == "Table") {
    auto table = std::make_shared<Table>(std::vector<TableColumn>{ TableColumn{}, TableColumn{} });
    std::vector<std::vector<std::shared_ptr<Layer>>> rows;
    for (int row = 0; row < 3; ++row) {
      rows.push_back({ std::make_shared<TextLayer>("Row " + std::to_string(row + 1), theme.body),
        std::make_shared<TextLayer>("Value " + std::to_string(row + 1), theme.body) });
    }
    table->setRows(std::move(rows));
    table->setSelectedRow(1);
    add(table, Rect::MakeXYWH(44, 56, 500, 200));
  } else if (control == "NativeButton" || control == "NativeSwitch" ||
             control == "NativeSearchField" || control == "NativeStepper" ||
             control == "NativeProgressIndicator" || control == "NativePopUpButton") {
#if defined(__APPLE__)
    const auto configure = [this](const auto& native) {
      if (native == nullptr) return;
      native->setControlSize(macos::NativeControlSize::Small);
      if (mNativeDisabled) native->setEnabled(false);
    };
    if (control == "NativeButton") {
      auto native = macos::NativeButton::create("Native",
        [notify] { notify("NativeButton clicked", "native"); });
      configure(native);
      if (native != nullptr) add(native, Rect::MakeXYWH(44, 72, 120, 28));
    } else if (control == "NativeSwitch") {
      auto native = macos::NativeSwitch::create("On", true,
        [notify](bool on) { notify("NativeSwitch changed", on ? "on" : "off"); });
      configure(native);
      if (native != nullptr) add(native, Rect::MakeXYWH(44, 72, 100, 28));
    } else if (control == "NativeSearchField") {
      auto native = macos::NativeSearchField::create("Search",
        [notify](std::string_view text) { notify("NativeSearchField changed", std::string(text)); });
      configure(native);
      if (native != nullptr) add(native, Rect::MakeXYWH(44, 72, 220, 28));
    } else if (control == "NativeStepper") {
      auto native = macos::NativeStepper::create(0.0, 10.0, 4.0,
        [notify](double value) { notify("NativeStepper changed", std::to_string(value)); });
      configure(native);
      if (native != nullptr) add(native, Rect::MakeXYWH(44, 72, 90, 28));
    } else if (control == "NativeProgressIndicator") {
      auto native = macos::NativeProgressIndicator::create(false);
      configure(native);
      if (native != nullptr) {
        native->setValue(0.65);
        add(native, Rect::MakeXYWH(44, 72, 160, 20));
      }
    } else if (control == "NativePopUpButton") {
      auto native = macos::NativePopUpButton::create({ "One", "Two" }, 0,
        [notify](int index, std::string_view text) {
          notify("NativePopUpButton changed", std::to_string(index) + " · " + std::string(text));
        });
      configure(native);
      if (native != nullptr) add(native, Rect::MakeXYWH(44, 72, 120, 28));
    }
    text("Native controls are live NSViews; headless snapshots show their frame only.",
      44.0f, 120.0f, theme.textMuted);
#else
    nativeFallback(control);
#endif
  } else {
    text(control + " has no standalone demo yet.", 44.0f, 72.0f, theme.textMuted);
  }
}

void SectionedGallery::buildInputs(Layer& body, const Theme& theme) {
  addLabel(body, theme, "Buttons", 20.0f, 8.0f);
  addControl(body, std::make_shared<Button>("Primary", theme.body),
    Rect::MakeXYWH(20.0f, 32.0f, 120.0f, 36.0f));
  addControl(body, std::make_shared<SplitButton>("Save", theme.body),
    Rect::MakeXYWH(152.0f, 32.0f, 150.0f, 36.0f));

  addLabel(body, theme, "Search / Select / Multi", 20.0f, 84.0f);
  addControl(body, std::make_shared<SearchField>("Search", theme.body),
    Rect::MakeXYWH(20.0f, 108.0f, 260.0f, 32.0f));
  addControl(body, std::make_shared<Select>(
    std::vector<std::string>{ "Alpha", "Beta", "Gamma" }, theme.body),
    Rect::MakeXYWH(292.0f, 108.0f, 150.0f, 32.0f));
  auto multi = std::make_shared<MultiSelect>(
    std::vector<std::string>{ "Alpha", "Beta", "Gamma" }, theme.body);
  multi->setSelectedIndices({ 0, 2 });
  addControl(body, multi, Rect::MakeXYWH(454.0f, 108.0f, 170.0f, 32.0f));

  addLabel(body, theme, "Text fields", 20.0f, 156.0f);
  auto input = std::make_shared<TextInput>("", theme.body);
  input->setPlaceholder("Text input");
  input->setBounds(Rect::MakeXYWH(kContentX + 20.0f, kBodyY + 180.0f, 300.0f, 32.0f));
  body.addChild(input);
  mInputField = input;
  addControl(body, std::make_shared<PasswordField>("Password", theme.body),
    Rect::MakeXYWH(334.0f, 180.0f, 290.0f, 32.0f));
  addLabel(body, theme, "ComboBox", 20.0f, 228.0f);
  addControl(body, std::make_shared<ComboBox>(
    std::vector<std::string>{ "One", "Two", "Three" }, theme.body),
    Rect::MakeXYWH(20.0f, 252.0f, 200.0f, 32.0f));
  addLabel(body, theme, "Multiline", 240.0f, 228.0f);
  auto multiline = std::make_shared<TextInput>("", theme.body);
  multiline->setMultiline(true);
  multiline->setPlaceholder("Enter adds a line");
  addControl(body, multiline, Rect::MakeXYWH(240.0f, 252.0f, 384.0f, 72.0f));
}

void SectionedGallery::buildSelection(Layer& body, const Theme& theme) {
  addLabel(body, theme, "Boolean", 20.0f, 8.0f);
  addControl(body, std::make_shared<Toggle>("Enabled", theme.body, true),
    Rect::MakeXYWH(20.0f, 32.0f, 160.0f, 28.0f));
  addControl(body, std::make_shared<CheckBox>("Remember", theme.body, true),
    Rect::MakeXYWH(194.0f, 32.0f, 160.0f, 28.0f));
  addControl(body, std::make_shared<Chip>("Filter", theme.body),
    Rect::MakeXYWH(366.0f, 32.0f, 90.0f, 28.0f));

  addLabel(body, theme, "RadioGroup", 20.0f, 82.0f);
  addControl(body, std::make_shared<RadioGroup>(
    std::vector<std::string>{ "Ascending", "Descending" }, theme.body, 0),
    Rect::MakeXYWH(20.0f, 106.0f, 220.0f, 64.0f));
  addLabel(body, theme, "CheckBoxGroup", 280.0f, 82.0f);
  auto checks = std::make_shared<CheckBoxGroup>(
    std::vector<std::string>{ "Photos", "Videos", "Docs" }, theme.body);
  checks->setSelectedIndices({ 0, 2 });
  addControl(body, checks, Rect::MakeXYWH(280.0f, 106.0f, 220.0f, 96.0f));

  addLabel(body, theme, "ToggleGroup", 20.0f, 208.0f);
  auto toggles = std::make_shared<ToggleGroup>(
    std::vector<std::string>{ "Bold", "Italic", "Underline" }, theme.body);
  toggles->setSelectedIndices({ 0 });
  addControl(body, toggles, Rect::MakeXYWH(20.0f, 232.0f, 280.0f, 36.0f));
  addLabel(body, theme, "Rating", 320.0f, 208.0f);
  auto rating = std::make_shared<Rating>(5, 3);
  rating->setRating(3.5);
  addControl(body, rating, Rect::MakeXYWH(320.0f, 232.0f, 160.0f, 32.0f));

  addLabel(body, theme, "Slider", 20.0f, 284.0f);
  addControl(body, std::make_shared<Slider>("Volume", theme.body, 0.0, 100.0, 60.0),
    Rect::MakeXYWH(20.0f, 308.0f, 300.0f, 32.0f));
  addLabel(body, theme, "RangeSlider", 20.0f, 350.0f);
  auto range = std::make_shared<RangeSlider>(0.0, 100.0);
  range->setLow(20.0);
  range->setHigh(80.0);
  addControl(body, range, Rect::MakeXYWH(20.0f, 374.0f, 300.0f, 28.0f));
  addLabel(body, theme, "Stepper", 340.0f, 350.0f);
  addControl(body, std::make_shared<Stepper>(0.0, 10.0, 4.0),
    Rect::MakeXYWH(340.0f, 374.0f, 120.0f, 28.0f));
}

void SectionedGallery::buildDateTime(Layer& body, const Theme& theme) {
  using namespace std::chrono;
  const year_month_day selectedDay{ year{2026}, November, day{24} };
  addLabel(body, theme, "DateField / TimeField", 20.0f, 8.0f);
  addControl(body, std::make_shared<DateField>(theme.body, selectedDay),
    Rect::MakeXYWH(20.0f, 32.0f, 200.0f, 36.0f));
  addControl(body, std::make_shared<TimeField>(theme.body, 13, 45),
    Rect::MakeXYWH(236.0f, 32.0f, 160.0f, 36.0f));

  addLabel(body, theme, "DateTimeField", 20.0f, 84.0f);
  addControl(body, std::make_shared<DateTimeField>(theme.body, selectedDay, 13, 45),
    Rect::MakeXYWH(20.0f, 108.0f, 300.0f, 36.0f));
  addLabel(body, theme, "Calendar", 360.0f, 84.0f);
  addControl(body, std::make_shared<Calendar>(selectedDay, theme.body),
    Rect::MakeXYWH(360.0f, 108.0f, 300.0f, 220.0f));

  addLabel(body, theme, "DateRangeField", 20.0f, 160.0f);
  addControl(body, std::make_shared<DateRangeField>(theme.body, selectedDay,
    year_month_day{ year{2026}, November, day{30} }),
    Rect::MakeXYWH(20.0f, 184.0f, 300.0f, 36.0f));
  addLabel(body, theme, "TimeRangeField", 20.0f, 236.0f);
  addControl(body, std::make_shared<TimeRangeField>(theme.body, 9, 0, 17, 0),
    Rect::MakeXYWH(20.0f, 260.0f, 300.0f, 36.0f));
}

void SectionedGallery::buildFeedback(Layer& body, const Theme& theme) {
  addLabel(body, theme, "Progress", 20.0f, 8.0f);
  auto progress = std::make_shared<ProgressBar>(theme.body, 0.62);
  addControl(body, progress, Rect::MakeXYWH(20.0f, 32.0f, 300.0f, 20.0f));
  auto ring = std::make_shared<ProgressRing>(40.0f, 0.72);
  addControl(body, ring, Rect::MakeXYWH(340.0f, 18.0f, 40.0f, 40.0f));
  auto meter = std::make_shared<Meter>(0.0, 1.0, 0.55);
  addControl(body, meter, Rect::MakeXYWH(20.0f, 70.0f, 300.0f, 20.0f));
  addControl(body, std::make_shared<Spinner>(), Rect::MakeXYWH(340.0f, 68.0f, 24.0f, 24.0f));

  addLabel(body, theme, "Banner", 20.0f, 108.0f);
  auto banner = std::make_shared<Banner>("Saved successfully", theme.body,
    BannerSeverity::Success);
  banner->setIcon(galleryIcon());
  banner->setAction("Undo", [] {});
  addControl(body, banner, Rect::MakeXYWH(20.0f, 132.0f, 620.0f, 40.0f));

  addLabel(body, theme, "ToastStack", 20.0f, 184.0f);
  auto toasts = std::make_shared<ToastStack>(theme.body, 2);
  toasts->show("Saved successfully", std::chrono::seconds(30));
  addControl(body, toasts, Rect::MakeXYWH(20.0f, 208.0f, 380.0f, 34.0f));

  addLabel(body, theme, "StatusIndicator / Pagination", 20.0f, 260.0f);
  addControl(body, std::make_shared<StatusIndicator>("Online", theme.body, StatusKind::Online),
    Rect::MakeXYWH(20.0f, 284.0f, 120.0f, 20.0f));
  addControl(body, std::make_shared<Pagination>(theme.body, 8, 2),
    Rect::MakeXYWH(160.0f, 280.0f, 260.0f, 30.0f));

  addLabel(body, theme, "EmptyState", 420.0f, 184.0f);
  auto empty = std::make_shared<EmptyState>(theme.body, "No projects",
    "Create your first project");
  empty->setIcon(galleryIcon());
  empty->setAction(std::make_shared<Button>("Create", theme.body));
  addControl(body, empty, Rect::MakeXYWH(420.0f, 208.0f, 220.0f, 170.0f));
}

void SectionedGallery::buildData(Layer& body, const Theme& theme) {
  auto empty = std::make_shared<EmptyState>(theme.body, "No projects",
    "Create your first project");
  empty->setIcon(galleryIcon());
  empty->setAction(std::make_shared<Button>("Create", theme.body));
  auto card = std::make_shared<Card>(theme.body, "Card surface", empty);
  card->setHeaderAction(std::make_shared<Button>("Edit", theme.body));
  addControl(body, card, Rect::MakeXYWH(20.0f, 16.0f, 380.0f, 230.0f));

  addLabel(body, theme, "AvatarGroup", 430.0f, 8.0f);
  auto avatars = std::make_shared<AvatarGroup>(
    std::vector<std::string>{ "AL", "MK", "TS" }, theme.body, 2, 28.0f);
  avatars->setInitials({ "AL", "MK", "TS" });
  addControl(body, avatars, Rect::MakeXYWH(430.0f, 32.0f, 180.0f, 28.0f));

  addLabel(body, theme, "Badge / TagList", 430.0f, 76.0f);
  addControl(body, std::make_shared<Badge>("12", theme.body),
    Rect::MakeXYWH(430.0f, 100.0f, 44.0f, 20.0f));
  addControl(body, std::make_shared<TagList>(
    std::vector<std::string>{ "design", "ui", "macOS", "release" }, theme.body, 3),
    Rect::MakeXYWH(490.0f, 98.0f, 220.0f, 26.0f));

  addLabel(body, theme, "Link / Kbd", 430.0f, 140.0f);
  addControl(body, std::make_shared<Link>("Documentation", theme.body),
    Rect::MakeXYWH(430.0f, 164.0f, 140.0f, 20.0f));
  addControl(body, std::make_shared<Kbd>("⌘K", theme.body),
    Rect::MakeXYWH(590.0f, 162.0f, 40.0f, 22.0f));

  addLabel(body, theme, "Stat", 430.0f, 204.0f);
  addControl(body, std::make_shared<Stat>("Users", "1,240", theme.body, "+12%",
    StatTrend::Up), Rect::MakeXYWH(430.0f, 228.0f, 220.0f, 62.0f));

  addLabel(body, theme, "Timeline", 430.0f, 300.0f);
  addControl(body, std::make_shared<Timeline>(std::vector<TimelineEntry>{
    { "Created", "Project created", "09:00", TimelineState::Complete },
    { "Review", "Waiting for review", "10:30", TimelineState::Current },
    { "Ship", "Not started", "Later", TimelineState::Pending },
  }, theme.body), Rect::MakeXYWH(430.0f, 324.0f, 240.0f, 120.0f));

  addLabel(body, theme, "PropertyRow", 20.0f, 264.0f);
  addControl(body, std::make_shared<PropertyRow>("Version", "1.4.2", theme.body),
    Rect::MakeXYWH(20.0f, 288.0f, 380.0f, 18.0f));
}

void SectionedGallery::buildLayout(Layer& body, const Theme& theme) {
  addControl(body, std::make_shared<SectionHeader>("SectionHeader", theme.body,
    "Section subtitle", std::make_shared<Button>("Action", theme.body)),
    Rect::MakeXYWH(20.0f, 16.0f, 620.0f, 38.0f));

  auto field = std::make_shared<FormField>("Email", std::make_shared<TextInput>("", theme.body),
    theme.body);
  field->setHelpText("We never share it");
  addControl(body, field, Rect::MakeXYWH(20.0f, 70.0f, 300.0f, 74.0f));

  auto toolbar = std::make_shared<Toolbar>(theme.body);
  toolbar->addItem(std::make_shared<Button>("One", theme.body));
  toolbar->addSeparator();
  toolbar->addItem(std::make_shared<Button>("Two", theme.body));
  auto expander = std::make_shared<Expander>("Details", theme.body, toolbar);
  addControl(body, expander, Rect::MakeXYWH(20.0f, 160.0f, 360.0f, 160.0f));

  auto drop = std::make_shared<DropZone>("Drop files here", theme.body);
  addControl(body, drop, Rect::MakeXYWH(400.0f, 160.0f, 240.0f, 80.0f));

  addControl(body, std::make_shared<Divider>(Divider::Orientation::Horizontal),
    Rect::MakeXYWH(20.0f, 340.0f, 620.0f, 1.0f));
  addControl(body, std::make_shared<TagInput>(theme.body, "Add tag"),
    Rect::MakeXYWH(20.0f, 360.0f, 360.0f, 34.0f));
  addLabel(body, theme, "StepIndicator", 20.0f, 414.0f);
  addControl(body, std::make_shared<StepIndicator>(
    std::vector<std::string>{ "Account", "Profile", "Done" }, theme.body, 1),
    Rect::MakeXYWH(20.0f, 438.0f, 620.0f, 46.0f));
}

void SectionedGallery::buildLists(Layer& body, const Theme& theme) {
  addLabel(body, theme, "List", 20.0f, 8.0f);
  auto list = std::make_shared<ScrollView>();
  list->setBackgroundColor(theme.surface);
  list->setContent(makeListContent(theme, 18, "Row", 320.0f));
  list->setBounceEnabled(true);
  addControl(body, list, Rect::MakeXYWH(20.0f, 32.0f, 320.0f, 260.0f));

  addLabel(body, theme, "Nested ScrollView (inner list inside outer list)", 20.0f, 306.0f);
  auto outer = std::make_shared<ScrollView>();
  outer->setBackgroundColor(theme.surface);
  auto outerContent = std::make_shared<Flex>(Axis::Column);
  outerContent->setGap(10.0f);
  outerContent->setPadding(Insets{ 16.0f, 16.0f, 16.0f, 16.0f });
  outerContent->setCrossAlign(Align::Start);
  auto inner = std::make_shared<ScrollView>();
  inner->setBackgroundColor(theme.background);
  inner->setContent(makeListContent(theme, 14, "Inner row", 400.0f));
  inner->setBounceEnabled(true);
  inner->setBounds(Rect::MakeWH(400.0f, 300.0f));
  inner->setPreferredHeight(300.0f);
  outerContent->addChild(inner);
  outer->setContent(outerContent);
  outer->setBounceEnabled(true);
  addControl(body, outer, Rect::MakeXYWH(20.0f, 330.0f, 620.0f, 250.0f));
}

void SectionedGallery::buildNative(Layer& body, const Theme& theme) {
#if defined(__APPLE__)
  addLabel(body, theme, "Native AppKit views (macOS)", 20.0f, 8.0f);
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setGap(12.0f);
  row->setCrossAlign(Align::Center);
  row->setBounds(Rect::MakeXYWH(kContentX + 20.0f, kBodyY + 36.0f, 620.0f, 30.0f));
  const auto configure = [this](const auto& control) {
    if (control == nullptr) return;
    control->setControlSize(macos::NativeControlSize::Small);
    if (mNativeDisabled) control->setEnabled(false);
  };
  auto nativeProgress = macos::NativeProgressIndicator::create(false);
  if (nativeProgress != nullptr) {
    nativeProgress->setValue(0.65);
    configure(nativeProgress);
    row->addChild(nativeProgress);
  }
  auto nativeStepper = macos::NativeStepper::create(0.0, 10.0, 4.0,
    [](double) {});
  if (nativeStepper != nullptr) {
    configure(nativeStepper);
    row->addChild(nativeStepper);
  }
  auto nativeButton = macos::NativeButton::create("Native");
  if (nativeButton != nullptr) {
    configure(nativeButton);
    row->addChild(nativeButton);
  }
  auto nativeSwitch = macos::NativeSwitch::create("On", true);
  if (nativeSwitch != nullptr) {
    configure(nativeSwitch);
    row->addChild(nativeSwitch);
  }
  body.addChild(row);

  auto row2 = std::make_shared<Flex>(Axis::Row);
  row2->setGap(12.0f);
  row2->setCrossAlign(Align::Center);
  row2->setBounds(Rect::MakeXYWH(kContentX + 20.0f, kBodyY + 82.0f, 620.0f, 30.0f));
  auto nativeSearch = macos::NativeSearchField::create("Search");
  if (nativeSearch != nullptr) {
    configure(nativeSearch);
    row2->addChild(nativeSearch);
  }
  auto nativePopup = macos::NativePopUpButton::create({ "One", "Two" }, 0);
  if (nativePopup != nullptr) {
    configure(nativePopup);
    row2->addChild(nativePopup);
  }
  body.addChild(row2);
  addLabel(body, theme, "Native controls are live NSViews; headless snapshots show their bounds only.",
    20.0f, 132.0f, 620.0f);
#else
  addLabel(body, theme, "Native AppKit controls are available on macOS only.", 20.0f, 20.0f);
#endif
}

} // namespace

std::shared_ptr<coral::ui::Layer> buildSectionedGallery(
    const std::shared_ptr<coral::ui::Layer>& page, const coral::ui::Theme& theme,
    const bool nativeDisabled, std::shared_ptr<coral::ui::TextInput>& inputField,
    const int initialSection, std::string initialControl) {
  auto gallery = std::make_shared<SectionedGallery>(theme, nativeDisabled, inputField,
    page->bounds().getWidth(), page->bounds().getHeight(), initialSection,
    std::move(initialControl));
  page->addChild(gallery);
  return gallery;
}
