# Declarative DSL prototype

This branch explores a concise C++ façade over the existing `Element` / `Reconciler` / Layer
architecture. It is not a second renderer and does not change the reconciliation semantics.

## Goals

- Variadic children, so a page reads like a tree rather than a sequence of `push_back` calls.
- A build-scoped theme, so text/button/card defaults do not have to be threaded through every call.
- Static children need no keys; keys remain for dynamic/reordered lists.
- Default-styled high-level containers (`Page`, `Section`, `Card`, `FormField`).
- Custom retained controls must be first-class citizens in the DSL, not an escape hatch bolted on
  beside it.

## The facade

```cpp
dsl::ThemeScope scope(theme);

dsl::Page("Library",
  dsl::Caption("3 builds"),
  dsl::Row({ .gap = 10.0f },
    dsl::Button("Refresh", [this] { refresh(); }),
    dsl::Spacer(),
    dsl::Caption("updated now")),
  dsl::Card("Recent builds",
    dsl::Text("main"),
    dsl::Caption("2 minutes ago")),
  dsl::Section("Details",
    dsl::Text("Everything here inherited font, colour and padding.")));
```

The options struct keeps the variadic call readable while leaving room for new layout fields:

```cpp
dsl::Column({ .gap = 12.0f, .padding = Insets{}.all(24.0f) },
  dsl::Title("Settings"),
  dsl::Text("Only the theme supplies the font and colour."));
```

## Theme scope

`dsl::ThemeScope` sets the theme for the current build. Scopes nest and restore the previous theme.
`Text`, `Title`, `Caption`, `Button`, `Card`, `Section`, `FormField` and `Page` read it; explicit
fonts/colours still win in the options struct.

The scope is thread-local and build-scoped: construct it inside the `ElementHost` build function, not
at application start. A theme switch rebuilds the page, which re-creates the scope with the new
theme.

## Keys

`Text`, `Button`, `Card` and the layout helpers do not require keys for static children. The
positional identity the reconciler already provides is enough. Use explicit keys for dynamic lists
and reorders:

```cpp
dsl::Column({ .gap = 8.0f }, components::ForEach(items, [](const Item& item, size_t) {
  return dsl::Card(item.title, dsl::Text(item.subtitle), item.id);
}));
```

(For keyed list sections, `components::RecyclingList` and `components::ForEach` remain the
primitives.)

## Custom controls

There are two supported paths, both using the same element tree:

1. **Reusable UI control as a `Component`.** A player panel, inspector or streaming editor composes
   DSL nodes and exposes state; it is mounted with `components::Component`.
2. **Custom drawing/retained surface.** A `Layer` subclass is hosted with `dsl::Canvas`:

```cpp
class WaveformLayer : public Layer {
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
};

auto waveform = std::make_shared<WaveformLayer>();
dsl::Column({ .gap = 8.0f },
  dsl::Caption("Waveform"),
  dsl::Canvas(waveform, 84.0f, "waveform"));
```

`dsl::Canvas` is a typed alias over `components::Layer` with an optional minimum height; it keeps the
same layer instance across rebuilds, so custom state (playhead, selection, viewport) survives. A
custom control that needs a DSL subtree as well can be a `Component` whose build returns
`dsl::Canvas` plus ordinary DSL nodes.

## Overlays

`dsl::Overlay` is the thin façade over `OverlayHost`. It still pushes transient Window surfaces; it
just removes construction and dismissal bookkeeping from the call site:

```cpp
dsl::Overlay overlays(window, theme);

const uint64_t toast = overlays.Toast("Saved");
overlays.Tooltip("Hint", pointer);
overlays.Dialog("Delete build?", "This cannot be undone", contentLayer, {
  { "Cancel", {} },
  { "Delete", [this] { removeBuild(); } },
});
overlays.Popup(contentLayer, anchoredOptions);
overlays.Menu({ "Duplicate", "Rename", "Delete" }, pointer, [this](int index) { ... });

overlays.close(toast);
```

- `Toast`/`Tooltip`/`Dialog`/`Popup`/`Menu` each build the control and push it through `OverlayHost`.
- Dialog buttons run their action and then close the entry; `Menu` items do the same.
- `close`/`closeAll` mirror `OverlayHost`.
- Content is still a retained Layer (`dsl::Canvas` or a custom control); a future pass can add an
  `Element`-content overload that mounts the content through a dedicated `Reconciler`.
## What stays unchanged

- `Element` is still immutable and `Reconciler` still matches `(kind, key)`.
- Widgets are still retained Layers; the DSL only constructs their Element descriptions.
- `OverlayHost` remains the lifecycle entry point for Dialog/Popup/Toast/Tooltip/Menu.
- `components::Layer` remains available for a raw layer that has no semantic wrapper yet.

## Example

`examples/ui/ui_dsl_gallery.cpp` is the executable reference. It ports every page of
`ui_declarative_gallery` to the DSL, then appends two pages after the Escape hatch:

- **Waveform** — a custom retained `WaveformLayer` hosted by `dsl::Canvas`, with transport
  controls and a Toast through `dsl::Overlay`.
- **DSL** — the variadic/theme-scoped page from the first prototype.

```sh
cmake --build out/examples --config Release --target ui_dsl_gallery
out/examples/ui/Release/ui_dsl_gallery --headless --frames 2 --out dsl.png
```

It exercises theme defaults, variadic `Row`/`Column`, high-level `Page`/`Card`/`Section`, and a
custom `WaveformLayer` hosted through `dsl::Canvas`.
