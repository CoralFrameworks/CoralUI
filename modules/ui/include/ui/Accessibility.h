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

#ifndef CORAL_UI_ACCESSIBILITY_H
#define CORAL_UI_ACCESSIBILITY_H

#include <cstdint>
#include <optional>
#include <ui/Layer.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

/** Performs an action on the node at @p path; the bridge passes the value for SetValue. */
using AccessibilityActionInvoker = std::function<bool(std::string_view path,
  AccessibilityAction action, std::string_view value)>;

/** A snapshot of the layer tree in the vocabulary a platform accessibility bridge consumes. */
struct AccessibilityNode {
  std::string name;
  std::string description;
  /** The node value (text contents, slider reading); see `Layer::accessibleValue`. */
  std::string value;
  /** Child-index path from the root ("0/2/1"), used by platform bridges to address a node. */
  std::string path;
  AccessibilityRole role = AccessibilityRole::Generic;
  /** Actions derived from the role and from the node state, for the bridge to expose as patterns. */
  AccessibilityAction actions = AccessibilityAction::None;
  Rect bounds{};
  bool focused = false;
  bool focusable = false;
  bool selected = false;
  bool editable = false;
  /** False when this node or an ancestor is disabled: assistive technology must announce it. */
  bool enabled = true;
  /** Set when the node exposes a numeric range (slider/progress); see `AccessibleRange`. */
  std::optional<AccessibleRange> range;
  /** Set when the node is a scrollable viewport; see `AccessibleScroll`. */
  std::optional<AccessibleScroll> scroll;
  /** Set when the node is a grid/table container or one of its cells; see `AccessibleGrid`. */
  std::optional<AccessibleGrid> grid;
  /** Set when the node is an editable text control: the current selection, in UTF-16 units. */
  std::optional<AccessibleTextSelection> textSelection;
  /** Mirror of Layer::tabOrder(); children are emitted in this order. */
  int tabOrder = 0;
  std::vector<AccessibilityNode> children;
};

/** The cross-platform patterns a node exposes; a platform bridge maps each onto its own interface. */
enum class AccessibilityPattern : uint32_t {
  None = 0,
  Invoke = 1u << 0,
  Toggle = 1u << 1,
  Value = 1u << 2,
  SelectionItem = 1u << 3,
  Selection = 1u << 4,
  RangeValue = 1u << 5,
  Scroll = 1u << 6,
  Grid = 1u << 7,
  GridItem = 1u << 8,
  Text = 1u << 9,
};

[[nodiscard]] constexpr AccessibilityPattern operator|(const AccessibilityPattern left,
  const AccessibilityPattern right) noexcept {
  return static_cast<AccessibilityPattern>(
    static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}
[[nodiscard]] constexpr bool hasAccessibilityPattern(const AccessibilityPattern patterns,
  const AccessibilityPattern wanted) noexcept {
  return (static_cast<uint32_t>(patterns) & static_cast<uint32_t>(wanted)) != 0;
}

/**
 * The patterns @p node exposes, derived from its role, actions and structured data.
 *
 * This is the single place a bridge should ask: keeping it in the UI library means the Windows UIA
 * provider, and any future AT-SPI/NSAccessibility bridge, cannot drift from each other on what a
 * control offers.
 */
[[nodiscard]] AccessibilityPattern accessibilityPatterns(const AccessibilityNode& node);

/** Builds the tree from visible layers; unnamed generic layers are still traversed for their children. */
[[nodiscard]] AccessibilityNode accessibilityTree(const Layer& root,
  const Layer* focused = nullptr);

/**
 * Flattens the accessibility tree into focus order (children are already tabOrder-sorted).
 *
 * A platform AX bridge that needs a linear focus chain can consume this instead of re-sorting the
 * tree itself. The returned pointers refer into @p root.
 */
[[nodiscard]] Layer* accessibilityLayerAtPath(Layer& root, std::string_view path);

/** The path of @p target inside @p root, or an empty string when it is not part of the tree. */
[[nodiscard]] std::string accessibilityPathOf(const Layer& root, const Layer* target);

/** A state change a bridge may want to announce to assistive technology. */
enum class AccessibilityEvent {
  ValueChanged,
  ToggleChanged,
  Selected,
  /** The node's enabled state flipped; a bridge announces it as an `IsEnabled` property change. */
  EnabledChanged,
  /** A child was added or removed; a bridge announces it as a structure change. */
  ChildrenAdded,
  ChildrenRemoved,
};

/** Receives the events a window wants the platform bridge to announce. */
using AccessibilityNotifier = std::function<void(Layer& layer, AccessibilityEvent event)>;

class Window;

/**
 * Registers the notifier for one window; pass a null function to remove it.
 *
 * Routing by window instead of by thread matters when a thread owns several windows: a per-thread slot
 * meant the last window created received every event, with a node path that belonged to another window.
 * `notifyAccessibilityEvent` looks the notifier up through `Layer::window()`, so a control needs no
 * registration of its own.
 */
void setAccessibilityNotifier(const Window* window, AccessibilityNotifier notifier);

/** Controls call this when their value/selection actually changed; without a window it is a no-op. */
void notifyAccessibilityEvent(Layer& layer, AccessibilityEvent event);

[[nodiscard]] std::vector<const AccessibilityNode*> accessibilityFocusOrder(
  const AccessibilityNode& root);

} // namespace coral::ui

#endif // CORAL_UI_ACCESSIBILITY_H
