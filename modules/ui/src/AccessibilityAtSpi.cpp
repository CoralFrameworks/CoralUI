#include <ui/AccessibilityAtSpi.h>

namespace coral::ui {

const char* atspiRoleName(const AccessibilityRole role) {
  switch (role) {
    case AccessibilityRole::Button: return "push button";
    case AccessibilityRole::CheckBox: return "check box";
    case AccessibilityRole::Radio: return "radio button";
    case AccessibilityRole::Slider: return "slider";
    case AccessibilityRole::ComboBox: return "combo box";
    case AccessibilityRole::ColorPicker: return "color chooser";
    case AccessibilityRole::SpinButton: return "spin button";
    case AccessibilityRole::Calendar: return "calendar";
    case AccessibilityRole::ProgressIndicator: return "progress bar";
    case AccessibilityRole::TabList: return "page tab list";
    case AccessibilityRole::TextField: return "text";
    case AccessibilityRole::Label: return "label";
    case AccessibilityRole::Link: return "link";
    case AccessibilityRole::List: return "list";
    case AccessibilityRole::Tree: return "tree";
    case AccessibilityRole::Menu: return "menu";
    case AccessibilityRole::Table: return "table";
    case AccessibilityRole::ListItem: return "list item";
    case AccessibilityRole::ScrollView: return "scroll pane";
    case AccessibilityRole::Dialog: return "dialog";
    case AccessibilityRole::Group: return "panel";
    default: return "unknown";
  }
}

std::vector<std::string> atspiInterfaces(const AccessibilityNode& node) {
  // Derived from the cross-platform pattern set, the same one the Windows provider uses: two bridges
  // that each re-derive "what does this control offer" from role/actions drift apart.
  const AccessibilityPattern patterns = accessibilityPatterns(node);
  std::vector<std::string> interfaces{ "org.a11y.atspi.Accessible", "org.a11y.atspi.Component" };
  if (hasAccessibilityPattern(patterns, AccessibilityPattern::Invoke) ||
      hasAccessibilityPattern(patterns, AccessibilityPattern::Toggle)) {
    interfaces.push_back("org.a11y.atspi.Action");
  }
  if (hasAccessibilityPattern(patterns, AccessibilityPattern::Value) ||
      hasAccessibilityPattern(patterns, AccessibilityPattern::RangeValue)) {
    interfaces.push_back("org.a11y.atspi.Value");
  }
  if (hasAccessibilityPattern(patterns, AccessibilityPattern::Text)) {
    interfaces.push_back("org.a11y.atspi.Text");
  }
  if (node.editable) {
    interfaces.push_back("org.a11y.atspi.EditableText");
  }
  if (hasAccessibilityPattern(patterns, AccessibilityPattern::SelectionItem) ||
      hasAccessibilityPattern(patterns, AccessibilityPattern::Selection)) {
    interfaces.push_back("org.a11y.atspi.Selection");
  }
  // AT-SPI has one Table interface for the container; cells are ordinary Accessible children with the
  // "table cell" role, which is why a GridItem adds no interface here.
  if (hasAccessibilityPattern(patterns, AccessibilityPattern::Grid)) {
    interfaces.push_back("org.a11y.atspi.Table");
  }
  return interfaces;
}

std::string atspiName(const AccessibilityNode& node) {
  if (!node.name.empty()) return node.name;
  if (node.role == AccessibilityRole::TextField && !node.value.empty()) return node.value;
  return atspiRoleName(node.role);
}

} // namespace coral::ui
