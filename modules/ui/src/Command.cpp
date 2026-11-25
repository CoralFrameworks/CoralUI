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

#include <ui/Command.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

bool Shortcut::matches(const KeyEvent& event) const {
  return key != Key::Unknown && event.key == key && event.control == control &&
    event.shift == shift && event.alt == alt && event.meta == meta;
}

std::string keyLabel(const Key key) {
  if (key >= Key::A && key <= Key::Z) {
    return std::string(1, static_cast<char>('A' + (static_cast<int>(key) - static_cast<int>(Key::A))));
  }
  if (key >= Key::Digit0 && key <= Key::Digit9) {
    return std::string(1,
      static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(Key::Digit0))));
  }
  if (key >= Key::F1 && key <= Key::F12) {
    return "F" + std::to_string(1 + static_cast<int>(key) - static_cast<int>(Key::F1));
  }
  if (key >= Key::Numpad0 && key <= Key::Numpad9) {
    return "Num " + std::to_string(static_cast<int>(key) - static_cast<int>(Key::Numpad0));
  }
  switch (key) {
    case Key::Escape: return "Esc";
    case Key::Enter: return "Enter";
    case Key::Space: return "Space";
    case Key::Tab: return "Tab";
    case Key::Backspace: return "Backspace";
    case Key::Left: return "Left";
    case Key::Right: return "Right";
    case Key::Up: return "Up";
    case Key::Down: return "Down";
    case Key::Delete: return "Delete";
    case Key::Home: return "Home";
    case Key::End: return "End";
    case Key::PageUp: return "PageUp";
    case Key::PageDown: return "PageDown";
    case Key::Insert: return "Insert";
    case Key::Comma: return ",";
    case Key::Period: return ".";
    case Key::Slash: return "/";
    case Key::Backslash: return "\\";
    case Key::Semicolon: return ";";
    case Key::Apostrophe: return "'";
    case Key::LeftBracket: return "[";
    case Key::RightBracket: return "]";
    case Key::Minus: return "-";
    case Key::Equal: return "=";
    case Key::Grave: return "`";
    case Key::LeftShift: return "Left Shift";
    case Key::RightShift: return "Right Shift";
    case Key::LeftControl: return "Left Ctrl";
    case Key::RightControl: return "Right Ctrl";
    case Key::LeftAlt: return "Left Alt";
    case Key::RightAlt: return "Right Alt";
    case Key::LeftMeta: return "Left Meta";
    case Key::RightMeta: return "Right Meta";
    case Key::CapsLock: return "Caps Lock";
    case Key::NumLock: return "Num Lock";
    case Key::ScrollLock: return "Scroll Lock";
    case Key::PrintScreen: return "Print Screen";
    case Key::Pause: return "Pause";
    case Key::Menu: return "Menu";
    case Key::NumpadMultiply: return "Num *";
    case Key::NumpadAdd: return "Num +";
    case Key::NumpadSubtract: return "Num -";
    case Key::NumpadDecimal: return "Num .";
    case Key::NumpadDivide: return "Num /";
    case Key::NumpadEnter: return "Num Enter";
    case Key::NumpadEqual: return "Num =";
    default: return {};
  }
}

std::string shortcutLabel(const Shortcut& shortcut) {
  if (shortcut.key == Key::Unknown) return {};
  std::string label;
  const auto append = [&label](const char* part) {
    if (!label.empty()) label += '+';
    label += part;
  };
  if (shortcut.control) append("Ctrl");
  if (shortcut.shift) append("Shift");
  if (shortcut.alt) append("Alt");
  if (shortcut.meta) append("Cmd");
  const std::string key = keyLabel(shortcut.key);
  if (key.empty()) return {};
  if (!label.empty()) label += '+';
  label += key;
  return label;
}

namespace {

[[nodiscard]] bool sameShortcut(const Shortcut& left, const Shortcut& right) {
  return left.key == right.key && left.control == right.control && left.shift == right.shift &&
    left.alt == right.alt && left.meta == right.meta;
}

} // namespace

bool CommandRegistry::bindAlias(const Shortcut shortcut, const std::string& name) {
  if (name.empty() || shortcut.key == Key::Unknown) return false;
  const auto source = std::find_if(mCommands.begin(), mCommands.end(),
    [&name](const Entry& entry) { return entry.name == name; });
  if (source == mCommands.end()) return false;
  std::erase_if(mCommands,
    [&shortcut](const Entry& entry) { return sameShortcut(entry.shortcut, shortcut); });
  Entry alias = *source;
  alias.shortcut = shortcut;
  mCommands.push_back(std::move(alias));
  notifyChanged();
  return true;
}

void CommandRegistry::bind(Shortcut shortcut, std::string name, Handler handler,
    Predicate enabled) {
  if (name.empty() || !handler) {
    return;
  }
  // A name identifies one command: re-binding it moves the command instead of duplicating it.
  std::erase_if(mCommands, [&name](const Entry& entry) { return entry.name == name; });
  // One shortcut has one command: this is what lets a modal page temporarily take over a key.
  // A shortcut-less command has no key to take over, so it must not evict the other menu-only ones.
  if (shortcut.key != Key::Unknown) {
    std::erase_if(mCommands,
      [&shortcut](const Entry& entry) { return sameShortcut(entry.shortcut, shortcut); });
  }
  mCommands.push_back(Entry{ shortcut, std::move(name), std::move(handler), std::move(enabled),
    true, false });
  notifyChanged();
}

void CommandRegistry::bind(std::string name, Handler handler, Predicate enabled) {
  bind(Shortcut{}, std::move(name), std::move(handler), std::move(enabled));
}

void CommandRegistry::unbind(const std::string& name) {
  const auto removed = std::erase_if(mCommands,
    [&name](const Entry& entry) { return entry.name == name; });
  if (removed != 0) notifyChanged();
}

bool CommandRegistry::dispatch(const KeyEvent& event) {
  // The most recently bound command wins, which is the usual overlay/modal precedence.
  for (auto position = mCommands.rbegin(); position != mCommands.rend(); ++position) {
    if (position->shortcut.key == Key::Unknown || !position->shortcut.matches(event)) continue;
    if (!evaluate(*position)) return false;
    // Copy the handler before invoking it: a handler may bind or unbind commands, which can
    // reallocate the vector that is being walked.
    const Handler handler = position->handler;
    if (handler) handler();
    return true;
  }
  return false;
}

bool CommandRegistry::invoke(const std::string& name) {
  Entry* entry = find(name);
  if (entry == nullptr || !evaluate(*entry)) return false;
  const Handler handler = entry->handler;
  if (handler) handler();
  return true;
}

bool CommandRegistry::isEnabled(const std::string& name) const {
  const Entry* entry = find(name);
  return entry != nullptr && evaluate(*entry);
}

void CommandRegistry::setEnabled(const std::string& name, const bool enabled) {
  Entry* entry = find(name);
  if (entry == nullptr || entry->explicitlyEnabled == enabled) return;
  entry->explicitlyEnabled = enabled;
  notifyChanged();
}

bool CommandRegistry::isChecked(const std::string& name) const {
  const Entry* entry = find(name);
  return entry != nullptr && entry->checked;
}

void CommandRegistry::setChecked(const std::string& name, const bool checked) {
  Entry* entry = find(name);
  if (entry == nullptr || entry->checked == checked) return;
  entry->checked = checked;
  notifyChanged();
}

Shortcut CommandRegistry::shortcutFor(const std::string& name) const {
  const Entry* entry = find(name);
  return entry != nullptr ? entry->shortcut : Shortcut{};
}

std::string CommandRegistry::shortcutLabelFor(const std::string& name) const {
  const Entry* entry = find(name);
  return entry != nullptr ? shortcutLabel(entry->shortcut) : std::string();
}

void CommandRegistry::setInfo(const std::string& name, Info info) {
  Entry* entry = find(name);
  if (entry == nullptr) return;
  entry->hasInfo = true;
  entry->info = std::move(info);
  notifyChanged();
}

std::optional<CommandRegistry::Info> CommandRegistry::info(const std::string& name) const {
  const Entry* entry = find(name);
  if (entry == nullptr || !entry->hasInfo) return std::nullopt;
  return entry->info;
}

std::vector<std::string> CommandRegistry::names() const {
  std::vector<std::string> result;
  result.reserve(mCommands.size());
  for (const Entry& entry: mCommands) result.push_back(entry.name);
  return result;
}

void CommandRegistry::clear() {
  if (mCommands.empty()) return;
  mCommands.clear();
  notifyChanged();
}

void CommandRegistry::setChangedCallback(std::function<void()> callback) {
  mChangedHandlers.clear();
  if (callback) {
    mChangedHandlers.push_back({ mNextHandlerToken++, std::move(callback) });
  }
}

int CommandRegistry::addChangedHandler(std::function<void()> handler) {
  if (!handler) return 0;
  const int token = mNextHandlerToken++;
  mChangedHandlers.push_back({ token, std::move(handler) });
  return token;
}

void CommandRegistry::removeChangedHandler(const int token) {
  if (token == 0) return;
  mChangedHandlers.erase(std::remove_if(mChangedHandlers.begin(), mChangedHandlers.end(),
                                 [token](const Listener& listener) {
                                   return listener.token == token;
                                 }),
    mChangedHandlers.end());
}

void CommandRegistry::notifyChanged() const {
  // Call a copy: a handler may remove its own listener (a menu closing while it refreshes).
  std::vector<std::function<void()>> handlers;
  handlers.reserve(mChangedHandlers.size());
  for (const Listener& listener: mChangedHandlers) handlers.push_back(listener.handler);
  for (const auto& handler: handlers) handler();
}

CommandRegistry::Entry* CommandRegistry::find(const std::string& name) {
  const auto position = std::find_if(mCommands.begin(), mCommands.end(),
    [&name](const Entry& entry) { return entry.name == name; });
  return position != mCommands.end() ? &*position : nullptr;
}

const CommandRegistry::Entry* CommandRegistry::find(const std::string& name) const {
  const auto position = std::find_if(mCommands.begin(), mCommands.end(),
    [&name](const Entry& entry) { return entry.name == name; });
  return position != mCommands.end() ? &*position : nullptr;
}

bool CommandRegistry::evaluate(const Entry& entry) const {
  if (!entry.explicitlyEnabled) return false;
  return !entry.enabled || entry.enabled();
}

} // namespace coral::ui
