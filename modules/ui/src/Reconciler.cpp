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

#include <ui/Motion.h>
#include <ui/Reconciler.h>
#include <ui/Component.h>

#include <ui/Avatar.h>
#include <ui/AvatarGroup.h>
#include <ui/Badge.h>
#include <ui/Banner.h>
#include <ui/Breadcrumb.h>
#include <ui/Calendar.h>
#include <ui/Card.h>
#include <ui/CheckBoxGroup.h>
#include <ui/EmptyState.h>
#include <ui/Expander.h>
#include <ui/FormField.h>
#include <ui/SectionHeader.h>
#include <ui/SplitView.h>
#include <ui/TabBar.h>
#include <ui/Toolbar.h>
#include <ui/StatusBar.h>
#include <ui/Select.h>
#include <ui/ComboBox.h>
#include <ui/SearchField.h>
#include <ui/PasswordField.h>
#include <ui/NumberField.h>
#include <ui/TagInput.h>
#include <ui/ColorInput.h>
#include <ui/Timeline.h>
#include <ui/ColorPicker.h>
#include <ui/CandidateListView.h>
#include <ui/TableView.h>
#include <ui/DateField.h>
#include <ui/TimeField.h>
#include <ui/DateTimeField.h>
#include <ui/DateRangeField.h>
#include <ui/TimeRangeField.h>
#include <ui/TimePicker.h>
#include <ui/TreeView.h>
#include <ui/Chip.h>
#include <ui/DropZone.h>
#include <ui/MultiSelect.h>
#include <ui/RadioGroup.h>
#include <ui/ToggleGroup.h>
#include <ui/Link.h>
#include <ui/Pagination.h>
#include <ui/RangeSlider.h>
#include <ui/Rating.h>
#include <ui/SplitButton.h>
#include <ui/Stepper.h>
#include <ui/TitleBar.h>
#include <ui/ToggleButton.h>
#include <ui/Button.h>
#include <ui/CheckBox.h>
#include <ui/Divider.h>
#include <ui/Icon.h>
#include <ui/Kbd.h>
#include <ui/Meter.h>
#include <ui/ProgressRing.h>
#include <ui/PropertyRow.h>
#include <ui/Skeleton.h>
#include <ui/Stat.h>
#include <ui/StatusIndicator.h>
#include <ui/StepIndicator.h>
#include <ui/StyledText.h>
#include <ui/TagList.h>
#include <ui/ProgressBar.h>
#include <ui/RadioButton.h>
#include <ui/SegmentedControl.h>
#include <ui/ColorButton.h>
#include <ui/Spinner.h>
#include <ui/IconButton.h>
#include <ui/Table.h>
#include <ui/Toggle.h>
#include <ui/ColorLayer.h>
#include <ui/ContentView.h>
#include <ui/DockView.h>
#include <ui/Flex.h>
#include <ui/Grid.h>
#include <ui/ListView.h>
#include <ui/RecyclingListView.h>
#include <ui/ScrollView.h>
#include <ui/Slider.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <core/observer/Observation.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui {

struct Reconciler::Impl {
  struct Node {
    ElementPtr element;
    std::shared_ptr<Layer> layer;
    std::vector<Node> children;
    observer::Observation inputObservation;
    observer::Observation enabledObservation;
    /** Set for a component-owned node: the reconciler drives its lifecycle. */
    std::shared_ptr<Component> component;
    std::shared_ptr<ComponentContext> componentContext;
  };

  Node root;
  bool mounted = false;
  /** Bounds of the shared-element tags that left the tree, for the next hero transition. */
  std::map<std::string, Rect> sharedElementBounds;
  std::chrono::milliseconds sharedElementDuration{ 250 };
  /** Nodes whose props were applied during the current update; see lastAppliedNodeCount(). */
  uint32_t appliedNodes = 0;

  [[nodiscard]] std::shared_ptr<Layer> createLayer(const ElementPtr& element) {
    switch (element->kind()) {
      case ElementKind::Text: {
        const auto& text = static_cast<const TextElement&>(*element);
        return std::make_shared<TextLayer>(text.text(), text.font(), text.color());
      }
      case ElementKind::Color: {
        const auto& color = static_cast<const ColorElement&>(*element);
        return std::make_shared<ColorLayer>(color.color());
      }
      case ElementKind::Stack: {
        const auto& stack = static_cast<const StackElement&>(*element);
        auto flex = std::make_shared<Flex>(stack.axis());
        flex->setGap(stack.gap());
        flex->setPadding(stack.padding());
        flex->setCrossAlign(stack.crossAlign());
        return flex;
      }
      case ElementKind::Grid: {
        const auto& grid = static_cast<const GridElement&>(*element);
        auto layer = std::make_shared<Grid>(grid.columns());
        layer->setGap(grid.gap());
        layer->setPadding(grid.padding());
        return layer;
      }
      case ElementKind::Dock: {
        const auto& dock = static_cast<const DockElement&>(*element);
        auto layer = std::make_shared<DockView>();
        layer->setSize(DockView::Region::Left, dock.leftWidth());
        layer->setSize(DockView::Region::Right, dock.rightWidth());
        layer->setSize(DockView::Region::Bottom, dock.bottomHeight());
        return layer;
      }
      case ElementKind::Button: {
        const auto& button = static_cast<const ButtonElement&>(*element);
        return std::make_shared<Button>(button.label(), button.font());
      }
      case ElementKind::IconButton: {
        const auto& button = static_cast<const IconButtonElement&>(*element);
        return std::make_shared<IconButton>(button.icon(), button.accessibleName());
      }
      case ElementKind::ColorButton: {
        const auto& button = static_cast<const ColorButtonElement&>(*element);
        return std::make_shared<ColorButton>(button.color(), button.accessibleName());
      }
      case ElementKind::Spinner: {
        const auto& spinner = static_cast<const SpinnerElement&>(*element);
        return std::make_shared<Spinner>(spinner.spinning(), spinner.color());
      }
      case ElementKind::Slider: {
        const auto& slider = static_cast<const SliderElement&>(*element);
        return std::make_shared<Slider>(slider.label(), slider.font(), slider.minimum(),
          slider.maximum(), slider.value());
      }
      case ElementKind::TextInput: {
        const auto& input = static_cast<const TextInputElement&>(*element);
        return std::make_shared<TextInput>(input.text(), input.font());
      }
      case ElementKind::CheckBox: {
        const auto& box = static_cast<const CheckBoxElement&>(*element);
        return std::make_shared<CheckBox>(box.label(), box.font(), box.checked());
      }
      case ElementKind::Toggle: {
        const auto& toggle = static_cast<const ToggleElement&>(*element);
        return std::make_shared<Toggle>(toggle.label(), toggle.font(), toggle.on());
      }
      case ElementKind::RadioButton: {
        const auto& radio = static_cast<const RadioButtonElement&>(*element);
        return std::make_shared<RadioButton>(radio.label(), radio.font(), radio.selected());
      }
      case ElementKind::ProgressBar: {
        const auto& bar = static_cast<const ProgressBarElement&>(*element);
        return std::make_shared<ProgressBar>(bar.font(), bar.value());
      }
      case ElementKind::SegmentedControl: {
        const auto& control = static_cast<const SegmentedControlElement&>(*element);
        return std::make_shared<SegmentedControl>(control.segments(), control.font());
      }
      case ElementKind::Table: {
        const auto& table = static_cast<const TableElement&>(*element);
        auto layer = std::make_shared<Table>(table.columns());
        layer->setGap(table.gap());
        layer->setPadding(table.padding());
        layer->setRows(table.rows());
        return layer;
      }
      case ElementKind::ListView: {
        const auto& list = static_cast<const ListViewElement&>(*element);
        return std::make_shared<ListView>(list.font(), list.rowHeight());
      }
      case ElementKind::RecyclingList: {
        const auto& list = static_cast<const RecyclingListElement&>(*element);
        return std::make_shared<RecyclingListView>(list.rowBuilder(), list.rowHeight());
      }
      case ElementKind::Scroll: {
        const auto& scroll = static_cast<const ScrollElement&>(*element);
        auto view = std::make_shared<ScrollView>();
        view->setPreferredHeight(scroll.preferredHeight());
        return view;
      }
      case ElementKind::ContentView: {
        const auto& content = static_cast<const ContentViewElement&>(*element);
        auto view = std::make_shared<ContentView>(content.host());
        view->setPlaceholderColor(content.placeholder());
        return view;
      }
      case ElementKind::Divider: {
        const auto& divider = static_cast<const DividerElement&>(*element);
        return std::make_shared<Divider>(divider.orientation());
      }
      case ElementKind::Kbd: {
        const auto& kbd = static_cast<const KbdElement&>(*element);
        return std::make_shared<Kbd>(kbd.label(), kbd.font());
      }
      case ElementKind::Badge: {
        const auto& badge = static_cast<const BadgeElement&>(*element);
        return std::make_shared<Badge>(badge.text(), badge.font());
      }
      case ElementKind::Icon: {
        const auto& icon = static_cast<const IconElement&>(*element);
        return std::make_shared<Icon>(icon.image(), icon.size());
      }
      case ElementKind::Avatar: {
        const auto& avatar = static_cast<const AvatarElement&>(*element);
        return std::make_shared<Avatar>(avatar.initials(), avatar.font(), avatar.size());
      }
      case ElementKind::AvatarGroup: {
        const auto& group = static_cast<const AvatarGroupElement&>(*element);
        return std::make_shared<AvatarGroup>(group.initials(), group.font(), group.maxVisible(),
          group.size());
      }
      case ElementKind::Skeleton: {
        const auto& skeleton = static_cast<const SkeletonElement&>(*element);
        return std::make_shared<Skeleton>(skeleton.width(), skeleton.height());
      }
      case ElementKind::Stat: {
        const auto& stat = static_cast<const StatElement&>(*element);
        return std::make_shared<Stat>(stat.label(), stat.value(), stat.font(), stat.caption(),
          stat.trend());
      }
      case ElementKind::Meter: {
        const auto& meter = static_cast<const MeterElement&>(*element);
        return std::make_shared<Meter>(meter.minimum(), meter.maximum(), meter.value());
      }
      case ElementKind::ProgressRing: {
        const auto& ring = static_cast<const ProgressRingElement&>(*element);
        return std::make_shared<ProgressRing>(ring.size(), ring.value());
      }
      case ElementKind::StatusIndicator: {
        const auto& status = static_cast<const StatusIndicatorElement&>(*element);
        return std::make_shared<StatusIndicator>(status.label(), status.font(), status.status());
      }
      case ElementKind::StepIndicator: {
        const auto& steps = static_cast<const StepIndicatorElement&>(*element);
        return std::make_shared<StepIndicator>(steps.steps(), steps.font(), steps.current());
      }
      case ElementKind::PropertyRow: {
        const auto& row = static_cast<const PropertyRowElement&>(*element);
        return std::make_shared<PropertyRow>(row.label(), row.value(), row.font());
      }
      case ElementKind::TagList: {
        const auto& tags = static_cast<const TagListElement&>(*element);
        return std::make_shared<TagList>(tags.tags(), tags.font(), tags.maxVisible());
      }
      case ElementKind::Link: {
        const auto& link = static_cast<const LinkElement&>(*element);
        return std::make_shared<Link>(link.text(), link.font());
      }
      case ElementKind::Chip: {
        const auto& chip = static_cast<const ChipElement&>(*element);
        return std::make_shared<Chip>(chip.text(), chip.font());
      }
      case ElementKind::Rating: {
        const auto& rating = static_cast<const RatingElement&>(*element);
        return std::make_shared<Rating>(rating.stars(), static_cast<int>(rating.rating()));
      }
      case ElementKind::Stepper: {
        const auto& stepper = static_cast<const StepperElement&>(*element);
        return std::make_shared<Stepper>(stepper.minimum(), stepper.maximum(), stepper.value());
      }
      case ElementKind::RangeSlider: {
        const auto& range = static_cast<const RangeSliderElement&>(*element);
        return std::make_shared<RangeSlider>(range.minimum(), range.maximum(), range.low(),
          range.high(), range.step());
      }
      case ElementKind::Pagination: {
        const auto& pagination = static_cast<const PaginationElement&>(*element);
        return std::make_shared<Pagination>(pagination.font(), pagination.pageCount(),
          pagination.page());
      }
      case ElementKind::ToggleButton: {
        const auto& button = static_cast<const ToggleButtonElement&>(*element);
        return std::make_shared<ToggleButton>(button.label(), button.font(), button.selected());
      }
      case ElementKind::Breadcrumb: {
        const auto& crumb = static_cast<const BreadcrumbElement&>(*element);
        return std::make_shared<Breadcrumb>(crumb.items(), crumb.font());
      }
      case ElementKind::Banner: {
        const auto& banner = static_cast<const BannerElement&>(*element);
        return std::make_shared<Banner>(banner.text(), banner.font(), banner.severity());
      }
      case ElementKind::TitleBar: {
        const auto& bar = static_cast<const TitleBarElement&>(*element);
        return std::make_shared<TitleBar>(bar.title(), bar.font());
      }
      case ElementKind::SplitButton: {
        const auto& button = static_cast<const SplitButtonElement&>(*element);
        return std::make_shared<SplitButton>(button.text(), button.font(), button.options());
      }
      case ElementKind::DropZone: {
        const auto& zone = static_cast<const DropZoneElement&>(*element);
        return std::make_shared<DropZone>(zone.label(), zone.font());
      }
      case ElementKind::Calendar: {
        const auto& calendar = static_cast<const CalendarElement&>(*element);
        return std::make_shared<Calendar>(calendar.date(), calendar.font());
      }
      case ElementKind::StyledText: {
        const auto& text = static_cast<const StyledTextElement&>(*element);
        return std::make_shared<StyledText>(text.document());
      }
      case ElementKind::ToggleGroup: {
        const auto& group = static_cast<const ToggleGroupElement&>(*element);
        return std::make_shared<ToggleGroup>(group.options(), group.font());
      }
      case ElementKind::RadioGroup: {
        const auto& group = static_cast<const RadioGroupElement&>(*element);
        return std::make_shared<RadioGroup>(group.options(), group.font(), group.selected());
      }
      case ElementKind::CheckBoxGroup: {
        const auto& group = static_cast<const CheckBoxGroupElement&>(*element);
        return std::make_shared<CheckBoxGroup>(group.options(), group.font());
      }
      case ElementKind::MultiSelect: {
        const auto& select = static_cast<const MultiSelectElement&>(*element);
        return std::make_shared<MultiSelect>(select.options(), select.font());
      }
      case ElementKind::Card: {
        const auto& card = static_cast<const CardElement&>(*element);
        return std::make_shared<Card>(card.font(), card.title());
      }
      case ElementKind::Expander: {
        const auto& expander = static_cast<const ExpanderElement&>(*element);
        return std::make_shared<Expander>(expander.title(), expander.font());
      }
      case ElementKind::FormField: {
        const auto& field = static_cast<const FormFieldElement&>(*element);
        return std::make_shared<FormField>(field.label(), nullptr, field.font());
      }
      case ElementKind::SectionHeader: {
        const auto& header = static_cast<const SectionHeaderElement&>(*element);
        return std::make_shared<SectionHeader>(header.title(), header.font(), header.subtitle());
      }
      case ElementKind::EmptyState: {
        const auto& empty = static_cast<const EmptyStateElement&>(*element);
        return std::make_shared<EmptyState>(empty.font(), empty.title(), empty.description());
      }
      case ElementKind::SplitView: {
        const auto& split = static_cast<const SplitViewElement&>(*element);
        return std::make_shared<SplitView>(split.axis(), split.fraction());
      }
      case ElementKind::TabBar: {
        const auto& bar = static_cast<const TabBarElement&>(*element);
        return std::make_shared<TabBar>(bar.font());
      }
      case ElementKind::Toolbar: {
        const auto& toolbar = static_cast<const ToolbarElement&>(*element);
        return std::make_shared<Toolbar>(toolbar.font());
      }
      case ElementKind::StatusBar: {
        const auto& bar = static_cast<const StatusBarElement&>(*element);
        return std::make_shared<StatusBar>(bar.font(), bar.leftText(), bar.rightText());
      }
      case ElementKind::Select: {
        const auto& select = static_cast<const SelectElement&>(*element);
        return std::make_shared<Select>(select.options(), select.font());
      }
      case ElementKind::ComboBox: {
        const auto& combo = static_cast<const ComboBoxElement&>(*element);
        return std::make_shared<ComboBox>(combo.options(), combo.font(), combo.text());
      }
      case ElementKind::SearchField: {
        const auto& search = static_cast<const SearchFieldElement&>(*element);
        return std::make_shared<SearchField>(search.placeholder(), search.font());
      }
      case ElementKind::PasswordField: {
        const auto& password = static_cast<const PasswordFieldElement&>(*element);
        return std::make_shared<PasswordField>(password.placeholder(), password.font());
      }
      case ElementKind::NumberField: {
        const auto& field = static_cast<const NumberFieldElement&>(*element);
        return std::make_shared<NumberField>(field.font(), field.minimum(), field.maximum(),
          field.value());
      }
      case ElementKind::TagInput: {
        const auto& tags = static_cast<const TagInputElement&>(*element);
        return std::make_shared<TagInput>(tags.font(), tags.placeholder());
      }
      case ElementKind::ColorInput: {
        const auto& input = static_cast<const ColorInputElement&>(*element);
        return std::make_shared<ColorInput>(input.font(), input.color());
      }      case ElementKind::Timeline: {
        const auto& timeline = static_cast<const TimelineElement&>(*element);
        return std::make_shared<Timeline>(timeline.entries(), timeline.font());
      }
      case ElementKind::ColorPicker: {
        const auto& picker = static_cast<const ColorPickerElement&>(*element);
        return std::make_shared<ColorPicker>(picker.palette());
      }
      case ElementKind::CandidateListView: {
        const auto& list = static_cast<const CandidateListViewElement&>(*element);
        return std::make_shared<CandidateListView>(list.font());
      }
      case ElementKind::TableView:
        return std::make_shared<TableView>();      case ElementKind::DateField: {
        const auto& field = static_cast<const DateFieldElement&>(*element);
        return std::make_shared<DateField>(field.font(), field.date());
      }
      case ElementKind::TimeField: {
        const auto& field = static_cast<const TimeFieldElement&>(*element);
        return std::make_shared<TimeField>(field.font(), field.hour(), field.minute(),
          field.use24HourClock());
      }
      case ElementKind::DateTimeField: {
        const auto& field = static_cast<const DateTimeFieldElement&>(*element);
        return std::make_shared<DateTimeField>(field.font(), field.date(), field.hour(),
          field.minute());
      }
      case ElementKind::DateRangeField: {
        const auto& field = static_cast<const DateRangeFieldElement&>(*element);
        return std::make_shared<DateRangeField>(field.font(), field.start(), field.end());
      }
      case ElementKind::TimeRangeField: {
        const auto& field = static_cast<const TimeRangeFieldElement&>(*element);
        return std::make_shared<TimeRangeField>(field.font(), field.startHour(),
          field.startMinute(), field.endHour(), field.endMinute());
      }
      case ElementKind::TimePicker: {
        const auto& picker = static_cast<const TimePickerElement&>(*element);
        return std::make_shared<TimePicker>(picker.font(), picker.hour(), picker.minute(),
          picker.use24HourClock());
      }
      case ElementKind::TreeView: {
        const auto& tree = static_cast<const TreeViewElement&>(*element);
        return std::make_shared<TreeView>(tree.font(), tree.rowHeight());
      }
      case ElementKind::Layer: {
        const auto& host = static_cast<const LayerElement&>(*element);
        return host.layer();
      }
      case ElementKind::When:
      case ElementKind::Component:
      case ElementKind::Props:
        // These mount through their resolved branch in `mountNode`.
        return nullptr;
    }
    return nullptr;
  }

  void applyProps(const ElementPtr& element, Layer& layer) {
    switch (element->kind()) {
      case ElementKind::Text: {
        const auto& text = static_cast<const TextElement&>(*element);
        auto& label = static_cast<TextLayer&>(layer);
        label.setText(text.text());
        label.setFont(text.font());
        label.setColor(text.color());
        break;
      }
      case ElementKind::Color: {
        const auto& color = static_cast<const ColorElement&>(*element);
        static_cast<ColorLayer&>(layer).setColor(color.color());
        break;
      }
      case ElementKind::Stack: {
        const auto& stack = static_cast<const StackElement&>(*element);
        auto& flex = static_cast<Flex&>(layer);
        flex.setAxis(stack.axis());
        flex.setGap(stack.gap());
        flex.setPadding(stack.padding());
        flex.setCrossAlign(stack.crossAlign());
        break;
      }
      case ElementKind::Grid: {
        const auto& grid = static_cast<const GridElement&>(*element);
        auto& view = static_cast<Grid&>(layer);
        view.setColumns(grid.columns());
        view.setGap(grid.gap());
        view.setPadding(grid.padding());
        break;
      }
      case ElementKind::Dock: {
        const auto& dock = static_cast<const DockElement&>(*element);
        auto& view = static_cast<DockView&>(layer);
        view.setSize(DockView::Region::Left, dock.leftWidth());
        view.setSize(DockView::Region::Right, dock.rightWidth());
        view.setSize(DockView::Region::Bottom, dock.bottomHeight());
        break;
      }
      case ElementKind::Button: {
        const auto& button = static_cast<const ButtonElement&>(*element);
        auto& buttonLayer = static_cast<Button&>(layer);
        buttonLayer.setLabel(button.label());
        buttonLayer.setFont(button.font());
        buttonLayer.setOnClick(button.onClick());
        if (button.enabledProperty() == nullptr) buttonLayer.setEnabled(button.enabled());
        break;
      }
      case ElementKind::Slider: {
        const auto& slider = static_cast<const SliderElement&>(*element);
        auto& view = static_cast<Slider&>(layer);
        view.setLabel(slider.label());
        view.setFont(slider.font());
        view.setRange(slider.minimum(), slider.maximum());
        if (view.value() != slider.value()) view.setValue(slider.value());
        view.setOnChanged(slider.onChanged());
        view.setOnEditBegin(slider.onEditBegin());
        view.setOnEditEnd(slider.onEditEnd());
        break;
      }
      case ElementKind::TextInput: {
        const auto& input = static_cast<const TextInputElement&>(*element);
        auto& field = static_cast<TextInput&>(layer);
        if (field.text() != input.text()) field.setText(input.text());
        field.setFont(input.font());
        break;
      }
      case ElementKind::ListView: {
        const auto& list = static_cast<const ListViewElement&>(*element);
        auto& view = static_cast<ListView&>(layer);
        view.setItems(list.items());
        view.setOnSelection(list.onSelection());
        view.setSelectedIndex(list.selected());
        break;
      }
      case ElementKind::RecyclingList: {
        const auto& list = static_cast<const RecyclingListElement&>(*element);
        auto& view = static_cast<RecyclingListView&>(layer);
        view.setRowBuilder(list.rowBuilder());
        view.setRows(list.keys());
        break;
      }
      case ElementKind::Scroll: {
        const auto& scroll = static_cast<const ScrollElement&>(*element);
        static_cast<ScrollView&>(layer).setPreferredHeight(scroll.preferredHeight());
        break;
      }
      case ElementKind::ContentView: {
        const auto& content = static_cast<const ContentViewElement&>(*element);
        auto& view = static_cast<ContentView&>(layer);
        view.setHost(content.host());
        view.setPlaceholderColor(content.placeholder());
        break;
      }
      case ElementKind::Divider: {
        const auto& divider = static_cast<const DividerElement&>(*element);
        auto& rule = static_cast<Divider&>(layer);
        rule.setOrientation(divider.orientation());
        rule.setThickness(divider.thickness());
        rule.setInset(divider.inset());
        if (divider.color().has_value()) rule.setColor(*divider.color());
        break;
      }
      case ElementKind::Kbd: {
        const auto& kbd = static_cast<const KbdElement&>(*element);
        auto& cap = static_cast<Kbd&>(layer);
        cap.setLabel(kbd.label());
        cap.setFont(kbd.font());
        break;
      }
      case ElementKind::Badge: {
        const auto& badge = static_cast<const BadgeElement&>(*element);
        auto& pill = static_cast<Badge&>(layer);
        pill.setText(badge.text());
        pill.setFont(badge.font());
        pill.setDot(badge.dot());
        pill.setIcon(badge.icon());
        break;
      }
      case ElementKind::Icon: {
        const auto& icon = static_cast<const IconElement&>(*element);
        auto& image = static_cast<Icon&>(layer);
        image.setImage(icon.image());
        image.setSize(icon.size());
        if (icon.color().has_value()) image.setColor(*icon.color());
        break;
      }
      case ElementKind::Avatar: {
        const auto& avatar = static_cast<const AvatarElement&>(*element);
        auto& view = static_cast<Avatar&>(layer);
        view.setInitials(avatar.initials());
        view.setFont(avatar.font());
        view.setSize(avatar.size());
        view.setImage(avatar.image());
        view.setStatusVisible(avatar.statusVisible());
        if (avatar.statusColor().has_value()) view.setStatusColor(*avatar.statusColor());
        break;
      }
      case ElementKind::AvatarGroup: {
        const auto& group = static_cast<const AvatarGroupElement&>(*element);
        auto& view = static_cast<AvatarGroup&>(layer);
        view.setInitials(group.initials());
        view.setMaxVisible(group.maxVisible());
        view.setSize(group.size());
        break;
      }
      case ElementKind::Skeleton: {
        const auto& skeleton = static_cast<const SkeletonElement&>(*element);
        auto& view = static_cast<Skeleton&>(layer);
        view.setSize(skeleton.width(), skeleton.height());
        view.setLines(skeleton.lines());
        view.setLineGap(skeleton.lineGap());
        view.setShape(skeleton.shape());
        view.setAnimating(skeleton.animating());
        break;
      }
      case ElementKind::Stat: {
        const auto& stat = static_cast<const StatElement&>(*element);
        auto& view = static_cast<Stat&>(layer);
        view.setLabel(stat.label());
        view.setValue(stat.value());
        view.setCaption(stat.caption());
        view.setTrend(stat.trend());
        view.setFont(stat.font());
        break;
      }
      case ElementKind::Meter: {
        const auto& meter = static_cast<const MeterElement&>(*element);
        auto& view = static_cast<Meter&>(layer);
        view.setRange(meter.minimum(), meter.maximum());
        view.setValue(meter.value());
        break;
      }
      case ElementKind::ProgressRing: {
        const auto& ring = static_cast<const ProgressRingElement&>(*element);
        auto& view = static_cast<ProgressRing&>(layer);
        view.setSize(ring.size());
        view.setValue(ring.value());
        view.setThickness(ring.thickness());
        view.setIndeterminate(ring.indeterminate());
        break;
      }
      case ElementKind::StatusIndicator: {
        const auto& status = static_cast<const StatusIndicatorElement&>(*element);
        auto& view = static_cast<StatusIndicator&>(layer);
        view.setLabel(status.label());
        view.setFont(status.font());
        view.setStatus(status.status());
        break;
      }
      case ElementKind::StepIndicator: {
        const auto& steps = static_cast<const StepIndicatorElement&>(*element);
        auto& view = static_cast<StepIndicator&>(layer);
        if (view.font() != steps.font()) view.setFont(steps.font());
        if (view.steps() != steps.steps()) view.setSteps(steps.steps());
        view.setCurrent(steps.current());
        break;
      }
      case ElementKind::PropertyRow: {
        const auto& row = static_cast<const PropertyRowElement&>(*element);
        auto& view = static_cast<PropertyRow&>(layer);
        view.setLabel(row.label());
        view.setValue(row.value());
        view.setHelpText(row.help());
        view.setFont(row.font());
        if (row.labelWidth() > 0.0f) view.setLabelWidth(row.labelWidth());
        break;
      }
      case ElementKind::TagList: {
        const auto& tags = static_cast<const TagListElement&>(*element);
        auto& view = static_cast<TagList&>(layer);
        view.setTags(tags.tags());
        view.setMaxVisible(tags.maxVisible());
        break;
      }
      case ElementKind::Link: {
        const auto& link = static_cast<const LinkElement&>(*element);
        auto& view = static_cast<Link&>(layer);
        view.setText(link.text());
        view.setFont(link.font());
        break;
      }
      case ElementKind::Chip: {
        const auto& chip = static_cast<const ChipElement&>(*element);
        auto& view = static_cast<Chip&>(layer);
        view.setText(chip.text());
        view.setFont(chip.font());
        view.setVariant(chip.variant());
        view.setIcon(chip.icon());
        view.setRemovable(chip.removable());
        view.setSelected(chip.selected());
        break;
      }
      case ElementKind::Rating: {
        const auto& rating = static_cast<const RatingElement&>(*element);
        auto& view = static_cast<Rating&>(layer);
        if (view.stars() != rating.stars()) view.setStars(rating.stars());
        view.setRating(rating.rating());
        view.setHalfSteps(rating.halfSteps());
        view.setReadOnly(rating.readOnly());
        break;
      }
      case ElementKind::Stepper: {
        const auto& stepper = static_cast<const StepperElement&>(*element);
        auto& view = static_cast<Stepper&>(layer);
        view.setRange(stepper.minimum(), stepper.maximum());
        view.setStep(stepper.step());
        view.setValue(stepper.value());
        break;
      }
      case ElementKind::RangeSlider: {
        const auto& range = static_cast<const RangeSliderElement&>(*element);
        auto& view = static_cast<RangeSlider&>(layer);
        view.setRange(range.minimum(), range.maximum());
        view.setStep(range.step());
        view.setLow(range.low());
        view.setHigh(range.high());
        break;
      }
      case ElementKind::Pagination: {
        const auto& pagination = static_cast<const PaginationElement&>(*element);
        auto& view = static_cast<Pagination&>(layer);
        view.setPageCount(pagination.pageCount());
        view.setPage(pagination.page());
        break;
      }
      case ElementKind::ToggleButton: {
        const auto& button = static_cast<const ToggleButtonElement&>(*element);
        auto& view = static_cast<ToggleButton&>(layer);
        view.setLabel(button.label());
        view.setFont(button.font());
        view.setSelected(button.selected());
        break;
      }
      case ElementKind::Breadcrumb: {
        const auto& crumb = static_cast<const BreadcrumbElement&>(*element);
        auto& view = static_cast<Breadcrumb&>(layer);
        if (view.items() != crumb.items()) view.setItems(crumb.items());
        view.setFont(crumb.font());
        view.setSeparator(crumb.separator());
        view.setCurrentIndex(crumb.currentIndex());
        break;
      }
      case ElementKind::Banner: {
        const auto& banner = static_cast<const BannerElement&>(*element);
        auto& view = static_cast<Banner&>(layer);
        view.setText(banner.text());
        view.setFont(banner.font());
        view.setSeverity(banner.severity());
        view.setDismissible(banner.dismissible());
        view.setIcon(banner.icon());
        break;
      }
      case ElementKind::TitleBar: {
        const auto& bar = static_cast<const TitleBarElement&>(*element);
        auto& view = static_cast<TitleBar&>(layer);
        view.setTitle(bar.title());
        view.setShowMinimize(bar.showMinimize());
        view.setShowMaximize(bar.showMaximize());
        view.setShowClose(bar.showClose());
        break;
      }
      case ElementKind::SplitButton: {
        const auto& button = static_cast<const SplitButtonElement&>(*element);
        auto& view = static_cast<SplitButton&>(layer);
        view.setText(button.text());
        view.setFont(button.font());
        view.setOptions(button.options());
        break;
      }
      case ElementKind::DropZone: {
        const auto& zone = static_cast<const DropZoneElement&>(*element);
        auto& view = static_cast<DropZone&>(layer);
        view.setLabel(zone.label());
        view.setFont(zone.font());
        break;
      }
      case ElementKind::Calendar: {
        const auto& calendar = static_cast<const CalendarElement&>(*element);
        auto& view = static_cast<Calendar&>(layer);
        view.setDate(calendar.date());
        view.setFont(calendar.font());
        view.setFirstWeekday(calendar.firstWeekday());
        break;
      }
      case ElementKind::StyledText: {
        const auto& text = static_cast<const StyledTextElement&>(*element);
        static_cast<StyledText&>(layer).setDocument(text.document());
        break;
      }
      case ElementKind::ToggleGroup: {
        const auto& group = static_cast<const ToggleGroupElement&>(*element);
        auto& view = static_cast<ToggleGroup&>(layer);
        if (view.options() != group.options()) view.setOptions(group.options());
        view.setSelectedIndices(group.selected());
        break;
      }
      case ElementKind::RadioGroup: {
        const auto& group = static_cast<const RadioGroupElement&>(*element);
        auto& view = static_cast<RadioGroup&>(layer);
        if (view.options() != group.options()) view.setOptions(group.options());
        view.setSelectedIndex(group.selected());
        break;
      }
      case ElementKind::CheckBoxGroup: {
        const auto& group = static_cast<const CheckBoxGroupElement&>(*element);
        auto& view = static_cast<CheckBoxGroup&>(layer);
        if (view.options() != group.options()) view.setOptions(group.options());
        view.setSelectedIndices(group.selected());
        break;
      }
      case ElementKind::MultiSelect: {
        const auto& select = static_cast<const MultiSelectElement&>(*element);
        auto& view = static_cast<MultiSelect&>(layer);
        if (view.options() != select.options()) view.setOptions(select.options());
        view.setSelectedIndices(select.selected());
        view.setPlaceholder(select.placeholder());
        view.setFont(select.font());
        break;
      }
      case ElementKind::Card: {
        const auto& card = static_cast<const CardElement&>(*element);
        auto& view = static_cast<Card&>(layer);
        view.setTitle(card.title());
        view.setFont(card.font());
        break;
      }
      case ElementKind::Expander: {
        const auto& expander = static_cast<const ExpanderElement&>(*element);
        auto& view = static_cast<Expander&>(layer);
        view.setTitle(expander.title());
        view.setFont(expander.font());
        view.setExpanded(expander.expanded());
        break;
      }
      case ElementKind::FormField: {
        const auto& field = static_cast<const FormFieldElement&>(*element);
        auto& view = static_cast<FormField&>(layer);
        view.setLabel(field.label());
        view.setHelpText(field.help());
        view.setErrorText(field.error());
        view.setFont(field.font());
        break;
      }
      case ElementKind::SectionHeader: {
        const auto& header = static_cast<const SectionHeaderElement&>(*element);
        auto& view = static_cast<SectionHeader&>(layer);
        view.setTitle(header.title());
        view.setSubtitle(header.subtitle());
        view.setFont(header.font());
        break;
      }
      case ElementKind::EmptyState: {
        const auto& empty = static_cast<const EmptyStateElement&>(*element);
        auto& view = static_cast<EmptyState&>(layer);
        view.setTitle(empty.title());
        view.setDescription(empty.description());
        view.setFont(empty.font());
        view.setIcon(empty.icon());
        break;
      }
      case ElementKind::SplitView: {
        const auto& split = static_cast<const SplitViewElement&>(*element);
        static_cast<SplitView&>(layer).setFraction(split.fraction());
        break;
      }
      case ElementKind::TabBar:
        // The tab list is rebuilt by `patchContainerSlots`, not by a value setter.
        break;
      case ElementKind::Toolbar:
        static_cast<Toolbar&>(layer).setHeight(
          static_cast<const ToolbarElement&>(*element).height());
        break;
      case ElementKind::StatusBar: {
        const auto& bar = static_cast<const StatusBarElement&>(*element);
        auto& view = static_cast<StatusBar&>(layer);
        view.setLeftText(bar.leftText());
        view.setRightText(bar.rightText());
        view.setFont(bar.font());
        break;
      }
      case ElementKind::Select: {
        const auto& select = static_cast<const SelectElement&>(*element);
        auto& view = static_cast<Select&>(layer);
        if (view.options() != select.options()) view.setOptions(select.options());
        view.setFont(select.font());
        view.setPlaceholder(select.placeholder());
        view.setSelectedIndex(select.selected());
        break;
      }
      case ElementKind::ComboBox: {
        const auto& combo = static_cast<const ComboBoxElement&>(*element);
        auto& view = static_cast<ComboBox&>(layer);
        if (view.options() != combo.options()) view.setOptions(combo.options());
        view.setFont(combo.font());
        view.setPlaceholder(combo.placeholder());
        if (view.text() != combo.text()) view.setText(combo.text());
        view.setSelectedIndex(combo.selected());
        break;
      }
      case ElementKind::SearchField: {
        const auto& search = static_cast<const SearchFieldElement&>(*element);
        auto& view = static_cast<SearchField&>(layer);
        view.setPlaceholder(search.placeholder());
        view.setFont(search.font());
        if (view.text() != search.text()) view.setText(search.text());
        break;
      }
      case ElementKind::PasswordField: {
        const auto& password = static_cast<const PasswordFieldElement&>(*element);
        auto& view = static_cast<PasswordField&>(layer);
        view.setPlaceholder(password.placeholder());
        view.setFont(password.font());
        view.setMaxLength(password.maxLength());
        if (view.text() != password.text()) view.setText(password.text());
        break;
      }
      case ElementKind::NumberField: {
        const auto& field = static_cast<const NumberFieldElement&>(*element);
        auto& view = static_cast<NumberField&>(layer);
        view.setFont(field.font());
        view.setRange(field.minimum(), field.maximum());
        view.setStep(field.step());
        view.setDecimals(field.decimals());
        view.setValue(field.value());
        break;
      }
      case ElementKind::TagInput: {
        const auto& tags = static_cast<const TagInputElement&>(*element);
        auto& view = static_cast<TagInput&>(layer);
        if (view.tokens() != tags.tokens()) view.setTokens(tags.tokens());
        view.setPlaceholder(tags.placeholder());
        view.setFont(tags.font());
        break;
      }
      case ElementKind::ColorInput: {
        const auto& input = static_cast<const ColorInputElement&>(*element);
        auto& view = static_cast<ColorInput&>(layer);
        view.setColor(input.color());
        view.setFont(input.font());
        break;
      }      case ElementKind::Timeline: {
        const auto& timeline = static_cast<const TimelineElement&>(*element);
        auto& view = static_cast<Timeline&>(layer);
        if (view.entries().size() != timeline.entries().size() ||
            !std::equal(view.entries().begin(), view.entries().end(),
              timeline.entries().begin(), [](const TimelineEntry& a, const TimelineEntry& b) {
                return a.title == b.title && a.detail == b.detail && a.time == b.time &&
                  a.state == b.state;
              })) {
          view.setEntries(timeline.entries());
        }
        view.setFont(timeline.font());
        break;
      }
      case ElementKind::ColorPicker: {
        const auto& picker = static_cast<const ColorPickerElement&>(*element);
        auto& view = static_cast<ColorPicker&>(layer);
        if (view.palette() != picker.palette()) view.setPalette(picker.palette());
        view.setMode(picker.mode());
        view.setColor(picker.color());
        view.setAlpha(picker.alpha());
        view.setSelected(picker.selected());
        break;
      }
      case ElementKind::CandidateListView: {
        const auto& list = static_cast<const CandidateListViewElement&>(*element);
        auto& view = static_cast<CandidateListView&>(layer);
        view.setFont(list.font());
        view.setCandidates(list.candidates(), list.selected());
        break;
      }
      case ElementKind::TableView: {
        const auto& table = static_cast<const TableViewElement&>(*element);
        auto& view = static_cast<TableView&>(layer);
        view.setColumns(table.columns());
        view.setRows(table.rows());
        view.setGap(table.gap());
        view.setPadding(table.padding());
        view.setSelectedRow(table.selected());
        break;
      }      case ElementKind::DateField: {
        const auto& field = static_cast<const DateFieldElement&>(*element);
        auto& view = static_cast<DateField&>(layer);
        view.setFont(field.font());
        view.setDate(field.date());
        break;
      }
      case ElementKind::TimeField: {
        const auto& field = static_cast<const TimeFieldElement&>(*element);
        auto& view = static_cast<TimeField&>(layer);
        view.setFont(field.font());
        view.setTime(field.hour(), field.minute());
        if (field.use24HourClock().has_value()) view.setUse24HourClock(*field.use24HourClock());
        break;
      }
      case ElementKind::DateTimeField: {
        const auto& field = static_cast<const DateTimeFieldElement&>(*element);
        auto& view = static_cast<DateTimeField&>(layer);
        view.setFont(field.font());
        view.setDate(field.date());
        view.setTime(field.hour(), field.minute());
        break;
      }
      case ElementKind::DateRangeField: {
        const auto& field = static_cast<const DateRangeFieldElement&>(*element);
        auto& view = static_cast<DateRangeField&>(layer);
        view.setFont(field.font());
        view.setStart(field.start());
        view.setEnd(field.end());
        break;
      }
      case ElementKind::TimeRangeField: {
        const auto& field = static_cast<const TimeRangeFieldElement&>(*element);
        auto& view = static_cast<TimeRangeField&>(layer);
        view.setFont(field.font());
        view.setStart(field.startHour(), field.startMinute());
        view.setEnd(field.endHour(), field.endMinute());
        break;
      }
      case ElementKind::TimePicker: {
        const auto& picker = static_cast<const TimePickerElement&>(*element);
        auto& view = static_cast<TimePicker&>(layer);
        view.setFont(picker.font());
        view.setTime(picker.hour(), picker.minute());
        view.setUse24HourClock(picker.use24HourClock());
        break;
      }
      case ElementKind::TreeView: {
        const auto& tree = static_cast<const TreeViewElement&>(*element);
        auto& view = static_cast<TreeView&>(layer);
        view.setOnSelection({});
        view.setOnActivated({});
        view.clear();
        std::function<int(const TreeNodeSpec&, int, bool)> addNode;
        addNode = [&view, &addNode](const TreeNodeSpec& node, const int parent,
                      const bool root) -> int {
          const int id = root ? view.addRoot(node.label, node.expanded)
                              : view.addChild(parent, node.label, node.folder);
          if (id < 0) return id;
          if (!node.detail.empty()) view.setNodeDetail(id, node.detail);
          if (node.icon != nullptr) view.setNodeIcon(id, node.icon);
          if (!root) view.setExpanded(id, node.expanded);
          for (const TreeNodeSpec& child: node.children) addNode(child, id, false);
          return id;
        };
        for (const TreeNodeSpec& root: tree.roots()) addNode(root, -1, true);
        view.setSelected(tree.selectedId());
        break;
      }      case ElementKind::Layer:
        // A hosted product widget owns its own properties.
        break;
      case ElementKind::When:
      case ElementKind::Component:
      case ElementKind::Props:
        break;
    }
  }

  static void applyElementProps(const ElementProps& props, Layer& layer) {
    if (props.bounds.has_value()) {
      const bool animate = props.layoutTransition.has_value() &&
        props.layoutTransition->count() > 0 && layer.bounds() != *props.bounds;
      if (animate) {
        layer.animateBounds(*props.bounds, *props.layoutTransition);
      } else {
        layer.setBounds(*props.bounds);
      }
    }
    layer.setLayoutParams(props.layout);
    layer.setVisible(props.visible);
    layer.setOpacity(props.opacity);
    layer.setClip(props.clip);
    if (props.backgroundRole.has_value()) {
      layer.setBackgroundColorRole(*props.backgroundRole);
    } else if (props.background.has_value()) {
      layer.setBackgroundColor(*props.background);
    }
    if (props.cornerRadius.has_value()) layer.setClipRadius(*props.cornerRadius);
    if (props.shadow.has_value()) {
      layer.setShadow(*props.shadow);
    } else {
      layer.clearShadow();
    }
    if (props.styleName.has_value()) layer.setStyleName(*props.styleName);
  }

  [[nodiscard]] static std::shared_ptr<Layer> emptyLayer() {
    return std::make_shared<Layer>();
  }

  [[nodiscard]] static ElementPtr resolveBranch(const ElementPtr& element) {
    if (element->kind() == ElementKind::When) {
      const auto& when = static_cast<const WhenElement&>(*element);
      return when.condition() ? when.thenBranch() : when.elseBranch();
    }
    if (element->kind() == ElementKind::Component) {
      const auto& component = static_cast<const ComponentElement&>(*element);
      return component.built();
    }
    return element;
  }

  void patchBranch(Node& node, const ElementPtr& branch) {
    if (branch == nullptr) {
      for (Node& child: node.children) disposeNode(child);
      node.children.clear();
      // No empty layer: a false `When` must not occupy a Flex slot or add a gap.
      node.layer = nullptr;
      return;
    }
    if (node.children.empty() || !sameIdentity(node.children.front().element, branch)) {
      for (Node& child: node.children) disposeNode(child);
      node.children.clear();
      node.children.push_back(mountNode(branch));
    } else {
      patchNode(node.children.front(), branch);
    }
    node.layer = node.children.front().layer;
  }

  [[nodiscard]] bool sameIdentity(const ElementPtr& left, const ElementPtr& right) const {
    return left->kind() == right->kind() && left->key() == right->key();
  }

  void attachButtonEnabled(Node& node, const std::shared_ptr<const ButtonElement>& button) {
    node.enabledObservation = observer::Observation{};
    if (button == nullptr || node.layer == nullptr || button->enabledProperty() == nullptr) return;
    const std::shared_ptr<Layer> layer = node.layer;
    node.enabledObservation = button->enabledProperty()->observe(
      [layer](const bool&, const bool& value) {
        static_cast<Button&>(*layer).setEnabled(value);
      },
      nullptr, true);
  }

  void attachInput(Node& node, const std::shared_ptr<const TextInputElement>& input) {
    node.inputObservation = observer::Observation{};
    if (input == nullptr || node.layer == nullptr) return;
    const auto& handler = input->onChanged();
    if (!handler) return;
    node.inputObservation = static_cast<TextInput&>(*node.layer).observeChanged(
      [input](const std::string& value) {
        const auto& callback = input->onChanged();
        if (callback) callback(value);
      });
  }

  /** Runs a node's component unmount and disposals, then drops its subtree. */
  /** Detaches a slot container's children before the widget is destroyed or the slot is reused. */
  static void clearContainerSlots(Node& node) {
    if (node.element == nullptr || node.layer == nullptr) return;
    switch (node.element->kind()) {
      case ElementKind::Card: {
        auto& card = static_cast<Card&>(*node.layer);
        card.setContent(nullptr);
        card.setHeaderAction(nullptr);
        break;
      }
      case ElementKind::Expander:
        static_cast<Expander&>(*node.layer).setContent(nullptr);
        break;
      case ElementKind::FormField:
        static_cast<FormField&>(*node.layer).setContent(nullptr);
        break;
      case ElementKind::SectionHeader:
        static_cast<SectionHeader&>(*node.layer).setAction(nullptr);
        break;
      case ElementKind::EmptyState: {
        auto& empty = static_cast<EmptyState&>(*node.layer);
        empty.setContent(nullptr);
        empty.setAction(nullptr);
        break;
      }
      case ElementKind::SplitView:
        static_cast<SplitView&>(*node.layer).setPanes(nullptr, nullptr);
        break;
      case ElementKind::TabBar:
        static_cast<TabBar&>(*node.layer).setOnChanged({});
        static_cast<TabBar&>(*node.layer).clear();
        break;
      case ElementKind::Toolbar:
        static_cast<Toolbar&>(*node.layer).clear();
        break;
      case ElementKind::StatusBar:
        static_cast<StatusBar&>(*node.layer).clearItems();
        break;
      default:
        break;
    }
  }

  static bool isSlotContainer(const ElementKind kind) {
    return kind == ElementKind::Card || kind == ElementKind::Expander ||
      kind == ElementKind::FormField || kind == ElementKind::SectionHeader ||
      kind == ElementKind::EmptyState || kind == ElementKind::SplitView ||
      kind == ElementKind::TabBar || kind == ElementKind::Toolbar ||
      kind == ElementKind::StatusBar;
  }
  void disposeNode(Node& node) {
    // Clear the public refs of a disappearing props wrapper: a caller that kept the mounted layer
    // alive would otherwise still see it through LayerRef after it left the tree.
    if (node.element != nullptr && node.element->kind() == ElementKind::Props) {
      const auto& props = static_cast<const PropsElement&>(*node.element);
      if (props.props().ref != nullptr) props.props().ref->clear();
      if (!props.props().sharedElementTag.empty() && node.layer != nullptr) {
        sharedElementBounds[props.props().sharedElementTag] = node.layer->bounds();
      }
    }
    clearContainerSlots(node);
    for (Node& child: node.children) disposeNode(child);
    if (node.component != nullptr) {
      node.component->onUnmount();
      if (node.componentContext != nullptr) node.componentContext->runDisposals();
      node.component.reset();
      node.componentContext.reset();
    }
    node.children.clear();
    node.layer = nullptr;
  }

  /**
   * True for the kinds whose props carry callbacks.
   *
   * `measureEquivalent` deliberately ignores callbacks, so these kinds must re-apply their props on
   * every patch: otherwise a rebuild that only swapped an `onClick` would keep the old handler.
   */
  static bool needsCallbackRebind(const ElementKind kind) {
    switch (kind) {
      case ElementKind::Button:
      case ElementKind::IconButton:
      case ElementKind::ColorButton:
      case ElementKind::CheckBox:
      case ElementKind::Toggle:
      case ElementKind::RadioButton:
      case ElementKind::Slider:
      case ElementKind::TextInput:
      case ElementKind::ListView:
      case ElementKind::RecyclingList:
      case ElementKind::SegmentedControl:
      case ElementKind::Component:
        return true;
      default:
        return false;
    }
  }

  /**
   * Applies a node's value props only when they changed.
   *
   * This is the property-level half of the retained-mode work: a rebuild that produced an equivalent
   * element must not walk the setter chain at all. Kinds with callbacks take the unconditional path.
   */
  /**
   * Value equality of an element's *own* props, ignoring its children.
   *
   * `measureEquivalent` answers a layout question and deliberately includes the child structure; using
   * it here would re-apply a container's axis/gap/padding every time one child changed. Children are
   * patched on their own, so this comparison only looks at the fields the kind itself owns.
   */
  static bool ownPropsEquivalent(const ElementPtr& left, const ElementPtr& right) {
    if (left == nullptr || right == nullptr) return left == right;
    if (left->kind() != right->kind() || left->key() != right->key()) return false;
    switch (left->kind()) {
      case ElementKind::Stack: {
        const auto& a = static_cast<const StackElement&>(*left);
        const auto& b = static_cast<const StackElement&>(*right);
        return a.axis() == b.axis() && a.gap() == b.gap() && a.crossAlign() == b.crossAlign() &&
          detail::sameInsets(a.padding(), b.padding());
      }
      case ElementKind::Grid: {
        const auto& a = static_cast<const GridElement&>(*left);
        const auto& b = static_cast<const GridElement&>(*right);
        return a.columns() == b.columns() && a.gap() == b.gap() &&
          detail::sameInsets(a.padding(), b.padding());
      }
      case ElementKind::Dock: {
        const auto& a = static_cast<const DockElement&>(*left);
        const auto& b = static_cast<const DockElement&>(*right);
        return a.leftWidth() == b.leftWidth() && a.rightWidth() == b.rightWidth() &&
          a.bottomHeight() == b.bottomHeight();
      }
      case ElementKind::Card: {
        const auto& a = static_cast<const CardElement&>(*left);
        const auto& b = static_cast<const CardElement&>(*right);
        return a.title() == b.title() && a.font() == b.font();
      }
      case ElementKind::Expander: {
        const auto& a = static_cast<const ExpanderElement&>(*left);
        const auto& b = static_cast<const ExpanderElement&>(*right);
        return a.title() == b.title() && a.font() == b.font() && a.expanded() == b.expanded();
      }
      case ElementKind::FormField: {
        const auto& a = static_cast<const FormFieldElement&>(*left);
        const auto& b = static_cast<const FormFieldElement&>(*right);
        return a.label() == b.label() && a.help() == b.help() && a.error() == b.error() &&
          a.font() == b.font();
      }
      case ElementKind::SectionHeader: {
        const auto& a = static_cast<const SectionHeaderElement&>(*left);
        const auto& b = static_cast<const SectionHeaderElement&>(*right);
        return a.title() == b.title() && a.subtitle() == b.subtitle() && a.font() == b.font();
      }
      case ElementKind::EmptyState: {
        const auto& a = static_cast<const EmptyStateElement&>(*left);
        const auto& b = static_cast<const EmptyStateElement&>(*right);
        return a.title() == b.title() && a.description() == b.description() &&
          a.font() == b.font() && a.icon() == b.icon();
      }
      case ElementKind::SplitView: {
        const auto& a = static_cast<const SplitViewElement&>(*left);
        const auto& b = static_cast<const SplitViewElement&>(*right);
        return a.axis() == b.axis() && a.fraction() == b.fraction();
      }
      case ElementKind::TabBar:
        return static_cast<const TabBarElement&>(*left).font() ==
          static_cast<const TabBarElement&>(*right).font();
      case ElementKind::Toolbar: {
        const auto& a = static_cast<const ToolbarElement&>(*left);
        const auto& b = static_cast<const ToolbarElement&>(*right);
        return a.font() == b.font() && a.height() == b.height();
      }
      case ElementKind::StatusBar: {
        const auto& a = static_cast<const StatusBarElement&>(*left);
        const auto& b = static_cast<const StatusBarElement&>(*right);
        return a.leftText() == b.leftText() && a.rightText() == b.rightText() &&
          a.font() == b.font();
      }
      case ElementKind::Scroll: {
        const auto& a = static_cast<const ScrollElement&>(*left);
        const auto& b = static_cast<const ScrollElement&>(*right);
        return a.preferredHeight() == b.preferredHeight();
      }
      default:
        // Leaves own no children, so their measure equivalence is exactly their value equality.
        return left->measureEquivalent(*right);
    }
  }

  void applyPropsIfNeeded(Node& node, const ElementPtr& next) {
    if (!needsCallbackRebind(next->kind()) && ownPropsEquivalent(node.element, next)) return;
    applyProps(next, *node.layer);
    ++appliedNodes;
  }

  /** Re-applies the model snapshot and callbacks for the simple control elements. */
  static void applyControlState(const Element& element, Layer& layer) {
    switch (element.kind()) {
      case ElementKind::TreeView: {
        const auto& tree = static_cast<const TreeViewElement&>(element);
        auto& view = static_cast<TreeView&>(layer);
        const auto paths = tree.paths();
        const TreeViewElement::PathHandler selected = tree.onSelected();
        const TreeViewElement::PathHandler activated = tree.onActivated();
        view.setOnSelection(paths != nullptr && selected ? [paths, selected](
          const int id, const std::string& label) {
          if (id >= 0 && static_cast<std::size_t>(id) < paths->size()) {
            selected((*paths)[static_cast<std::size_t>(id)], label);
          }
        } : std::function<void(int, const std::string&)>{});
        view.setOnActivated(paths != nullptr && activated ? [paths, activated](
          const int id, const std::string& label) {
          if (id >= 0 && static_cast<std::size_t>(id) < paths->size()) {
            activated((*paths)[static_cast<std::size_t>(id)], label);
          }
        } : std::function<void(int, const std::string&)>{});
        break;
      }
      case ElementKind::DateField: {
        const auto& field = static_cast<const DateFieldElement&>(element);
        static_cast<DateField&>(layer).setOnChanged(field.onChanged());
        break;
      }
      case ElementKind::TimeField: {
        const auto& field = static_cast<const TimeFieldElement&>(element);
        static_cast<TimeField&>(layer).setOnChanged(field.onChanged());
        break;
      }
      case ElementKind::DateTimeField: {
        const auto& field = static_cast<const DateTimeFieldElement&>(element);
        static_cast<DateTimeField&>(layer).setOnChanged(field.onChanged());
        break;
      }
      case ElementKind::DateRangeField: {
        const auto& field = static_cast<const DateRangeFieldElement&>(element);
        static_cast<DateRangeField&>(layer).setOnChanged(field.onChanged());
        break;
      }
      case ElementKind::TimeRangeField: {
        const auto& field = static_cast<const TimeRangeFieldElement&>(element);
        static_cast<TimeRangeField&>(layer).setOnChanged(field.onChanged());
        break;
      }
      case ElementKind::TimePicker: {
        const auto& picker = static_cast<const TimePickerElement&>(element);
        auto& view = static_cast<TimePicker&>(layer);
        view.setOnChanged(picker.onChanged());
        view.setOnCancel(picker.onCancel());
        view.setOnDone(picker.onDone());
        break;
      }
      case ElementKind::ColorPicker: {
        const auto& picker = static_cast<const ColorPickerElement&>(element);
        static_cast<ColorPicker&>(layer).setOnChanged(picker.onChanged());
        break;
      }
      case ElementKind::TableView: {
        const auto& table = static_cast<const TableViewElement&>(element);
        static_cast<TableView&>(layer).setOnRowActivated(table.onRowActivated());
        break;
      }
      case ElementKind::Select: {
        const auto& select = static_cast<const SelectElement&>(element);
        static_cast<Select&>(layer).setOnChanged(select.onChanged());
        break;
      }
      case ElementKind::ComboBox: {
        const auto& combo = static_cast<const ComboBoxElement&>(element);
        static_cast<ComboBox&>(layer).setOnChanged(combo.onChanged());
        break;
      }
      case ElementKind::SearchField: {
        const auto& search = static_cast<const SearchFieldElement&>(element);
        auto& view = static_cast<SearchField&>(layer);
        view.setOnChanged(search.onChanged());
        view.setOnSearch(search.onSearch());
        break;
      }
      case ElementKind::PasswordField: {
        const auto& password = static_cast<const PasswordFieldElement&>(element);
        static_cast<PasswordField&>(layer).setOnChanged(password.onChanged());
        break;
      }
      case ElementKind::NumberField: {
        const auto& field = static_cast<const NumberFieldElement&>(element);
        static_cast<NumberField&>(layer).setOnChanged(field.onChanged());
        break;
      }
      case ElementKind::TagInput: {
        const auto& tags = static_cast<const TagInputElement&>(element);
        static_cast<TagInput&>(layer).setOnChanged(tags.onChanged());
        break;
      }
      case ElementKind::ColorInput: {
        const auto& input = static_cast<const ColorInputElement&>(element);
        static_cast<ColorInput&>(layer).setOnChanged(input.onChanged());
        break;
      }
      case ElementKind::Expander: {
        const auto& expander = static_cast<const ExpanderElement&>(element);
        static_cast<Expander&>(layer).setOnChanged(expander.onChanged());
        break;
      }
      case ElementKind::SplitView: {
        const auto& split = static_cast<const SplitViewElement&>(element);
        static_cast<SplitView&>(layer).setOnFractionChanged(split.onFractionChanged());
        break;
      }
      case ElementKind::TabBar: {
        const auto& bar = static_cast<const TabBarElement&>(element);
        static_cast<TabBar&>(layer).setOnChanged(bar.onChanged());
        break;
      }
      case ElementKind::ToggleGroup: {
        const auto& group = static_cast<const ToggleGroupElement&>(element);
        static_cast<ToggleGroup&>(layer).setOnChanged(group.onChanged());
        break;
      }
      case ElementKind::RadioGroup: {
        const auto& group = static_cast<const RadioGroupElement&>(element);
        static_cast<RadioGroup&>(layer).setOnChanged(group.onChanged());
        break;
      }
      case ElementKind::CheckBoxGroup: {
        const auto& group = static_cast<const CheckBoxGroupElement&>(element);
        static_cast<CheckBoxGroup&>(layer).setOnChanged(group.onChanged());
        break;
      }
      case ElementKind::MultiSelect: {
        const auto& select = static_cast<const MultiSelectElement&>(element);
        static_cast<MultiSelect&>(layer).setOnChanged(select.onChanged());
        break;
      }
      case ElementKind::Link: {
        const auto& link = static_cast<const LinkElement&>(element);
        static_cast<Link&>(layer).setOnActivate(link.onActivate());
        break;
      }
      case ElementKind::Chip: {
        const auto& chip = static_cast<const ChipElement&>(element);
        static_cast<Chip&>(layer).setOnRemove(chip.onRemove());
        break;
      }
      case ElementKind::Rating: {
        const auto& rating = static_cast<const RatingElement&>(element);
        static_cast<Rating&>(layer).setOnRatingChanged(rating.onChanged());
        break;
      }
      case ElementKind::Stepper: {
        const auto& stepper = static_cast<const StepperElement&>(element);
        static_cast<Stepper&>(layer).setOnChanged(stepper.onChanged());
        break;
      }
      case ElementKind::RangeSlider: {
        const auto& range = static_cast<const RangeSliderElement&>(element);
        static_cast<RangeSlider&>(layer).setOnChanged(range.onChanged());
        break;
      }
      case ElementKind::Pagination: {
        const auto& pagination = static_cast<const PaginationElement&>(element);
        static_cast<Pagination&>(layer).setOnChanged(pagination.onChanged());
        break;
      }
      case ElementKind::ToggleButton: {
        const auto& button = static_cast<const ToggleButtonElement&>(element);
        static_cast<ToggleButton&>(layer).setOnChanged(button.onChanged());
        break;
      }
      case ElementKind::Breadcrumb: {
        const auto& crumb = static_cast<const BreadcrumbElement&>(element);
        static_cast<Breadcrumb&>(layer).setOnSelect(crumb.onSelect());
        break;
      }
      case ElementKind::Banner: {
        const auto& banner = static_cast<const BannerElement&>(element);
        auto& view = static_cast<Banner&>(layer);
        view.setAction(banner.actionLabel(), banner.onAction());
        view.setOnDismiss(banner.onDismiss());
        break;
      }
      case ElementKind::TitleBar: {
        const auto& bar = static_cast<const TitleBarElement&>(element);
        auto& view = static_cast<TitleBar&>(layer);
        view.setOnMinimize(bar.onMinimize());
        view.setOnMaximize(bar.onMaximize());
        view.setOnClose(bar.onClose());
        break;
      }
      case ElementKind::SplitButton: {
        const auto& button = static_cast<const SplitButtonElement&>(element);
        auto& view = static_cast<SplitButton&>(layer);
        view.setOnClick(button.onClick());
        view.setOnOption(button.onOption());
        break;
      }
      case ElementKind::DropZone: {
        const auto& zone = static_cast<const DropZoneElement&>(element);
        auto& view = static_cast<DropZone&>(layer);
        view.setOnFilesDropped(zone.onFilesDropped());
        view.setOnTextDropped(zone.onTextDropped());
        break;
      }
      case ElementKind::Calendar: {
        const auto& calendar = static_cast<const CalendarElement&>(element);
        auto& view = static_cast<Calendar&>(layer);
        view.setOnSelect(calendar.onSelect());
        view.setOnCancel(calendar.onCancel());
        break;
      }
      case ElementKind::StyledText: {
        const auto& text = static_cast<const StyledTextElement&>(element);
        static_cast<StyledText&>(layer).setOnLinkActivated(text.onLinkActivated());
        break;
      }
      case ElementKind::StepIndicator: {
        const auto& steps = static_cast<const StepIndicatorElement&>(element);
        static_cast<StepIndicator&>(layer).setOnChanged(steps.onChanged());
        break;
      }
      case ElementKind::CheckBox: {
        const auto& box = static_cast<const CheckBoxElement&>(element);
        auto& control = static_cast<CheckBox&>(layer);
        control.setChecked(box.checked());
        control.setOnChanged(box.onChanged());
        break;
      }
      case ElementKind::Toggle: {
        const auto& toggle = static_cast<const ToggleElement&>(element);
        auto& control = static_cast<Toggle&>(layer);
        control.setOn(toggle.on());
        control.setOnChanged(toggle.onChanged());
        break;
      }
      case ElementKind::RadioButton: {
        const auto& radio = static_cast<const RadioButtonElement&>(element);
        auto& control = static_cast<RadioButton&>(layer);
        control.setSelected(radio.selected());
        control.setOnSelected(radio.onSelected());
        break;
      }
      case ElementKind::ProgressBar: {
        const auto& bar = static_cast<const ProgressBarElement&>(element);
        static_cast<ProgressBar&>(layer).setValue(bar.value());
        break;
      }
      case ElementKind::SegmentedControl: {
        const auto& control = static_cast<const SegmentedControlElement&>(element);
        auto& view = static_cast<SegmentedControl&>(layer);
        view.setSegments(control.segments());
        view.setSelectedIndex(control.selected());
        view.setOnChanged(control.onChanged());
        break;
      }
      default:
        break;
    }
  }

  void mountSlot(Node& node, const size_t index, const ElementPtr& element,
      const std::function<void(std::shared_ptr<Layer>)>& setSlot) {
    if (element == nullptr) return;
    Node child = mountNode(element);
    if (child.layer == nullptr) return;
    setSlot(child.layer);
    node.children[index] = std::move(child);
  }

  void patchSlot(Node& slot, const ElementPtr& element,
      const std::function<void(std::shared_ptr<Layer>)>& setSlot) {
    if (element == nullptr) {
      if (slot.layer != nullptr) setSlot(nullptr);
      disposeNode(slot);
      slot = Node{};
      return;
    }
    if (slot.layer == nullptr || !sameIdentity(slot.element, element)) {
      if (slot.layer != nullptr) setSlot(nullptr);
      disposeNode(slot);
      Node fresh = mountNode(element);
      if (fresh.layer != nullptr) {
        setSlot(fresh.layer);
        slot = std::move(fresh);
      } else {
        slot = Node{};
      }
      return;
    }
    patchNode(slot, element);
    if (slot.layer == nullptr) {
      setSlot(nullptr);
      disposeNode(slot);
      slot = Node{};
    }
  }

  void mountContainerSlots(Node& node, const ElementPtr& element) {
    switch (element->kind()) {
      case ElementKind::Card: {
        const auto& card = static_cast<const CardElement&>(*element);
        node.children.resize(2);
        auto& view = static_cast<Card&>(*node.layer);
        mountSlot(node, 0, card.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        mountSlot(node, 1, card.headerAction(),
          [&view](std::shared_ptr<Layer> action) { view.setHeaderAction(std::move(action)); });
        break;
      }
      case ElementKind::Expander: {
        const auto& expander = static_cast<const ExpanderElement&>(*element);
        node.children.resize(1);
        auto& view = static_cast<Expander&>(*node.layer);
        mountSlot(node, 0, expander.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        break;
      }
      case ElementKind::FormField: {
        const auto& field = static_cast<const FormFieldElement&>(*element);
        node.children.resize(1);
        auto& view = static_cast<FormField&>(*node.layer);
        mountSlot(node, 0, field.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        break;
      }
      case ElementKind::SectionHeader: {
        const auto& header = static_cast<const SectionHeaderElement&>(*element);
        node.children.resize(1);
        auto& view = static_cast<SectionHeader&>(*node.layer);
        mountSlot(node, 0, header.action(),
          [&view](std::shared_ptr<Layer> action) { view.setAction(std::move(action)); });
        break;
      }
      case ElementKind::EmptyState: {
        const auto& empty = static_cast<const EmptyStateElement&>(*element);
        node.children.resize(2);
        auto& view = static_cast<EmptyState&>(*node.layer);
        mountSlot(node, 0, empty.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        mountSlot(node, 1, empty.action(),
          [&view](std::shared_ptr<Layer> action) { view.setAction(std::move(action)); });
        break;
      }
      case ElementKind::StatusBar: {
        const auto& bar = static_cast<const StatusBarElement&>(*element);
        auto& view = static_cast<StatusBar&>(*node.layer);
        node.children.resize(bar.leadingCount() + bar.trailingCount());
        for (size_t i = 0; i < bar.leadingCount(); ++i) {
          const ElementPtr& child = element->children()[i];
          if (child == nullptr) continue;
          node.children[i] = mountNode(child);
          view.addLeadingItem(node.children[i].layer);
        }
        for (size_t i = 0; i < bar.trailingCount(); ++i) {
          const ElementPtr& child = element->children()[bar.leadingCount() + i];
          if (child == nullptr) continue;
          node.children[bar.leadingCount() + i] = mountNode(child);
          view.addTrailingItem(node.children[bar.leadingCount() + i].layer);
        }
        break;
      }      case ElementKind::Toolbar: {
        const auto& toolbar = static_cast<const ToolbarElement&>(*element);
        auto& view = static_cast<Toolbar&>(*node.layer);
        view.setHeight(toolbar.height());
        node.children.resize(toolbar.items().size());
        for (size_t i = 0; i < toolbar.items().size(); ++i) {
          const ToolbarItem& item = toolbar.items()[i];
          if (item.separator) {
            view.addSeparator();
            continue;
          }
          if (item.element == nullptr) continue;
          node.children[i] = mountNode(item.element);
          view.addItem(node.children[i].layer, item.flexible, item.overflowLabel,
            item.overflowAction);
        }
        break;
      }      case ElementKind::TabBar: {
        const auto& bar = static_cast<const TabBarElement&>(*element);
        auto& view = static_cast<TabBar&>(*node.layer);
        view.setOnChanged({});
        node.children.resize(bar.tabs().size());
        for (size_t i = 0; i < bar.tabs().size(); ++i) {
          const TabItem& tab = bar.tabs()[i];
          if (tab.content != nullptr) {
            node.children[i] = mountNode(tab.content);
            view.addTab(tab.label, node.children[i].layer);
          } else {
            view.addTab(tab.label, nullptr);
          }
        }
        view.setActiveTab(bar.active());
        view.setOnChanged(bar.onChanged());
        break;
      }      case ElementKind::SplitView: {
        const auto& split = static_cast<const SplitViewElement&>(*element);
        node.children.resize(2);
        Node first;
        if (split.first() != nullptr) first = mountNode(split.first());
        Node second;
        if (split.second() != nullptr) second = mountNode(split.second());
        static_cast<SplitView&>(*node.layer).setPanes(first.layer, second.layer);
        node.children[0] = std::move(first);
        node.children[1] = std::move(second);
        break;
      }
      default:
        break;
    }
  }

  void patchContainerSlots(Node& node, const ElementPtr& element) {
    switch (element->kind()) {
      case ElementKind::Card: {
        const auto& card = static_cast<const CardElement&>(*element);
        node.children.resize(2);
        auto& view = static_cast<Card&>(*node.layer);
        patchSlot(node.children[0], card.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        patchSlot(node.children[1], card.headerAction(),
          [&view](std::shared_ptr<Layer> action) { view.setHeaderAction(std::move(action)); });
        break;
      }
      case ElementKind::Expander: {
        const auto& expander = static_cast<const ExpanderElement&>(*element);
        node.children.resize(1);
        auto& view = static_cast<Expander&>(*node.layer);
        patchSlot(node.children[0], expander.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        break;
      }
      case ElementKind::FormField: {
        const auto& field = static_cast<const FormFieldElement&>(*element);
        node.children.resize(1);
        auto& view = static_cast<FormField&>(*node.layer);
        patchSlot(node.children[0], field.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        break;
      }
      case ElementKind::SectionHeader: {
        const auto& header = static_cast<const SectionHeaderElement&>(*element);
        node.children.resize(1);
        auto& view = static_cast<SectionHeader&>(*node.layer);
        patchSlot(node.children[0], header.action(),
          [&view](std::shared_ptr<Layer> action) { view.setAction(std::move(action)); });
        break;
      }
      case ElementKind::EmptyState: {
        const auto& empty = static_cast<const EmptyStateElement&>(*element);
        node.children.resize(2);
        auto& view = static_cast<EmptyState&>(*node.layer);
        patchSlot(node.children[0], empty.content(),
          [&view](std::shared_ptr<Layer> content) { view.setContent(std::move(content)); });
        patchSlot(node.children[1], empty.action(),
          [&view](std::shared_ptr<Layer> action) { view.setAction(std::move(action)); });
        break;
      }
      case ElementKind::StatusBar: {
        const auto& bar = static_cast<const StatusBarElement&>(*element);
        auto& view = static_cast<StatusBar&>(*node.layer);
        const auto& old = static_cast<const StatusBarElement&>(*node.element);
        const size_t oldLeading = old.leadingCount();
        const size_t oldTrailing = old.trailingCount();
        std::vector<bool> consumed(oldLeading + oldTrailing, false);
        const size_t leading = bar.leadingCount();
        const size_t trailing = bar.trailingCount();
        std::vector<Node> nextChildren(leading + trailing);

        const auto patchGroup = [&](const size_t offset, const size_t count, const size_t oldOffset,
                                    const size_t oldCount) {
          for (size_t i = 0; i < count; ++i) {
            const ElementPtr& child = element->children()[offset + i];
            if (child == nullptr) continue;
            size_t match = oldCount;
            if (!child->key().empty()) {
              for (size_t oldIndex = 0; oldIndex < oldCount; ++oldIndex) {
                if (consumed[oldOffset + oldIndex]) continue;
                if (node.children[oldOffset + oldIndex].element != nullptr &&
                    node.children[oldOffset + oldIndex].element->key() == child->key()) {
                  match = oldIndex;
                  break;
                }
              }
            } else if (i < oldCount && !consumed[oldOffset + i]) {
              match = i;
            }
            if (match < oldCount && node.children[oldOffset + match].layer != nullptr &&
                sameIdentity(node.children[oldOffset + match].element, child)) {
              consumed[oldOffset + match] = true;
              nextChildren[offset + i] = std::move(node.children[oldOffset + match]);
              patchNode(nextChildren[offset + i], child);
            } else {
              if (match < oldCount) {
                consumed[oldOffset + match] = true;
                disposeNode(node.children[oldOffset + match]);
              }
              nextChildren[offset + i] = mountNode(child);
            }
          }
        };

        patchGroup(0, leading, 0, oldLeading);
        patchGroup(leading, trailing, oldLeading, oldTrailing);
        for (size_t i = 0; i < node.children.size() && i < consumed.size(); ++i) {
          if (!consumed[i]) disposeNode(node.children[i]);
        }
        view.clearItems();
        for (size_t i = 0; i < leading; ++i) {
          if (nextChildren[i].layer != nullptr) view.addLeadingItem(nextChildren[i].layer);
        }
        for (size_t i = 0; i < trailing; ++i) {
          if (nextChildren[leading + i].layer != nullptr) {
            view.addTrailingItem(nextChildren[leading + i].layer);
          }
        }
        node.children = std::move(nextChildren);
        break;
      }      case ElementKind::Toolbar: {
        const auto& toolbar = static_cast<const ToolbarElement&>(*element);
        auto& view = static_cast<Toolbar&>(*node.layer);
        const auto& oldItems = static_cast<const ToolbarElement&>(*node.element).items();
        std::vector<bool> consumed(oldItems.size(), false);
        std::vector<Node> nextChildren(toolbar.items().size());
        for (size_t i = 0; i < toolbar.items().size(); ++i) {
          const ToolbarItem& item = toolbar.items()[i];
          if (item.separator || item.element == nullptr) continue;
          size_t match = oldItems.size();
          if (!item.key.empty()) {
            for (size_t old = 0; old < oldItems.size(); ++old) {
              if (!consumed[old] && oldItems[old].key == item.key) {
                match = old;
                break;
              }
            }
          } else if (i < node.children.size() && !consumed[i]) {
            match = i;
          }
          if (match < oldItems.size() && node.children.size() == oldItems.size() &&
              node.children[match].layer != nullptr &&
              sameIdentity(node.children[match].element, item.element)) {
            consumed[match] = true;
            nextChildren[i] = std::move(node.children[match]);
            patchNode(nextChildren[i], item.element);
          } else {
            if (match < oldItems.size() && node.children.size() == oldItems.size()) {
              consumed[match] = true;
              disposeNode(node.children[match]);
            }
            nextChildren[i] = mountNode(item.element);
          }
        }
        for (size_t old = 0; old < node.children.size() && old < consumed.size(); ++old) {
          if (!consumed[old]) disposeNode(node.children[old]);
        }
        view.clear();
        view.setHeight(toolbar.height());
        for (size_t i = 0; i < toolbar.items().size(); ++i) {
          const ToolbarItem& item = toolbar.items()[i];
          if (item.separator) {
            view.addSeparator();
          } else if (nextChildren[i].layer != nullptr) {
            view.addItem(nextChildren[i].layer, item.flexible, item.overflowLabel,
              item.overflowAction);
          }
        }
        node.children = std::move(nextChildren);
        break;
      }      case ElementKind::TabBar: {
        const auto& bar = static_cast<const TabBarElement&>(*element);
        auto& view = static_cast<TabBar&>(*node.layer);
        const auto& oldTabs = static_cast<const TabBarElement&>(*node.element).tabs();
        std::vector<bool> consumed(oldTabs.size(), false);
        std::vector<Node> nextChildren(bar.tabs().size());
        view.setOnChanged({});
        for (size_t i = 0; i < bar.tabs().size(); ++i) {
          const TabItem& tab = bar.tabs()[i];
          if (tab.content == nullptr) continue;
          size_t match = oldTabs.size();
          if (!tab.key.empty()) {
            for (size_t old = 0; old < oldTabs.size(); ++old) {
              if (!consumed[old] && oldTabs[old].key == tab.key) {
                match = old;
                break;
              }
            }
          } else if (i < node.children.size() && !consumed[i]) {
            match = i;
          }
          if (match < oldTabs.size() && node.children.size() == oldTabs.size() &&
              node.children[match].layer != nullptr &&
              sameIdentity(node.children[match].element, tab.content)) {
            consumed[match] = true;
            nextChildren[i] = std::move(node.children[match]);
            patchNode(nextChildren[i], tab.content);
          } else {
            if (match < oldTabs.size() && node.children.size() == oldTabs.size()) {
              consumed[match] = true;
              disposeNode(node.children[match]);
            }
            nextChildren[i] = mountNode(tab.content);
          }
        }
        for (size_t old = 0; old < node.children.size() && old < consumed.size(); ++old) {
          if (!consumed[old]) disposeNode(node.children[old]);
        }
        view.clear();
        for (size_t i = 0; i < bar.tabs().size(); ++i) {
          view.addTab(bar.tabs()[i].label, nextChildren[i].layer);
        }
        node.children = std::move(nextChildren);
        view.setActiveTab(bar.active());
        view.setOnChanged(bar.onChanged());
        break;
      }      case ElementKind::SplitView: {
        const auto& split = static_cast<const SplitViewElement&>(*element);
        node.children.resize(2);
        patchSlot(node.children[0], split.first(), [](std::shared_ptr<Layer>) {});
        patchSlot(node.children[1], split.second(), [](std::shared_ptr<Layer>) {});
        auto& view = static_cast<SplitView&>(*node.layer);
        view.setPanes(node.children[0].layer, node.children[1].layer);
        break;
      }
      default:
        break;
    }
  }
  Node mountNode(const ElementPtr& element) {
    Node node;
    node.element = element;
    if (element->kind() == ElementKind::Component &&
        static_cast<const ComponentElement&>(*element).ownsComponent()) {
      const auto& componentElement = static_cast<const ComponentElement&>(*element);
      node.component = componentElement.component();
      node.componentContext = componentElement.context();
      if (node.componentContext == nullptr) {
        node.componentContext = std::make_shared<ComponentContext>(nullptr, nullptr);
      }
      const ElementPtr branch = node.componentContext->guard(
        [this, &node] { return node.component->build(*node.componentContext); });
      if (branch != nullptr) {
        node.children.push_back(mountNode(branch));
        node.layer = node.children.front().layer;
      }
      node.component->onMount(*node.componentContext);
      return node;
    }
    if (element->kind() == ElementKind::When || element->kind() == ElementKind::Component) {
      const ElementPtr branch = resolveBranch(element);
      if (branch == nullptr) {
        // Mount nothing; the parent container skips a null child layer.
        node.layer = nullptr;
        return node;
      }
      node.children.push_back(mountNode(branch));
      node.layer = node.children.front().layer;
      return node;
    }
    if (element->kind() == ElementKind::Props) {
      const auto& props = static_cast<const PropsElement&>(*element);
      const auto& child = element->children();
      if (child.empty() || child.front() == nullptr) {
        node.layer = emptyLayer();
      } else {
        node.children.push_back(mountNode(child.front()));
        node.layer = node.children.front().layer;
      }
      if (node.layer != nullptr) {
        applyProps(element, *node.layer);
        applyElementProps(props.props(), *node.layer);
        if (props.props().ref != nullptr) props.props().ref->set(node.layer);
        if (!props.props().sharedElementTag.empty()) {
          const auto previous = sharedElementBounds.find(props.props().sharedElementTag);
          if (previous != sharedElementBounds.end() && props.props().bounds.has_value() &&
              previous->second != *props.props().bounds) {
            const Rect target = *props.props().bounds;
            node.layer->setBounds(previous->second);
            if (sharedElementDuration.count() > 0 && !reduceMotion()) {
              node.layer->animateBounds(target, sharedElementDuration);
            } else {
              node.layer->setBounds(target);
            }
          }
          if (props.props().bounds.has_value()) {
            sharedElementBounds[props.props().sharedElementTag] = *props.props().bounds;
          }
        }
      }
      return node;
    }
    node.layer = createLayer(element);
    if (node.layer == nullptr) return node;
    applyProps(element, *node.layer);
    if (element->kind() == ElementKind::TextInput) {
      attachInput(node, std::static_pointer_cast<const TextInputElement>(element));
    }
    if (element->kind() == ElementKind::Button) {
      attachButtonEnabled(node, std::static_pointer_cast<const ButtonElement>(element));
    }
    applyControlState(*element, *node.layer);
    if (element->kind() == ElementKind::Stack || element->kind() == ElementKind::Grid) {
      for (const ElementPtr& child: element->children()) {
        Node childNode = mountNode(child);
        if (childNode.layer != nullptr) {
          node.layer->addChild(childNode.layer);
          node.children.push_back(std::move(childNode));
        }
      }
    } else if (element->kind() == ElementKind::Scroll && !element->children().empty()) {
      Node childNode = mountNode(element->children().front());
      if (childNode.layer != nullptr) {
        static_cast<ScrollView&>(*node.layer).setContent(childNode.layer);
        node.children.push_back(std::move(childNode));
      }
    } else if (isSlotContainer(element->kind())) {
      mountContainerSlots(node, element);
    } else if (element->kind() == ElementKind::Dock) {
      const auto& dock = static_cast<const DockElement&>(*element);
      const ElementPtr branches[4] = { dock.left(), dock.center(), dock.right(), dock.bottom() };
      const DockView::Region regions[4] = { DockView::Region::Left, DockView::Region::Center,
        DockView::Region::Right, DockView::Region::Bottom };
      const float sizes[4] = { dock.leftWidth(), 200.0f, dock.rightWidth(), dock.bottomHeight() };
      node.children.resize(4);
      auto& view = static_cast<DockView&>(*node.layer);
      for (size_t i = 0; i < 4; ++i) {
        if (branches[i] == nullptr) continue;
        Node child = mountNode(branches[i]);
        if (child.layer == nullptr) continue;
        view.setPanel(regions[i], child.layer, sizes[i]);
        node.children[i] = std::move(child);
      }
    }
    return node;
  }

  void patchNode(Node& node, const ElementPtr& next) {
    if (node.layer == nullptr || !sameIdentity(node.element, next)) {
      disposeNode(node);
      node = mountNode(next);
      return;
    }
    if (next->kind() == ElementKind::Component) {
      const auto& componentElement = static_cast<const ComponentElement&>(*next);
      if (componentElement.ownsComponent()) {
        if (node.component == componentElement.component() && node.componentContext != nullptr) {
          node.component->onUpdate(*node.componentContext);
          const ElementPtr branch = node.componentContext->guard(
            [this, &node] { return node.component->build(*node.componentContext); });
          patchBranch(node, branch);
        } else {
          disposeNode(node);
          node = mountNode(next);
          return;
        }
        node.element = next;
        return;
      }
      patchBranch(node, componentElement.built());
      node.element = next;
      return;
    }
    if (next->kind() == ElementKind::When) {
      patchBranch(node, resolveBranch(next));
      node.element = next;
      return;
    }
    if (next->kind() == ElementKind::Props) {
      const auto& children = next->children();
      patchBranch(node, children.empty() ? ElementPtr{} : children.front());
      if (node.layer != nullptr) {
        const ElementProps& props = static_cast<const PropsElement&>(*next).props();
        // The wrapper owns layer properties, not callbacks, so a value-equal props set needs no work.
        const auto& previous = static_cast<const PropsElement&>(*node.element).props();
        if (!detail::sameElementProps(previous, props)) {
          applyElementProps(props, *node.layer);
          ++appliedNodes;
        }
        if (props.ref != nullptr) props.ref->set(node.layer);
      }
      node.element = next;
      return;
    }
    if (next->kind() == ElementKind::Scroll) {
      const auto& children = next->children();
      if (children.empty()) {
        disposeNode(node);
        node = mountNode(next);
        return;
      }
      if (node.children.empty()) {
        node.children.push_back(mountNode(children.front()));
      } else {
        patchNode(node.children.front(), children.front());
      }
      // A content branch that resolves to null (a false `When`) must detach the old content, not
      // leave it scrolling under a viewport the new tree no longer describes.
      if (node.children.front().layer == nullptr) {
        static_cast<ScrollView&>(*node.layer).setContent(nullptr);
        for (Node& child: node.children) disposeNode(child);
        node.children.clear();
      } else if (node.layer != nullptr) {
        static_cast<ScrollView&>(*node.layer).setContent(node.children.front().layer);
      }
      applyProps(next, *node.layer);
      node.element = next;
      return;
    }
    if (next->kind() == ElementKind::Layer &&
        static_cast<const LayerElement&>(*next).layer() != node.layer) {
      // A hosted widget is structural: swapping the layer must replace the mounted node.
      disposeNode(node);
      node = mountNode(next);
      return;
    }
    if (next->kind() == ElementKind::Table) {
      applyProps(next, *node.layer);
      const auto& table = static_cast<const TableElement&>(*next);
      auto& layer = static_cast<Table&>(*node.layer);
      layer.setColumns(table.columns());
      layer.setGap(table.gap());
      layer.setPadding(table.padding());
      layer.setRows(table.rows());
      node.element = next;
      return;
    }

    if (isSlotContainer(next->kind())) {
      if (next->kind() == ElementKind::SplitView) {
        static_cast<SplitView&>(*node.layer).setOnFractionChanged({});
      }
      applyPropsIfNeeded(node, next);
      applyControlState(*next, *node.layer);
      patchContainerSlots(node, next);
      node.element = next;
      return;
    }
    if (next->kind() == ElementKind::Dock) {
      const auto& dock = static_cast<const DockElement&>(*next);
      auto& view = static_cast<DockView&>(*node.layer);
      const ElementPtr branches[4] = { dock.left(), dock.center(), dock.right(), dock.bottom() };
      const DockView::Region regions[4] = { DockView::Region::Left, DockView::Region::Center,
        DockView::Region::Right, DockView::Region::Bottom };
      const float sizes[4] = { dock.leftWidth(), 200.0f, dock.rightWidth(), dock.bottomHeight() };
      if (node.children.size() != 4) node.children.resize(4);
      for (size_t i = 0; i < 4; ++i) {
        Node& old = node.children[i];
        if (branches[i] == nullptr) {
          if (old.layer != nullptr) view.clearPanel(regions[i]);
          disposeNode(old);
          old = Node{};
          continue;
        }
        if (old.layer == nullptr || !sameIdentity(old.element, branches[i])) {
          Node fresh = mountNode(branches[i]);
          if (fresh.layer != nullptr) {
            view.setPanel(regions[i], fresh.layer, sizes[i]);
            disposeNode(old);
            old = std::move(fresh);
          } else {
            if (old.layer != nullptr) view.clearPanel(regions[i]);
            disposeNode(old);
            old = Node{};
          }
        } else {
          patchNode(old, branches[i]);
          // A branch that resolved to null (a false `When`) has no layer any more; the region must
          // drop its old panel instead of keeping a widget the new tree no longer describes.
          if (old.layer == nullptr) {
            view.clearPanel(regions[i]);
            disposeNode(old);
            old = Node{};
          }
        }
      }
      applyProps(next, *node.layer);
      node.element = next;
      return;
    }

    applyPropsIfNeeded(node, next);
    if (next->kind() == ElementKind::TextInput) {
      attachInput(node, std::static_pointer_cast<const TextInputElement>(next));
    }
    if (next->kind() == ElementKind::Button) {
      attachButtonEnabled(node, std::static_pointer_cast<const ButtonElement>(next));
    }
    if (next->kind() == ElementKind::IconButton) {
      const auto& button = static_cast<const IconButtonElement&>(*next);
      auto& control = static_cast<IconButton&>(*node.layer);
      control.setIcon(button.icon());
      control.setOnClick(button.onClick());
    }
    if (next->kind() == ElementKind::ColorButton) {
      const auto& button = static_cast<const ColorButtonElement&>(*next);
      auto& control = static_cast<ColorButton&>(*node.layer);
      control.setColor(button.color());
      control.setOnClick(button.onClick());
    }
    if (next->kind() == ElementKind::Spinner) {
      const auto& spinner = static_cast<const SpinnerElement&>(*next);
      auto& control = static_cast<Spinner&>(*node.layer);
      control.setColor(spinner.color());
      control.setSpinning(spinner.spinning());
    }
    applyControlState(*next, *node.layer);
    node.element = next;

    if (next->kind() != ElementKind::Stack && next->kind() != ElementKind::Grid) return;

    std::map<std::string, size_t> keyed;
    for (size_t i = 0; i < node.children.size(); ++i) {
      const std::string& key = node.children[i].element->key();
      if (!key.empty()) keyed.emplace(key, i);
    }
    std::vector<bool> consumed(node.children.size(), false);
    std::vector<Node> nextChildren;
    nextChildren.reserve(next->children().size());

    for (size_t i = 0; i < next->children().size(); ++i) {
      const ElementPtr& child = next->children()[i];
      size_t match = node.children.size();
      if (!child->key().empty()) {
        const auto it = keyed.find(child->key());
        if (it != keyed.end() && !consumed[it->second]) match = it->second;
      } else if (i < node.children.size() && !consumed[i] &&
                 node.children[i].element->key().empty() &&
                 node.children[i].element->kind() == child->kind()) {
        match = i;
      }

      if (match != node.children.size()) {
        consumed[match] = true;
        Node old = std::move(node.children[match]);
        patchNode(old, child);
        nextChildren.push_back(std::move(old));
      } else {
        Node fresh = mountNode(child);
        if (fresh.layer != nullptr) nextChildren.push_back(std::move(fresh));
      }
    }

    // Reordering is expressed by moving layers, not by rebuilding the tail: the layer identities
    // themselves are preserved, so focus, animations and draw caches survive a keyed move. A child
    // that keeps its position is never detached, which is what makes an insertion in the middle O(1)
    // for every trailing child instead of re-attaching all of them.
    std::vector<std::shared_ptr<Layer>> current = node.layer->children();
    std::vector<std::shared_ptr<Layer>> desired;
    desired.reserve(nextChildren.size());
    for (const Node& child: nextChildren) {
      if (child.layer != nullptr) desired.push_back(child.layer);
    }
    // Drop the children that no longer appear in the new tree.
    for (size_t i = 0; i < current.size();) {
      if (std::find(desired.begin(), desired.end(), current[i]) == desired.end()) {
        node.layer->removeChild(current[i]);
        current.erase(current.begin() + static_cast<std::ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
    // Place every desired child at its final index, moving an existing layer instead of recreating it.
    for (size_t i = 0; i < desired.size(); ++i) {
      if (i < current.size() && current[i] == desired[i]) continue;
      const auto found = std::find(current.begin(), current.end(), desired[i]);
      if (found != current.end()) {
        node.layer->removeChild(*found);
        current.erase(found);
      }
      node.layer->insertChild(i, desired[i]);
      current.insert(current.begin() + static_cast<std::ptrdiff_t>(i), desired[i]);
    }
    // Old nodes that no key matched are gone from the new tree: unmount their components now.
    for (size_t i = 0; i < node.children.size(); ++i) {
      if (!consumed[i]) disposeNode(node.children[i]);
    }
    node.children = std::move(nextChildren);
  }

  void reset() {
    disposeNode(root);
    root = Node{};
    mounted = false;
  }
};

Reconciler::Reconciler() : mImpl(std::make_unique<Impl>()) {
}

Reconciler::~Reconciler() = default;

void Reconciler::setSharedElementDuration(const std::chrono::milliseconds duration) {
  if (mImpl != nullptr) {
    mImpl->sharedElementDuration = std::max(duration, std::chrono::milliseconds{ 0 });
  }
}

std::chrono::milliseconds Reconciler::sharedElementDuration() const noexcept {
  return mImpl != nullptr ? mImpl->sharedElementDuration : std::chrono::milliseconds{ 0 };
}

std::shared_ptr<Layer> Reconciler::mount(ElementPtr root) {
  mImpl->appliedNodes = 0;
  mImpl->reset();
  if (root == nullptr) return nullptr;
  mImpl->root = mImpl->mountNode(root);
  mImpl->mounted = mImpl->root.layer != nullptr;
  return mImpl->root.layer;
}

void Reconciler::update(ElementPtr root) {
  mImpl->appliedNodes = 0;
  if (root == nullptr) {
    mImpl->reset();
    return;
  }
  if (!mImpl->mounted || mImpl->root.layer == nullptr) {
    mount(std::move(root));
    return;
  }
  if (!mImpl->sameIdentity(mImpl->root.element, root)) {
    mount(std::move(root));
    return;
  }
  mImpl->patchNode(mImpl->root, root);
}

std::shared_ptr<Layer> Reconciler::root() const {
  return mImpl->root.layer;
}

bool Reconciler::mounted() const noexcept {
  return mImpl->mounted;
}

uint32_t Reconciler::lastAppliedNodeCount() const noexcept {
  return mImpl != nullptr ? mImpl->appliedNodes : 0u;
}

} // namespace coral::ui
