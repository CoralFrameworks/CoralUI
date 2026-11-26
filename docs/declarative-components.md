# Declarative components: status and coverage

This page is the current, code-checked answer to "what can be written declaratively?" It supersedes the
early tutorial (`docs/tutorials/03-declarative-components.md` in the design archive), whose examples
predate most of the widget set. The audit below enumerated every public `Layer`-derived type in
`modules/ui/include/ui/` and every `ElementKind` in `modules/ui/include/ui/Element.h`, mounted each
first-class kind in the unit suite, and built the `ui_declarative_gallery` example against the result.

- **80 public Layer types have a first-class element wrapper.** They cover text, styled text, colour,
  the layout primitives, the form controls, the display leaves, the interactive widgets, the selection
  groups, the content/structured containers, the editor/date-time fields and the tree view.
- **9 public Layer types do not.** They are still usable from an element tree through the
  `components::Layer` escape hatch or a registered `ElementRegistry` type, but the layer owns its state
  and props, and a new layer instance replaces the node instead of patching it.
- **4 structural element kinds** (`When`, `Component`, `Props`, `Layer`) do not map to one layer type of
  their own.

The declarative slice is therefore usable for pages, but it is not yet a complete alternative to the
retained layer API. The gallery's last page (`Escape hatch`) shows the boundary on purpose.

## The pieces

| Piece | Header | Role |
|---|---|---|
| `Element` / `ElementPtr` | `<ui/Element.h>` | Immutable description of one node; rebuild it from state on every change |
| `Reconciler` | `<ui/Reconciler.h>` | `mount()` once, `update()` for later trees; matches `(kind, key)` and patches layers in place |
| `ElementHost` | `<ui/ElementHost.h>` | Owns a reconciler and a build function; observes properties and coalesces rebuilds into one posted task |
| `Component` / `ComponentHost` | `<ui/Component.h>` | Stateful unit with `build`/`onMount`/`onUpdate`/`onUnmount`, an error boundary and `provide`/`find` DI |
| `State<T>` | `<ui/State.h>` | Component-owned value with observers; equal writes are ignored |
| `ElementRegistry` | `<ui/ElementRegistry.h>` | Maps a type name to an element builder, with optional defaults |
| `ElementSpec` | `<ui/ElementSpec.h>` | Parses the flat `type#key name=value ...` text form (v0) |
| `LayerRef` | `<ui/Ref.h>` | Stable handle to the mounted layer, for focus/scroll/measure from a component |
| `StyledText` / `TextDocument` | `<ui/StyledText.h>` | Value-typed rich text with spans, paragraph style and link hit-testing |
| `OverlayHost` | `<ui/OverlayHost.h>` | Per-window lifecycle for Dialog/Popup/Toast/Tooltip/Menu overlays |

The flow is one direction:

```text
state write -> build a fresh Element tree -> Reconciler.update -> patch the retained Layer tree
```

Layer identity survives a rebuild, so focus, scroll position, animations and recordings are preserved.
Keyed children (`Element::key`) keep their identity across reorders; an empty key means positional
identity within the parent.

## Traditional vs declarative

The same page in both styles, using the public API. Neither is deprecated.

### Traditional (retained layers)

```cpp
using namespace coral::ui;

auto page = std::make_shared<Layer>();
page->setBackgroundColorRole(ThemeColorRole::Background);

auto title = std::make_shared<TextLayer>("Library", theme.title, theme.text);
title->setBounds(Rect::MakeXYWH(24.0f, 20.0f, 320.0f, 32.0f));
page->addChild(title);

auto search = std::make_shared<TextInput>("Search", theme.body);
search->setBounds(Rect::MakeXYWH(24.0f, 64.0f, 320.0f, 32.0f));
search->observeChanged([&](const std::string& value) { model.setQuery(value); });
page->addChild(search);

auto list = std::make_shared<ListView>(theme.body, 28.0f);
list->setRows(model.items());
list->setBounds(Rect::MakeXYWH(24.0f, 108.0f, 320.0f, 400.0f));
page->addChild(list);

page->layout();
window->setRoot(page);
```

You own the rectangles and the update calls. This is the right path for a custom-drawn canvas, a
timeline, a waveform, a large virtualized surface, or a container that needs exact measure/layout
control.

### Declarative (elements + host)

```cpp
using namespace coral::ui;

State<int> selected{0};                 // or a view-model property
ElementHost host(*window, [&] {
  const Theme& theme = window->theme();
  std::vector<ElementPtr> rows;
  for (size_t i = 0; i < model.items().size(); ++i) {
    const auto& item = model.items()[i];
    rows.push_back(components::WithProps(
      components::Button(item, theme.body, [&, i] { selected.set(static_cast<int>(i)); },
        "row-" + std::to_string(i)),
      [] { ElementProps p; p.layout.basis = 32.0f; p.layout.shrink = 0.0f; return p; }(),
      "row-" + std::to_string(i) + ".wrap"));
  }
  return std::make_shared<StackElement>(Axis::Column, std::vector<ElementPtr>{
    std::make_shared<TextElement>("Library", theme.title, theme.text, "title"),
    components::TextInput(model.query(), theme.body,
      [&](const std::string& value) { model.setQuery(value); }, "search"),
    components::When(model.showList(),
      std::make_shared<StackElement>(Axis::Column, std::move(rows), "rows", 6.0f,
        Insets{}.all(24.0f), Align::Stretch),
      std::make_shared<TextElement>("No results", theme.body, theme.textMuted, "empty"),
      "list-branch"),
  }, "page", 12.0f, Insets{}.all(24.0f), Align::Stretch);
});
auto binding = host.bind(selected.property());   // keep the observation alive
host.mount();
window->setRoot(host.root());
```

`host` coalesces every `State` write in one UI turn into one reconcile. Call `window->pump()` in a
headless test to run the posted rebuild; a native window does it in its loop.

Use declarative when the UI is a function of a model: forms, lists, settings pages, dialogs. Use layers
inside an element tree (`components::Layer`) when a control has no wrapper yet, or when a widget needs
its own drawing.

## First-class coverage

These are the `ElementKind` values that create a layer. The wrapper factories are in
`namespace coral::ui::components`.

| Element kind | Wrapper / factory | Layer it creates | Notes |
|---|---|---|---|
| `Text` | `TextElement` | `TextLayer` | font and colour are per element; rebuild on theme change |
| `Color` | `ColorElement` | `ColorLayer` | solid fill |
| `Stack` | `StackElement` constructor | `Flex` | axis, gap, padding, cross align |
| `Grid` | `GridElement` / `components::Grid` | `Grid` | equal columns |
| `Dock` | `DockElement` / `components::Dock` | `DockView` | left/centre/right/bottom regions |
| `Scroll` | `ScrollElement` / `components::Scroll` | `ScrollView` | one content child, optional preferred height |
| `Button` | `ButtonElement` / `components::Button` | `Button` | click, enabled, live enabled property |
| `IconButton` | `IconButtonElement` / `components::IconButton` | `IconButton` | icon + accessible name |
| `ColorButton` | `ColorButtonElement` / `components::ColorButton` | `ColorButton` | swatch + accessible name |
| `Spinner` | `SpinnerElement` / `components::Spinner` | `Spinner` | spinning drives the frame loop |
| `Slider` | `SliderElement` / `components::Slider` | `Slider` | value + edit begin/end callbacks |
| `TextInput` | `TextInputElement` / `components::TextInput` | `TextInput` | `onChanged`; caret is preserved by the layer |
| `CheckBox` | `CheckBoxElement` / `components::CheckBox` | `CheckBox` | checked snapshot + `onChanged` |
| `Toggle` | `ToggleElement` / `components::Toggle` | `Toggle` | on snapshot + `onChanged` |
| `RadioButton` | `RadioButtonElement` / `components::RadioButton` | `RadioButton` | the owner keeps the exclusive selection |
| `ProgressBar` | `ProgressBarElement` / `components::ProgressBar` | `ProgressBar` | 0..1 |
| `SegmentedControl` | `SegmentedControlElement` / `components::SegmentedControl` | `SegmentedControl` | selected index + `onChanged` |
| `Divider` | `DividerElement` / `components::Divider` | `Divider` | orientation, thickness, inset, optional colour |
| `Kbd` | `KbdElement` / `components::Kbd` | `Kbd` | label + font |
| `Badge` | `BadgeElement` / `components::Badge` | `Badge` | text, dot, optional icon |
| `Icon` | `IconElement` / `components::Icon` | `Icon` | image, size, optional tint |
| `Avatar` | `AvatarElement` / `components::Avatar` | `Avatar` | initials, image, size, status |
| `AvatarGroup` | `AvatarGroupElement` / `components::AvatarGroup` | `AvatarGroup` | initials, max visible, size; font is constructor-only |
| `Skeleton` | `SkeletonElement` / `components::Skeleton` | `Skeleton` | size, lines, shape, animation |
| `Stat` | `StatElement` / `components::Stat` | `Stat` | label, value, caption, trend |
| `Meter` | `MeterElement` / `components::Meter` | `Meter` | range + value |
| `ProgressRing` | `ProgressRingElement` / `components::ProgressRing` | `ProgressRing` | size, value, thickness, indeterminate |
| `StatusIndicator` | `StatusIndicatorElement` / `components::StatusIndicator` | `StatusIndicator` | label + `StatusKind` |
| `StepIndicator` | `StepIndicatorElement` / `components::StepIndicator` | `StepIndicator` | steps, current, `onChanged` |
| `PropertyRow` | `PropertyRowElement` / `components::PropertyRow` | `PropertyRow` | label, value, help, label width |
| `TagList` | `TagListElement` / `components::TagList` | `TagList` | tags + max visible; font is constructor-only |
| `Link` | `LinkElement` / `components::Link` | `Link` | text + `onActivate` |
| `Chip` | `ChipElement` / `components::Chip` | `Chip` | variant, icon, removable, selected, `onRemove` |
| `Rating` | `RatingElement` / `components::Rating` | `Rating` | stars, rating, half steps, read-only, `onChanged` |
| `Stepper` | `StepperElement` / `components::Stepper` | `Stepper` | range, value, step, `onChanged` |
| `RangeSlider` | `RangeSliderElement` / `components::RangeSlider` | `RangeSlider` | range, low/high, step, `onChanged` |
| `Pagination` | `PaginationElement` / `components::Pagination` | `Pagination` | page count, page, `onChanged` |
| `ToggleButton` | `ToggleButtonElement` / `components::ToggleButton` | `ToggleButton` | selected + `onChanged` |
| `Breadcrumb` | `BreadcrumbElement` / `components::Breadcrumb` | `Breadcrumb` | items, separator, current index, `onSelect` |
| `Banner` | `BannerElement` / `components::Banner` | `Banner` | severity, action, dismiss, icon, `onDismiss` |
| `TitleBar` | `TitleBarElement` / `components::TitleBar` | `TitleBar` | title, show flags, window callbacks; font is constructor-only |
| `SplitButton` | `SplitButtonElement` / `components::SplitButton` | `SplitButton` | options, `onClick`, `onOption` |
| `DropZone` | `DropZoneElement` / `components::DropZone` | `DropZone` | file/text drop callbacks |
| `Calendar` | `CalendarElement` / `components::Calendar` | `Calendar` | date, first weekday, `onSelect`/`onCancel`; month names and colours stay on the layer |
| `StyledText` | `StyledTextElement` / `components::StyledText` | `StyledText` | `TextDocument` spans, paragraph style, selection, link `onLinkActivated` |
| `ToggleGroup` | `ToggleGroupElement` / `components::ToggleGroup` | `ToggleGroup` | options, selected indices, `onChanged` |
| `RadioGroup` | `RadioGroupElement` / `components::RadioGroup` | `RadioGroup` | options, exclusive index, `onChanged` |
| `CheckBoxGroup` | `CheckBoxGroupElement` / `components::CheckBoxGroup` | `CheckBoxGroup` | options, selected indices, `onChanged` |
| `MultiSelect` | `MultiSelectElement` / `components::MultiSelect` | `MultiSelect` | options, selected indices, placeholder, `onChanged` |
| `Card` | `CardElement` / `components::Card` | `Card` | title + content/header-action slots |
| `Expander` | `ExpanderElement` / `components::Expander` | `Expander` | title, content slot, expanded + `onChanged` |
| `FormField` | `FormFieldElement` / `components::FormField` | `FormField` | label, help/error, content slot |
| `SectionHeader` | `SectionHeaderElement` / `components::SectionHeader` | `SectionHeader` | title/subtitle + action slot |
| `EmptyState` | `EmptyStateElement` / `components::EmptyState` | `EmptyState` | title/description, content/action slots, icon |
| `SplitView` | `SplitViewElement` / `components::SplitView` | `SplitView` | two panes + fraction + `onFractionChanged` |
| `TabBar` | `TabBarElement` / `components::TabBar` | `TabBar` | `TabItem` labels/content, active index, `onChanged` |
| `Toolbar` | `ToolbarElement` / `components::Toolbar` | `Toolbar` | item elements, flexible/separator/overflow metadata |
| `StatusBar` | `StatusBarElement` / `components::StatusBar` | `StatusBar` | left/right text + leading/trailing items |
| `Select` | `SelectElement` / `components::Select` | `Select` | options, selected index, placeholder, `onChanged` |
| `ComboBox` | `ComboBoxElement` / `components::ComboBox` | `ComboBox` | options, text, selected index, `onChanged` |
| `SearchField` | `SearchFieldElement` / `components::SearchField` | `SearchField` | text, placeholder, `onChanged`/`onSearch` |
| `PasswordField` | `PasswordFieldElement` / `components::PasswordField` | `PasswordField` | masked text, max length, `onChanged` |
| `NumberField` | `NumberFieldElement` / `components::NumberField` | `NumberField` | range, value, step, decimals, `onChanged` |
| `TagInput` | `TagInputElement` / `components::TagInput` | `TagInput` | tokens, placeholder, `onChanged` |
| `ColorInput` | `ColorInputElement` / `components::ColorInput` | `ColorInput` | colour + `onChanged` |
| `Timeline` | `TimelineElement` / `components::Timeline` | `Timeline` | entry snapshots + font |
| `ColorPicker` | `ColorPickerElement` / `components::ColorPicker` | `ColorPicker` | palette, mode, colour, alpha, selection, `onChanged` |
| `CandidateListView` | `CandidateListViewElement` / `components::CandidateListView` | `CandidateListView` | candidates + selected index |
| `TableView` | `TableViewElement` / `components::TableView` | `TableView` | columns, live cell layers, selected row, `onRowActivated` |
| `DateField` | `DateFieldElement` / `components::DateField` | `DateField` | date + `onChanged` |
| `TimeField` | `TimeFieldElement` / `components::TimeField` | `TimeField` | hour/minute, clock mode, `onChanged` |
| `DateTimeField` | `DateTimeFieldElement` / `components::DateTimeField` | `DateTimeField` | date + time, `onChanged` |
| `DateRangeField` | `DateRangeFieldElement` / `components::DateRangeField` | `DateRangeField` | start/end, `onChanged` |
| `TimeRangeField` | `TimeRangeFieldElement` / `components::TimeRangeField` | `TimeRangeField` | start/end time, `onChanged` |
| `TimePicker` | `TimePickerElement` / `components::TimePicker` | `TimePicker` | time, clock mode, `onChanged`/cancel/done |
| `TreeView` | `TreeViewElement` / `components::TreeView` | `TreeView` | `TreeNodeSpec` roots, path selection, expand/collapse, `onSelected`/`onActivated` |
| `Table` | `TableElement` / `components::Table` | `Table` | cells are live layers, not elements |
| `ListView` | `ListViewElement` / `components::ListView` | `ListView` | string rows, virtualized |
| `RecyclingList` | `RecyclingListElement` / `components::RecyclingList` | `RecyclingListView` | keyed rows built from elements |
| `ContentView` | `ContentViewElement` / `components::ContentView` | `ContentView` | zero-copy `ContentHost` preview |
| `Layer` | `LayerElement` / `components::Layer` | the hosted layer | escape hatch, one place at a time |
| `When` | `WhenElement` / `components::When` | the chosen branch | no extra layer for the branch node |
| `Component` | `ComponentElement` / `components::Component` / `ChildComponent` | the built subtree | lifecycle + error boundary |
| `Props` | `PropsElement` / `components::WithProps` | the child's layer | transparent wrapper; adds no node |

`components::ForEach` is a mapping helper, not a node; it returns a `std::vector<ElementPtr>` for a
container's child list.

### StyledText (R1 + R2)

`StyledText` is the document-shaped rich-text control; `RichTextLayer` stays the low-level renderer.
A `TextDocument` is a value:

```cpp
TextDocument document;
document.spans.push_back(TextSpan{ "Coral", TextStyle{ font, theme.text, false }, {} });
document.spans.push_back(TextSpan{ " docs", TextStyle{ font, theme.accent, true }, "docs" });
document.paragraph.maxLines = 2;
document.paragraph.ellipsis = true;
components::StyledText(document, [](std::string_view id) { open(id); }, "intro");
```

- **R1** renders spans, paragraph style (`align`, `lineHeight`, `maxLines`, `ellipsis`, `selectable`,
  `wrapWidth`) and selection through the child `RichTextLayer`.
- **R2** maps link spans to byte ranges, changes the cursor over a link, and calls
  `onLinkActivated(id)`. Styling a link (colour/underline) is the document's choice.
- Editing is out of scope; it would be a separate control above this document API.
### Content containers

`Card`, `Expander`, `FormField`, `SectionHeader` and `EmptyState` mount their child elements through
one shared slot helper in `Reconciler`: `mountSlot` / `patchSlot` attach the child to the widget's
setter (`setContent`, `setHeaderAction`, `setAction`), patch it in place when `(kind, key)` matches,
and detach it before the widget is disposed. The slots are fixed-order:

| Kind | Slot order |
|---|---|
| `Card` | content, header action |
| `Expander` | content |
| `FormField` | content |
| `SectionHeader` | action |
| `EmptyState` | content, action |

`Expander` is the only one with a callback; `onChanged(bool)` is rebound in `applyControlState`.
### Editor fields

`Select`, `ComboBox`, `SearchField`, `PasswordField`, `NumberField`, `TagInput` and `ColorInput`
patch their value props in place and rebind callbacks through `applyControlState`. Their internal
popups (the select/combobox menu, the colour panel) remain owned by the widget; the element only owns
the value and the callback.
### Overlays: `OverlayHost`

Transient surfaces are deliberately **not** `ElementKind`s: they do not participate in page layout, they
stack above it, and they have their own dismissal/focus lifecycle. `OverlayHost` is the single host for
them:

```cpp
OverlayHost overlays(*window);

auto dialog = std::make_shared<Dialog>(theme.title, "Delete build?");
dialog->setContent(components::Layer(...)->...);  // any layer/content
const uint64_t id = overlays.show(components::Layer(dialog),
  OverlayOptions{ .modal = true });
overlays.update(id, nextDialogTree);   // same root layer: patches in place
overlays.dismiss(id);
```

- Each entry owns its own `Reconciler`, so overlay content is still an Element subtree.
- `Window::pushOverlay` owns stacking, light dismiss, Escape, the modal scrim and focus restore.
- `OverlayOptions.anchor`/`preferred` position Popup/Menu/Tooltip; a centred Dialog gets the full area.
- `OverlayLayer` remains available as the low-level layer when a caller needs the whole host directly.

The Dialog/Popup/Toast/Tooltip/Menu **controls** stay in the escape-hatch group; `OverlayHost` is the
supported declarative entry point for using them.
### Not first-class yet (9)

These are complete `Layer` implementations with no element wrapper. `components::Layer` hosts them:

- Display and decoration: `Toast`, `ToastStack`, `Tooltip`.
- Surfaces, menus and chrome: `Dialog`, `Menu`, `MenuBar`, `OverlayLayer`, `Popup`.
- Platform content: `NativeViewLayer`.

The escape hatch is a real path, not a workaround to hide:

```cpp
const Theme& theme = window->theme();

// Construct the widget once, outside the build function; the element just hosts the layer.
auto card = std::make_shared<Card>(theme.section, "Build output");
card->setContent(std::make_shared<TextLayer>("3 problems", theme.body, theme.text));

// In the tree (the same instance is hosted on every rebuild):
components::Layer(card, "card-host");
```

A hosted layer keeps its own state, theme and layout. Swapping to a different layer instance replaces
the element node; mutating the same instance is not reconciled into props.

## ElementSpec v0

`parseElementSpec("CheckBox#spec_checkbox text=\"Parsed from a flat spec\" checked=true")` produces an
ordinary element tree. The grammar is `type ("#" key)? (" " name "=" value)*`; keys are identifiers
(`[A-Za-z0-9_]`), and the built-in properties are `background`, `radius`, `opacity`, `width`, `height`,
`text`, `checked` and `spinning`. Everything else (children, fonts, callbacks) is outside v0. Unknown
types resolve through `ElementRegistry::shared()` (or the registry passed in). The parser returns null
instead of throwing, so a caller can report its own error.

## Adding a first-class wrapper

A new kind touches a small, fixed set of places:

1. `ElementKind` and the element class in `include/ui/Element.h` (plus a `components::` factory).
2. `Reconciler::Impl::createLayer` — construct the layer.
3. `Reconciler::Impl::applyProps` — apply value props; `applyControlState` / `needsCallbackRebind` if it
   owns callbacks.
4. `mountNode` / `patchNode` — only for kinds with non-element children (a table's cells, a dock's
   regions) or structural behavior.
5. `measureEquivalent` and `detail::sameElementProps` inputs, so the reconciler skips no-op work.
6. A mount/patch test in `modules/ui/tests/test_Reconciler.cpp` (`MountsEveryLayerProducingElementKind`
   is the checklist).
7. The coverage table in this file and, if it is part of the gallery, a page or card in
   `examples/ui/ui_declarative_gallery.cpp`.
