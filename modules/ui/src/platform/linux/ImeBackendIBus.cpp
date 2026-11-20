/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ImeBackend.h"

#include <ui/Event.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <ibus.h>

#include <string>
#include <vector>

namespace coral::ui::detail {

namespace {

std::string textOf(IBusText* text) {
  if (text == nullptr) return {};
  const gchar* value = ibus_text_get_text(text);
  return value != nullptr ? std::string(value) : std::string();
}

class IbusIme final : public ImeBackend {
 public:
  IbusIme(void* display, const unsigned long window, ImeHost* host)
      : mDisplay(static_cast<Display*>(display)), mWindow(static_cast<::Window>(window)),
        mHost(host) {}

  ~IbusIme() override { shutdown(); }

  bool initialize() override {
    if (mDisplay == nullptr || mWindow == 0 || mHost == nullptr) return false;
    ibus_init();
    mBus = ibus_bus_new();
    if (mBus == nullptr || !ibus_bus_is_connected(mBus)) {
      shutdown();
      return false;
    }
    mContext = ibus_bus_create_input_context(mBus, "coral");
    if (mContext == nullptr) {
      shutdown();
      return false;
    }
    g_signal_connect(mContext, "commit-text", G_CALLBACK(onCommit), this);
    g_signal_connect(mContext, "update-preedit", G_CALLBACK(onPreedit), this);
    g_signal_connect(mContext, "show-lookup-table", G_CALLBACK(onLookupTable), this);
    g_signal_connect(mContext, "update-lookup-table", G_CALLBACK(onLookupTable), this);
    g_signal_connect(mContext, "hide-lookup-table", G_CALLBACK(onHideLookupTable), this);
    g_signal_connect(mContext, "cursor-up", G_CALLBACK(onCursor), this);
    g_signal_connect(mContext, "cursor-down", G_CALLBACK(onCursor), this);
    ibus_input_context_focus_in(mContext);
    return true;
  }

  void shutdown() override {
    if (mContext != nullptr) {
      ibus_input_context_focus_out(mContext);
      g_object_unref(mContext);
      mContext = nullptr;
    }
    if (mBus != nullptr) {
      g_object_unref(mBus);
      mBus = nullptr;
    }
  }

  bool filterEvent(void* nativeEvent) override {
    if (nativeEvent == nullptr) return false;
    XEvent* event = static_cast<XEvent*>(nativeEvent);
    if (event->type != KeyPress || mContext == nullptr) return false;
    const KeySym symbol = XLookupKeysym(&event->xkey, 0);
    const guint state = event->xkey.state;
    const gboolean consumed = ibus_input_context_process_key_event(mContext,
      static_cast<guint>(symbol), static_cast<guint>(event->xkey.keycode), state);
    // Signals are dispatched on the default GMainContext; drain them now so commit/preeedit reach
    // the control before the next platform event.
    while (g_main_context_iteration(nullptr, FALSE)) {}
    return consumed != FALSE;
  }

  int lookupString(void* nativeKeyEvent, char* utf8, const int capacity, uint32_t* keysym,
      int* status) override {
    auto* key = static_cast<XKeyEvent*>(nativeKeyEvent);
    if (key == nullptr || utf8 == nullptr || capacity <= 0) return 0;
    KeySym symbol = NoSymbol;
    XComposeStatus compose{};
    const int count = XLookupString(key, utf8, capacity, &symbol, &compose);
    if (keysym != nullptr) *keysym = static_cast<uint32_t>(symbol);
    if (status != nullptr) *status = 0;
    return count;
  }

  void focusIn() override {
    if (mContext != nullptr) ibus_input_context_focus_in(mContext);
  }
  void focusOut() override {
    if (mContext != nullptr) ibus_input_context_focus_out(mContext);
  }

  void setCursorRect(const Rect& rect) override {
    if (mContext == nullptr) return;
    ibus_input_context_set_cursor_location(mContext, static_cast<int>(rect.getLeft()),
      static_cast<int>(rect.getTop()), static_cast<int>(rect.getWidth()),
      static_cast<int>(rect.getHeight()));
  }

  void setSurroundingText(const std::string& text, const size_t cursor,
      const size_t anchor) override {
    if (mContext == nullptr) return;
    IBusText* surrounding = ibus_text_new_from_string(text.c_str());
    if (surrounding == nullptr) return;
    // IBus counts characters, while TextInput stores UTF-8 bytes.
    const auto charactersBefore = [&text](const size_t byteOffset) {
      size_t count = 0;
      size_t index = 0;
      while (index < text.size() && index < byteOffset) {
        ++count;
        ++index;
        while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80) ++index;
      }
      return count;
    };
    ibus_input_context_set_surrounding_text(mContext, surrounding,
      static_cast<guint>(charactersBefore(cursor)), static_cast<guint>(charactersBefore(anchor)));
    g_object_unref(surrounding);
  }

  bool handleKey(const uint32_t key) override {
    if (mContext == nullptr) return false;
    guint keyval = 0;
    switch (static_cast<Key>(key)) {
      case Key::Up: keyval = 0xFF52; break;
      case Key::Down: keyval = 0xFF54; break;
      case Key::Enter: keyval = 0xFF0D; break;
      case Key::Escape: keyval = 0xFF1B; break;
      default: return false;
    }
    const gboolean consumed = ibus_input_context_process_key_event(mContext, keyval, 0, 0);
    while (g_main_context_iteration(nullptr, FALSE)) {}
    return consumed != FALSE;
  }

  [[nodiscard]] const char* name() const override { return "ibus"; }

 private:
  static void onCommit(IBusInputContext*, IBusText* text, gpointer userData) {
    auto* self = static_cast<IbusIme*>(userData);
    if (self == nullptr || self->mHost == nullptr) return;
    self->mHost->onImeCandidates({}, 0);
    self->mHost->onImeCommit(textOf(text));
  }

  static void onPreedit(IBusInputContext*, IBusText* text, guint, gboolean visible,
      gpointer userData) {
    auto* self = static_cast<IbusIme*>(userData);
    if (self != nullptr && self->mHost != nullptr) {
      self->mHost->onImeComposition(visible != FALSE ? textOf(text) : std::string());
    }
  }

  static void onLookupTable(IBusInputContext*, IBusLookupTable* table, gboolean,
      gpointer userData) {
    auto* self = static_cast<IbusIme*>(userData);
    if (self == nullptr || self->mHost == nullptr || table == nullptr) return;
    std::vector<std::string> candidates;
    const guint count = ibus_lookup_table_get_number_of_candidates(table);
    candidates.reserve(count);
    for (guint index = 0; index < count; ++index) {
      std::string label = std::to_string(index + 1) + ". ";
      label += textOf(ibus_lookup_table_get_candidate(table, index));
      candidates.push_back(std::move(label));
    }
    self->mHost->onImeCandidates(std::move(candidates),
      static_cast<size_t>(ibus_lookup_table_get_cursor_pos(table)));
  }

  static void onHideLookupTable(IBusInputContext*, gpointer userData) {
    auto* self = static_cast<IbusIme*>(userData);
    if (self != nullptr && self->mHost != nullptr) self->mHost->onImeCandidates({}, 0);
  }

  static void onCursor(IBusInputContext*, gpointer userData) {
    auto* self = static_cast<IbusIme*>(userData);
    if (self != nullptr && self->mHost != nullptr) {
      // The table itself arrives in update-lookup-table immediately after cursor-up/down; nothing
      // to do here but keep the signal connected so the default handler still runs.
    }
  }

  Display* mDisplay = nullptr;
  ::Window mWindow = 0;
  ImeHost* mHost = nullptr;
  IBusBus* mBus = nullptr;
  IBusInputContext* mContext = nullptr;
};

} // namespace anonymous

std::unique_ptr<ImeBackend> makeIbusIme(void* display, const unsigned long window, ImeHost* host) {
  auto backend = std::make_unique<IbusIme>(display, window, host);
  if (!backend->initialize()) return nullptr;
  return backend;
}

} // namespace coral::ui::detail
