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

#ifndef CORAL_UI_COMMAND_H
#define CORAL_UI_COMMAND_H

#include <ui/Event.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

/**
 * A keyboard shortcut, expressed as a logical key plus the modifiers that must be down.
 *
 * The command modifier is `meta` (Cmd on macOS). On Windows and Linux an application that wants
 * Ctrl-based shortcuts binds `control`, which is what the platform maps there; the two are kept
 * separate so the same command table can later be adapted per platform without guessing.
 */
struct Shortcut {
  Key key = Key::Unknown;
  bool control = false;
  bool shift = false;
  bool alt = false;
  bool meta = false;

  [[nodiscard]] static constexpr Shortcut command(const Key key) {
    return Shortcut{ key, false, false, false, true };
  }

  [[nodiscard]] static constexpr Shortcut controlKey(const Key key) {
    return Shortcut{ key, true, false, false, false };
  }

  [[nodiscard]] static constexpr Shortcut controlShift(const Key key) {
    return Shortcut{ key, true, true, false, false };
  }

  [[nodiscard]] bool matches(const KeyEvent& event) const;
};

/** The display name of one logical key ("Esc", "F5", "Z", ","), for menus and tooltips. */
[[nodiscard]] std::string keyLabel(Key key);

/** "Ctrl+Shift+Z" style text, with "Cmd" for the meta modifier. */
[[nodiscard]] std::string shortcutLabel(const Shortcut& shortcut);

/**
 * The command table of a window.
 *
 * A command has a stable name, an optional shortcut, a handler and an availability predicate. Menus,
 * menu bars and buttons resolve their enabled/checked state through that name instead of keeping a
 * second copy of the state, which is what keeps "the command is unavailable" consistent everywhere.
 *
 * Dispatch happens after the focused layer had the chance to handle the key, so a text field can own
 * its editing shortcuts while the application still receives the ones the field ignores. The table is
 * UI-thread state, like the layer tree; the handlers are owned by it through `std::function`.
 */
class CommandRegistry {
 public:
  using Handler = std::function<void()>;
  /** Availability of a command; re-evaluated on every query, never cached. */
  using Predicate = std::function<bool()>;

  /**
   * Binds @p handler to @p shortcut.
   *
   * Binding the same shortcut again replaces the previous command, and binding the same name moves
   * it to the new shortcut: a command table is a map, not an event listener list.
   */
  void bind(Shortcut shortcut, std::string name, Handler handler, Predicate enabled = {});

  /** Binds a command that has no shortcut (menu-only action). */
  void bind(std::string name, Handler handler, Predicate enabled = {});

  /**
   * Adds @p shortcut as another way to trigger the command @p name.
   *
   * @return false when @p name is not bound. Removal is by name, so `unbind` removes aliases too.
   */
  bool bindAlias(Shortcut shortcut, const std::string& name);

  /** Removes a binding by name. */
  void unbind(const std::string& name);

  /**
   * Runs the command whose shortcut matches @p event.
   *
   * A disabled command does not consume the key: `false` lets the event continue to bubble or to a
   * second binding, which is what "greyed out" should mean for keyboard input.
   *
   * @return true when an enabled handler ran.
   */
  bool dispatch(const KeyEvent& event);

  /** Runs a command by name (menu item, toolbar button, programmatic action). */
  bool invoke(const std::string& name);

  /** Whether the command exists and its predicate/flag currently allow it. */
  [[nodiscard]] bool isEnabled(const std::string& name) const;

  /** Explicit disable, composed with the predicate; useful for modal pages. */
  void setEnabled(const std::string& name, bool enabled);

  [[nodiscard]] bool isChecked(const std::string& name) const;
  void setChecked(const std::string& name, bool checked);

  /** The bound shortcut, or a shortcut with `Key::Unknown` when the command has none. */
  [[nodiscard]] Shortcut shortcutFor(const std::string& name) const;
  /** The shortcut label for @p name, or an empty string when it has no shortcut. */
  [[nodiscard]] std::string shortcutLabelFor(const std::string& name) const;

  /** Command names in binding order, for a menu bar or a settings page. */
  [[nodiscard]] std::vector<std::string> names() const;

  /** Optional presentation metadata for a bound command. An empty title means "no menu item". */
  struct Info {
    std::string title;
    /** Top-level menu/group title; empty commands go into a generic Commands menu. */
    std::string group;
    /** Ordering inside the group; lower first, ties keep binding order. */
    int order = 0;
    /** Standard-menu insertion index; -1 appends. Ignored by a Commands submenu. */
    int menuIndex = -1;
    /** Insert a separator immediately before this command in a standard menu. */
    bool separatorBefore = false;
    /** Optional SF Symbol shown next to the command in native menus. */
    std::string icon;
  };

  /**
   * Attaches presentation metadata used by a native menu bar.
   *
   * The registry stays presentation-free unless a caller opts in; commands without an `Info.title`
   * are not published to a native menu.
   */
  void setInfo(const std::string& name, Info info);
  [[nodiscard]] std::optional<Info> info(const std::string& name) const;

  /**
   * Called after the table or a command's state changes.
   *
   * Replaces every handler previously added with `addChangedHandler` (and removes them), so this
   * stays the single-owner entry point; use `addChangedHandler` when several views must refresh.
   */
  void setChangedCallback(std::function<void()> callback);

  /**
   * Adds a change listener and returns a token for `removeChangedHandler`.
   *
   * A `Menu` adds one for as long as it is bound to the registry, so an open menu follows a command's
   * enabled/checked state without polling; several menus can listen at once.
   */
  [[nodiscard]] int addChangedHandler(std::function<void()> handler);
  /** Removes a listener; unknown or already-removed tokens are ignored. */
  void removeChangedHandler(int token);

  void clear();
  [[nodiscard]] bool empty() const noexcept {
    return mCommands.empty();
  }
  [[nodiscard]] std::size_t size() const noexcept {
    return mCommands.size();
  }

 private:
  struct Entry {
    Shortcut shortcut;
    std::string name;
    Handler handler;
    Predicate enabled;
    bool explicitlyEnabled = true;
    bool checked = false;
    bool hasInfo = false;
    Info info;
  };

  [[nodiscard]] Entry* find(const std::string& name);
  [[nodiscard]] const Entry* find(const std::string& name) const;
  [[nodiscard]] bool evaluate(const Entry& entry) const;
  void notifyChanged() const;

  struct Listener {
    int token = 0;
    std::function<void()> handler;
  };

  std::vector<Entry> mCommands;
  /** Registered change listeners; `notifyChanged` calls a copy so a handler may remove itself. */
  std::vector<Listener> mChangedHandlers;
  int mNextHandlerToken = 1;
};

} // namespace coral::ui

#endif // CORAL_UI_COMMAND_H
