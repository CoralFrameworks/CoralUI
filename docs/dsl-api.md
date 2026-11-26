# DSL API contract (P2 -> P3)

This document is the first API-freeze pass. The stable contract is `ui/DSLCore.h`; `ui/DSL.h` is the umbrella that adds the experimental forwarders. It records which names are intended to
survive the prototype, which are convenience forwarders, and what a caller may rely on.

## Stable core

The following names are the stable DSL surface. They are allowed to default from `ThemeScope` and
must not require a caller to spell out a font, colour, or key for the common case.

### Theme

- `dsl::ThemeScope`
- `dsl::Density`
- `dsl::theme()`

`ThemeScope` is build-scoped and nests. Construct it inside the `ElementHost` build function. A
`Density::Compact` scope owns a theme copy with the spacing scale at 75%; typography and colours are
unchanged.
The scope is thread-local and is not a substitute for the window-owned `Theme`; overlays that outlive a
build keep their own theme copy.

### Design tokens

- `dsl::Tokens`
- `dsl::tokens()`

`tokens()` snapshots the active theme's spacing, radius, font sizes, colours, control-state colours,
and font handles. `Page`,
`Card`, and `Section` use `spacing.xs/sm/md/lg/xl` for their default padding and gaps; a component
must not hard-code those values.

### Layout

- `dsl::Column`, `dsl::Row`, `dsl::Grid`
- `dsl::Spacer`

`StackOptions` / `GridOptions` are the supported way to pass `gap`, `padding`, `align`, and a key.
Variadic children are the default; the vector overloads are for `ForEach` and other runtime child
lists.

### Typography

- `dsl::Text`
- `dsl::Display`
- `dsl::Heading`
- `dsl::Title`
- `dsl::Body`
- `dsl::Label`
- `dsl::Caption`

`TextOptions` is for an explicit font/colour/key. The role helpers read the active theme; `Display`
and `Heading` currently use the title face, while `Body`, `Label`, and `Caption` use the body face.
The theme's font-size scale is exposed through Tokens, pending dedicated faces on Theme.

### Default-styled controls

- `dsl::Button`
- `dsl::Panel`
- `dsl::Card`
- `dsl::Section`
- `dsl::Page`
- `dsl::FormField`

`Panel` is the untitled surface container; `Card`, `Section`, `Page` and `FormField` add a title or
control semantics. These own their default font, colour, padding and gap. A caller should not need `ElementProps` for
the common case.

### Styles

`DSLStyle.h` is the stable style companion. `defaultStyleSheet(theme)` provides:

- `button.primary`
- `button.secondary`
- `button.ghost`
- `button.danger`

Install the sheet with `Window::setStyleSheet`, then select a style through
`ButtonOptions::styleName`. The sheet is theme-derived; rebuild it when the window theme changes.
Hover, pressed, focused and disabled variants are resolved by the existing `StyleSheet` machinery.

### Dynamic content

- `dsl::ForEach(items, builder, StackOptions)`
- `dsl::When(condition, thenBranch, elseBranch, key)`

`ForEach` produces a vertical stack of keyed rows. The builder receives the item and index; explicit
keys are still required when the order can change.

### Custom content and escape hatches

- `dsl::Canvas(layer, CanvasOptions)` for a retained `Layer`
- `dsl::Component(build, key)` for a reusable DSL build function
- `dsl::Layer` for a widget that has no semantic wrapper yet

`Canvas` preserves the layer instance across rebuilds and accepts min width/height plus grow/shrink
options. A custom control made of DSL nodes uses `Component`; a control that needs
onMount/onUpdate/onUnmount, providers, or the error boundary should be a `coral::ui::Component`
mounted through `ComponentHost` or `ChildComponent`, where the owning context supplies the
invalidation route.

### Overlays

`DSLOverlay.h` is the stable companion header for `dsl::Overlay`; include it explicitly when the
overlay service is used outside the umbrella header.

- `dsl::Overlay`
- `dsl::DialogButton`

`dsl::Overlay` is a thin lifecycle facade over `OverlayHost`. `Dialog`/`Popup` accept a retained
`Layer`; `DialogElement`/`PopupElement` accept an `ElementPtr` and host it through the public
`ElementContentLayer` bridge. The bridge owns a Reconciler and forwards measure/layout to its root.

## Experimental forwarders

`ui/DSL.h` includes `DSLCore.h` and also exposes `using components::...` names for the long tail of controls. These are
**convenience forwarders, not stable API**. Their signatures are the component signatures, they do
not necessarily pick up theme defaults, and they may move once each control gets a proper typed
wrapper.

The current list includes Avatar, Badge, Banner, Breadcrumb, CheckBox, Chip, ColorPicker, ComboBox,
Divider, Dock, EmptyState, Expander, IconButton, Kbd, Link, ListView, Meter, MultiSelect,
NumberField, Pagination, PasswordField, ProgressBar, ProgressRing, PropertyRow, RadioButton,
RadioGroup, RangeSlider, Rating, RecyclingList, Scroll, SearchField, SegmentedControl, Select,
Skeleton, Slider, Spinner, SplitButton, SplitView, Stat, StatusBar, StatusIndicator, StepIndicator,
Stepper, StyledText, TabBar, Table, TagInput, TagList, TextInput, Timeline, Toggle, ToggleButton,
ToggleGroup, Toolbar, and WithProps.

The component-signature overloads of `Button`, `FormField`, and `SectionHeader` exist only so the
existing gallery can keep its explicit fonts during the transition. New code should use the
theme-scoped forms.

## Rules for adding a stable name

1. It must have a theme-aware default.
2. It must accept variadic children or a documented options struct, not a mixed positional signature.
3. It must not expose `ElementProps` for normal use.
4. It needs a compile test in `test_DSL.cpp`.
5. It needs a gallery usage and, once rendering is part of the contract, a golden/DPI check.

## Non-goals for this pass

- No hooks or fine-grained reactivity.
- No replacement for `Component`/`ElementRegistry` extension points.
- No stable promise for the experimental forwarders.
- No automatic accessibility metadata for custom `Canvas` content.
## Performance baseline

`ui_dsl_bench` measures the two costs a DSL page pays most often:

```sh
out/examples/ui/Release/ui_dsl_bench --iterations 100 --rows 160
```

It reports:

- Element build + reconcile + layout + paint per refresh
- `PopupElement` open/close per iteration

This is a baseline, not a CI timing gate: shared runners are too noisy for a hard budget. The CTest
entry runs a small iteration count so the path stays built and exercised.
## Golden coverage

`UiGolden.TheExtraScenesMatchTheirGoldenImages` includes two DSL scenes:

- `modules/ui/tests/dsl_panel.png` at 1.0 scale
- `modules/ui/tests/dsl_panel_dpi15.png` at 1.5 scale

Regenerate them with:

```sh
CORAL_UPDATE_GOLDEN=1 test_ui --gtest_filter=UiGolden.TheExtraScenesMatchTheirGoldenImages
```

The scene covers `dsl::Panel`, `dsl::Column`, `dsl::Swatch` and a text role through the full
Element -> Reconciler -> GPU path at two scales.
