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

#include <ui/Accessibility.h>

#include <algorithm>
#include <map>
#include <mutex>

namespace coral::ui {

namespace {

/** The actions a role implies, before the node state narrows them. */
AccessibilityAction actionsForRole(const AccessibilityRole role) {
  switch (role) {
    case AccessibilityRole::Button:
      return AccessibilityAction::Invoke;
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::Radio:
      return AccessibilityAction::Toggle;
    case AccessibilityRole::Slider:
    case AccessibilityRole::TextField:
      return AccessibilityAction::SetValue;
    case AccessibilityRole::List:
    case AccessibilityRole::Tree:
    case AccessibilityRole::Table:
    case AccessibilityRole::ScrollView:
      return AccessibilityAction::Scroll;
    case AccessibilityRole::ListItem:
      return AccessibilityAction::Select;
    default:
      return AccessibilityAction::None;
  }
}

std::vector<const Layer*> accessibleChildren(const Layer& layer) {
  std::vector<const Layer*> children;
  children.reserve(layer.children().size());
  for (const auto& child: layer.children()) {
    if (child != nullptr && child->visible()) children.push_back(child.get());
  }
  std::stable_sort(children.begin(), children.end(), [](const Layer* left, const Layer* right) {
    return left->tabOrder() < right->tabOrder();
  });
  return children;
}

AccessibilityNode visit(const Layer& layer, const Layer* focused, const std::string& path) {
  AccessibilityNode node;
  node.name = layer.accessibleName();
  node.description = layer.accessibleDescription();
  node.value = layer.accessibleValue();
  node.role = layer.accessibleRole();
  node.actions = actionsForRole(node.role);
  node.bounds = layer.bounds();
  node.focused = &layer == focused;
  node.enabled = layer.inputEnabled();
  node.focusable = layer.focusable() && node.enabled;
  node.selected = layer.accessibleSelected();
  node.editable = layer.accessibleEditable();
  node.range = layer.accessibleRange();
  node.scroll = layer.accessibleScroll();
  node.grid = layer.accessibleGrid();
  node.textSelection = layer.accessibleTextSelection();
  node.path = path;
  node.tabOrder = layer.tabOrder();
  const std::vector<const Layer*> children = accessibleChildren(layer);
  for (std::size_t index = 0; index < children.size(); ++index) {
    const Layer* child = children[index];
    const std::string childPath = path.empty() ? std::to_string(index) : path + "/" + std::to_string(index);
    AccessibilityNode childNode = visit(*child, focused, childPath);
    if (childNode.name.empty() && childNode.role == AccessibilityRole::Generic &&
        childNode.children.empty()) {
      continue;
    }
    node.children.push_back(std::move(childNode));
  }
  // A grid container names its cells positionally: the platform grid pattern asks for (row, column)
  // and the bridge has no other way to know which child is which cell.
  if (node.grid.has_value() && node.grid->row < 0 && node.grid->rows > 0 && node.grid->columns > 0) {
    for (std::size_t index = 0; index < node.children.size(); ++index) {
      AccessibilityNode& child = node.children[index];
      if (child.grid.has_value()) continue;
      child.grid = AccessibleGrid{ node.grid->rows, node.grid->columns,
        static_cast<int>(index) / node.grid->columns, static_cast<int>(index) % node.grid->columns };
    }
  }
  return node;
}

} // namespace

AccessibilityPattern accessibilityPatterns(const AccessibilityNode& node) {
  AccessibilityPattern patterns = AccessibilityPattern::None;
  if (hasAccessibilityAction(node.actions, AccessibilityAction::Invoke)) {
    patterns = patterns | AccessibilityPattern::Invoke;
  }
  if (hasAccessibilityAction(node.actions, AccessibilityAction::Toggle)) {
    patterns = patterns | AccessibilityPattern::Toggle;
  }
  // A range control reports SetValue as well, but its pattern is RangeValue: a bridge that offers a
  // text field instead would show a slider as an editable string.
  if (node.range.has_value()) {
    patterns = patterns | AccessibilityPattern::RangeValue;
  } else if (hasAccessibilityAction(node.actions, AccessibilityAction::SetValue)) {
    patterns = patterns | AccessibilityPattern::Value;
  }
  if (hasAccessibilityAction(node.actions, AccessibilityAction::Select)) {
    patterns = patterns | AccessibilityPattern::SelectionItem;
  }
  if (node.role == AccessibilityRole::List || node.role == AccessibilityRole::Tree ||
      node.role == AccessibilityRole::Table) {
    patterns = patterns | AccessibilityPattern::Selection;
  }
  if (node.scroll.has_value()) {
    patterns = patterns | AccessibilityPattern::Scroll;
  }
  // An editable control gets the text pattern as well as Value: caret and selection are only reachable
  // through it, and a screen reader uses it to read by word/line.
  if (node.editable && node.role == AccessibilityRole::TextField) {
    patterns = patterns | AccessibilityPattern::Text;
  }
  if (node.grid.has_value()) {
    if (node.grid->row >= 0 && node.grid->column >= 0) {
      patterns = patterns | AccessibilityPattern::GridItem;
    } else if (node.grid->rows > 0 && node.grid->columns > 0) {
      patterns = patterns | AccessibilityPattern::Grid;
    }
  }
  return patterns;
}

AccessibilityNode accessibilityTree(const Layer& root, const Layer* focused) {
  // The root is the empty path, so a child of the root is "0" and resolving starts from the root.
  return visit(root, focused, "");
}

std::string accessibilityPathOf(const Layer& root, const Layer* target) {
  const auto visit = [&](auto&& self, const Layer& layer, const std::string& path) -> std::string {
    if (&layer == target) return path;
    const std::vector<const Layer*> children = accessibleChildren(layer);
    for (std::size_t index = 0; index < children.size(); ++index) {
      const std::string childPath = path.empty()
        ? std::to_string(index) : path + "/" + std::to_string(index);
      const std::string found = self(self, *children[index], childPath);
      if (!found.empty()) return found;
    }
    return {};
  };
  return visit(visit, root, "");
}

namespace {
std::mutex gNotifierMutex;
std::map<const Window*, AccessibilityNotifier> gNotifiers;
}

void setAccessibilityNotifier(const Window* window, AccessibilityNotifier notifier) {
  const std::lock_guard<std::mutex> lock(gNotifierMutex);
  if (!notifier) {
    gNotifiers.erase(window);
    return;
  }
  gNotifiers[window] = std::move(notifier);
}

void notifyAccessibilityEvent(Layer& layer, const AccessibilityEvent event) {
  AccessibilityNotifier notifier;
  {
    const std::lock_guard<std::mutex> lock(gNotifierMutex);
    const auto found = gNotifiers.find(layer.window());
    if (found != gNotifiers.end()) notifier = found->second;
  }
  // Copy the callback out of the registry and call it unlocked: a notifier may create or destroy
  // windows, and holding the registry lock across the platform call would deadlock there.
  if (notifier) notifier(layer, event);
}

Layer* accessibilityLayerAtPath(Layer& root, const std::string_view path) {
  Layer* current = &root;
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t slash = path.find('/', start);
    const std::string_view token = slash == std::string_view::npos
      ? path.substr(start) : path.substr(start, slash - start);
    if (token.empty()) return nullptr;
    const std::vector<const Layer*> children = accessibleChildren(*current);
    std::size_t index = 0;
    for (const char character : token) {
      if (character < '0' || character > '9') return nullptr;
      index = index * 10 + static_cast<std::size_t>(character - '0');
    }
    if (index >= children.size()) return nullptr;
    current = const_cast<Layer*>(children[index]);
    if (slash == std::string_view::npos) return current;
    start = slash + 1;
  }
  return current;
}

std::vector<const AccessibilityNode*> accessibilityFocusOrder(const AccessibilityNode& root) {
  std::vector<const AccessibilityNode*> result;
  const auto visitNode = [&](auto&& self, const AccessibilityNode& node) -> void {
    if (node.focusable) result.push_back(&node);
    for (const AccessibilityNode& child: node.children) self(self, child);
  };
  visitNode(visitNode, root);
  return result;
}

} // namespace coral::ui