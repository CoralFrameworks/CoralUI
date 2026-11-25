# HIG metrics

Default sizes in the cross-platform widgets. They are logical pixels, independent of the backing
scale, and can be overridden per control or by the theme/style sheet.

## Controls

| Control | Default height | Padding / gap | Notes |
|---|---:|---|---|
| `Button` | 32 | 12 horizontal | compact 28 |
| `ToggleButton` | 32 | 16 horizontal | selected state |
| `SearchField` | 32 | 8 | |
| `TextInput` | 32 | 9 | multiline grows to bounds |
| `PasswordField` | 32 | 8 | masked bullets |
| `NumberField` | 32 | 8 | |
| `DateField` / `TimeField` | 32 | 8 | |
| `Select` / `ComboBox` | 32 | 8 | |
| `MultiSelect` | 32 | 8 | checkbox menu |
| `Stepper` | 32 | — | private buttons |
| `Link` | 20 | 0 | underline |
| `Slider` | 28 | track 4 | |
| `RangeSlider` | 28 | track 4 | two handles |
| `SegmentedControl` | 30 | 4 | |
| `CheckBox` | 28 | 6 | |
| `Toggle` | 28 | 6 | |
| `Rating` | 20 | 4 star gap | star size 20 |
| `ProgressBar` | 14 | — | |
| `ProgressRing` | 36 | thickness 4 | |
| `Meter` | 10 | — | threshold colors |
| `Spinner` | 24 | — | |
| `Skeleton` | 14 per line | 8 line gap | |
| `Card` | content + 2×12 | header 30 | radius 6, shadow md |
| `Expander` | header 32 + content | 10 | radius 4 |
| `Toolbar` | 44 | 8, gap 6 | strip height configurable |
| `StatusBar` | 24 | 10 | |
| `Pagination` | 30 | 28 item / 4 gap | five-page window |
| `EmptyState` | content + action | 8 | centered |
| `Banner` | 40 | 12 | severity colours |
| `Chip` | 26 | 9 | radius 4 |
| `Badge` | 18 | 7 | dot 10 |
| `StatusIndicator` | 18 | dot 8 / gap 6 | |
| `SectionHeader` | lineHeight | gap 10 | optional subtitle/action |
| `Stat` | 44 | 4 gap | caption adds a line |
| `StepIndicator` | 46 | 24 gap | lineHeight + 28 |
| `Timeline` | 46/entry | dot 4 | title + detail |
| `Kbd` | 22 | 6/3 | key cap |
| `Icon` | 20 | — | size configurable |
| `DropZone` | 80 | — | hover border |
| `PropertyRow` | 18 | label 110 | help adds a line |
| `Avatar` | 32 (default) | — | size configurable |
| `AvatarGroup` | 28 | overlap 32% | +N marker |
| `TagList` | 26 | 4 gap | +N marker |
| `TagInput` | 34 | 4 | input + chips |

## No-font fallbacks

These sizes apply when a control is constructed without a font, which is useful for layout tests:

| Control | Fallback |
|---|---:|
| `Button` | 96 × 36 |
| `TextInput` | 32 high |
| `CheckBox` | 26 high |
| `ProgressBar` | 20 high |
| `Banner` | 36 high |
| `Chip` | 24 high |
| `Badge` | 18 high |
| `Link` | 20 high |

## Native macOS

| Control | Default / option |
|---|---|
| `NativeProgressIndicator` | bar 160×12, spinning 20×20 |
| `NativeStepper` | 19×27 |
| `NativeControlSize` | Small / Regular / Large → NSControlSize |

The native wrappers expose `preferredSize()`/`setPreferredSize()` so Flex/Stack can lay them out.
Gallery uses a Flex row for the native block.

## Theme tokens

Spacing comes from `Theme::spacing` and control metrics from `Theme::controls`; a theme can set the
default radius, borders, accent and focus ring. Individual controls expose `setPadding`,
`setHeaderHeight`, `setRadius`, `setHeight`, `setStarSize` and similar setters when a view needs a
one-off HIG adjustment.

## Related

- [cross-platform-controls.md](cross-platform-controls.md)
- [native-control-extension.md](native-control-extension.md)
