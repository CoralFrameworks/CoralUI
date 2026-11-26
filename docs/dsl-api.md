# DSL API contract (P2 -> P3)

This document is the first API-freeze pass. The stable contract is `ui/DSLCore.h`; `ui/DSL.h` is the umbrella that adds the experimental forwarders. It records which names are intended to
survive the prototype, which are convenience forwarders, and what a caller may rely on.

## Stable core

The following names are the stable DSL surface. They are allowed to default from `ThemeScope` and
must not require a caller to spell out a font, colour, or key for the common case.

### Theme

- `dsl::ThemeScope`
- `dsl::theme()`

`ThemeScope` is build-scoped and nests. Construct it inside the `ElementHost` build function. The
scope is thread-local and is not a substitute for the window-owned `Theme`; overlays that outlive a
build keep their own theme copy.

### Design tokens

- `dsl::Tokens`
- `dsl::tokens()`

`tokens()` snapshots the active theme's spacing, radius, font sizes, colours and font handles. `Page`,
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
- `dsl::Title`
- `dsl::Caption`

`TextOptions` is for an explicit font/colour/key. `Title` and `Caption` read the active theme.

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

### Dynamic content

- `dsl::ForEach(items, builder, StackOptions)`
- `dsl::When(condition, thenBranch, elseBranch, key)`

`ForEach` produces a vertical stack of keyed rows. The builder receives the item and index; explicit
keys are still required when the order can change.

### Custom content and escape hatches

- `dsl::Canvas(layer, minHeight, key)` for a retained `Layer`
- `dsl::Layer` for a widget that has no semantic wrapper yet

`Canvas` preserves the layer instance across rebuilds. A custom control that needs a DSL subtree
should be a `Component` whose build returns ordinary DSL nodes.

### Overlays

`DSLOverlay.h` is the stable companion header for `dsl::Overlay`; include it explicitly when the
overlay service is used outside the umbrella header.

- `dsl::Overlay`
- `dsl::DialogButton`

`dsl::Overlay` is a thin lifecycle facade over `OverlayHost`. Dialog/Popup content is currently a
retained `Layer`; an Element-content overload is a P3 item.

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
