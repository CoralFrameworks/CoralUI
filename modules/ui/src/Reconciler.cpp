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

#include <ui/Button.h>
#include <ui/CheckBox.h>
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
      case ElementKind::Layer:
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
