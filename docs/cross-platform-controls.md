# Cross-platform controls

These controls live in the core `coral::ui` namespace and are custom-drawn on every platform. A
platform-native control belongs in `coral::ui::macos` or `coral::ui::win32`, never in this list.

## Gallery

`ui_demo` is a sectioned catalog built on `RecyclingListView`: a virtualized left list of
expandable section headers and their control rows, and the selected section's examples on the right.
Section rows toggle open/closed and open the group page; selecting a control row opens that
control's standalone demo. The `More` section holds additional framework controls: IconButton,
ToggleButton, NumberField, SegmentedControl, ColorInput/ColorButton/ColorPicker, Avatar, Icon,
Breadcrumb, Chip, TagInput, Toast, Tooltip, Popup, Dialog, Skeleton, StatusBar, TabBar,
SplitView, ListView, TreeView, Table, TableView, RichText, TimePicker, CandidateListView,
RecyclingListView, Menu, NativeAlert, FileDialog, DatePickerPanel and MenuBar.

Each interactive demo exposes its state through `GalleryViewModel` (toggle/check/radio/rating/
slider/range/stepper/selection/text/page/step/expander/color/last event). The control page binds an
event label and a model-value label through `bindProperty`, so a click is visible in the UI even
without a debugger. The demo sits inside an accent `DemoFrame`, and the sidebar row also gets
an accent selection box. The list has its own scrollbar hit gutter, separate from
the content, so dragging the thin bar no longer starts a row selection.

The declarative catalog is `ui_declarative_gallery`: the same control groups as an immutable
`Element` tree, with keys, `When`/`ForEach`, `State<T>` and `ElementHost`. It covers every control
that has a first-class element wrapper; controls without one are hosted through
`components::Layer` and labelled as the escape hatch. See
[declarative-components.md](declarative-components.md) for the wrapper matrix and the gaps.

Selection is data-driven. `GalleryViewModel` derives from `mvvm::ObservableObject` and owns
`observer::Property<GallerySelection>`. The shell stores it in `ui::mvvm::DataContext`, binds it with
`bindProperty`, and row clicks only write the model:

```cpp
struct GallerySelection { int section = 0; std::string control; };
struct GalleryViewModel : mvvm::ObservableObject {
  observer::Property<GallerySelection> selection;
};

dataContext.set(vm);
selectionBinding = bindProperty(vm->selection, gallery,
  [&](const GallerySelection& value) { gallery.setSelection(value); });
vm->selection.set({ 0, "Button" });
```

The list rows, sidebar selection box and the one-control page are projections of that property;
section expansion is view state. Interactive demos publish their changes to the same view model and
bind an event/value readout; this is why a click now visibly changes model state instead of only
painting a transient pressed state. The `Lists`
section contains a nested `ScrollView`; the nested wheel test lives in
`UiInput.NestedScrollViewConsumesTheWheelFirst`. Use `--section N` for headless page snapshots.

## Input

| Control | Key behavior | Accessibility | Style hook |
|---|---|---|---|
| `Button` | click / Enter / Space | Button | `button` |
| `ToggleButton` | selected/unselected toolbar toggle | CheckBox + Toggle/Invoke | `togglebutton` |
| `ToggleGroup` | horizontal multi-select toggles | Group + ToggleButtons | `togglegroup` |
| `IconButton` | image icon | Button | `iconbutton` |
| `Link` | text link, keyboard/AX Invoke | Link | `link` |
| `Kbd` | shortcut key cap | `kbd` |
| `Icon` | square tinted image | `icon` |
| `SearchField` | text edit / clear | TextField | `searchfield` |
| `TextInput` | caret, IME, selection, multiline | TextField | `textinput` |
| `PasswordField` | masked entry, IME composition, paste, no copy-out | TextField + masked value | `password` |
| `NumberField` | numeric edit, system locale | TextField | `numberfield` |
| `DateField` | calendar popup, ISO/local parse | TextField | `datefield` |
| `DateRangeField` | start/end pair, start ≤ end | Group | `daterangefield` |
| `TimeRangeField` | start/end time pair, start ≤ end | Group | `timerangefield` |
| `DateTimeField` | date + time pair | Group | `datetimefield` |
| `TimeField` | 12/24h, TimePicker popup | TextField | `timefield` |
| `Toggle` | on/off | CheckBox | `toggle` |
| `CheckBox` | checked / unchecked / Mixed | CheckBox | `checkbox` |
| `RadioButton` | group selection | Radio | `radio` |
| `RadioGroup` | exclusive vertical options | Group + Radio children | `radiogroup` |
| `CheckBoxGroup` | multi-select vertical options | Group + CheckBox children | `checkboxgroup` |
| `Rating` | click/drag or arrows, 0..stars, half steps, double callback, read-only | Slider + range / SetValue | `rating` |
| `Slider` | drag / keys | Slider + range | `slider` |
| `RangeSlider` | two handles, Tab switches, keys | Slider + range | `rangeslider` |
| `Stepper` | +/- and range | SpinButton + range | `stepper` |
| `SegmentedControl` | Home/End/arrow selection | TabList + ListItems | `segmented` |
| `Select` | dropdown menu | ComboBox | `select` |
| `ComboBox` | editable + dropdown | ComboBox | `combobox` |
| `MultiSelect` | checkbox dropdown, summary, Up/Down keyboard navigation | ComboBox + Invoke/Select | `multiselect` |
| `SplitButton` | primary + menu | Button | `splitbutton` |
| `ColorPicker` | swatches / HSV | ColorPicker + hex value / SetValue | `colorpicker` |

## Display and containers

| Control | Notes | Style hook |
|---|---|---|
| `Card` | title, content, header action, shadow | `card` |
| `Expander` | collapsible content, configurable header | `expander` |
| `Breadcrumb` | path + keyboard navigation | `breadcrumb` |
| `Pagination` | previous/next + five-page window | `pagination` |
| `Badge` | text / dot / icon | `badge` |
| `StatusIndicator` | colored dot + label | `status` |
| `Stat` | label + value + trend/caption | `stat` |
| `StepIndicator` | clickable wizard steps | Group + current value | `stepindicator` |
| `Timeline` | vertical activity entries + state dots | Group + count | `timeline` |
| `DropZone` | external file/text drop target | `dropzone` |
| `Chip` | text / icon / selected / removable / variants | `chip` |
| `Banner` | severity, action, dismiss, enter/exit animation | `banner` |
| `Avatar` | initials / circular image / presence dot | `avatar` |
| `AvatarGroup` | overlapping avatars + overflow count | `avatargroup` |
| `TagList` | single-line tags + '+N' overflow | `taglist` |
| `TagInput` | submit text into removable tags | `taginput` |
| `Divider` | horizontal / vertical, thickness, inset | `divider` |
| `EmptyState` | title / description / icon / content / action | `emptystate` |
| `Toolbar` | fixed/flexible items, separators, overflow | `toolbar` |
| `StatusBar` | leading/trailing items + left/right text | `statusbar` |
| `ProgressBar` | determinate / indeterminate | `progress` |
| `Meter` | threshold-colored value bar | `meter` |
| `ProgressRing` | determinate / indeterminate | `progressring` |
| `Skeleton` | single/multi-line pulse | `skeleton` |
| `Spinner` | rotating dots | `spinner` |

## Selection and checked states

- `CheckBox` exposes checked + Mixed; clicking Mixed resolves to checked.
- `Toggle` uses the CheckBox accessibility role.
- `Chip` exposes selected and AX selected.
- `SegmentedControl` / `TabBar` expose a TabList role, selected value and ListItem proxies with
  Select/Invoke actions.
- `Expander` exposes Expanded/Collapsed as accessibility value plus Toggle.
- `Calendar` exposes an ISO date value and SetValue.
- `Select` / `ComboBox` expose the selected text as value; ComboBox keeps it updated while editing.
- `ProgressBar` / `ProgressRing` expose a 0..1 range; indeterminate rings report Loading.

## Animation scheduling

Self-driven controls must override `hasRunningAnimation()` while they need frames:

```text
Spinner   spinning
Skeleton  animating
ProgressBar indeterminate
ProgressRing indeterminate
Banner    mExitPending
TextInput focused caret blink
Tooltip   pending delayed show
Toast     pending deadline
```

## Other themed surfaces

| Control | Style hook |
|---|---|
| `TabBar` | `tabbar` |
| `Tooltip` | `tooltip` |
| `Toast` | `toast` |
| `ToastStack` | `toaststack` |
| `Popup` | `popup` |
| `Menu` | `menu` |
| `Dialog` | `dialog` |
| `TitleBar` | `titlebar` |
| `Table` | `table` |
| `TreeView` | `treeview` |
| `ListView` | `listview` |

## Layout primitives

`Flex`, `Stack`, `SplitView`, `ScrollView`, `TabBar`, `Toolbar`, `StatusBar`, `Card`, `Expander`,
`FormField`, `PropertyRow`, `SectionHeader`.

## Related docs

- [hig-metrics.md](hig-metrics.md) — default sizes and spacing.

- [native-control-extension.md](native-control-extension.md) — optional AppKit controls and future
  platform native-control extensions.
- [macOS-capability-plan.md](macOS-capability-plan.md) — platform service status and manual checks.
