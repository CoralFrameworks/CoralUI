# modules/mvvm

The view-model layer for CoralUI: observable objects, commands, observable lists and field
validation. It is deliberately a small, observer-only library, not an application framework.

## Target

| Target | Contents | Dependencies |
|---|---|---|
| `mvvm` | `ObservableObject`, `Command`, `AsyncCommand`, `ObservableList`, `Validation`, `Field`, `Dispatcher` | `observer` only |

It does not depend on `ui`, `event`, `graphics` or `effect`. A view model can therefore be tested
headlessly, used by a tool, or hosted by a different UI framework. The adapter that binds these
objects to `Element`/`Layer` controls lives in `modules/ui/include/ui/mvvm/` (`DataContext`,
`CommandBinding`, `ObservableListBinding`), not here.

## Header paths

```cpp
#include <mvvm/ObservableObject.h>
#include <mvvm/Command.h>
#include <mvvm/AsyncCommand.h>
#include <mvvm/ObservableList.h>
#include <mvvm/Field.h>
```

## Threading

`observer::Property` and `observer::Event` deliver on the thread that changed them unless an
`Executor` was supplied. A view model that can be written from a worker thread installs a
`mvvm::Dispatcher`; `AsyncCommand` posts its terminal state through it. The UI adapter supplies a
dispatcher that hops to the UI thread; `InlineDispatcher` is the deterministic default for tests.
Commands and view-model state are otherwise UI-thread objects, like the layer tree.

## Boundaries

- No `INotifyPropertyChanged` strings, reflection, source generation or dependency graph. Types are
  the schema.
- No navigation, DI container, service locator or validation DSL. `Validation` is an ordered list of
  `std::function` rules with product-owned wording.
- No reference to `ui::Layer`, `Window` or a renderer. Bindings are the adapter's job.
- `ObservableList` is index based and reports structural changes
  (`inserted`/`removed`/`moved`/`replaced`/`reset`); a list view keys its children by item
  identity, not by this event index.

## Tests

```sh
cmake --build <build> --target tests_MVVM
ctest --test-dir <build> -R tests_MVVM --output-on-failure
```

The suite covers command availability/refresh, async completion/cancel/error/dispatcher, list
structural events, field validation and dispatcher marshalling.
