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

#include "AtspiBridge.h"

#include <ui/AccessibilityAtSpi.h>

#include <dbus/dbus.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::detail {

namespace {

constexpr const char* kAccessiblePrefix = "/org/a11y/atspi/accessible";
constexpr const char* kAppPath = "/org/a11y/atspi/accessible/root";
constexpr const char* kWindowPrefix = "/org/a11y/atspi/accessible/window_";
constexpr const char* kNullPath = "/org/a11y/atspi/null";
constexpr const char* kBusName = "org.a11y.atspi.Registry";
constexpr const char* kRegistryPath = "/org/a11y/atspi/accessible/root";
constexpr const char* kSocketInterface = "org.a11y.atspi.Socket";

constexpr unsigned kStateChecked = 4;
constexpr unsigned kStateEditable = 7;
constexpr unsigned kStateEnabled = 8;
constexpr unsigned kStateFocusable = 12;
constexpr unsigned kStateFocused = 13;
constexpr unsigned kStateSelected = 23;
constexpr unsigned kStateSensitive = 24;
constexpr unsigned kStateShowing = 26;
constexpr unsigned kStateVisible = 30;

unsigned roleNumber(const AccessibilityRole role) {
  switch (role) {
    case AccessibilityRole::Button: return 43;       // ATSPI_ROLE_PUSH_BUTTON
    case AccessibilityRole::CheckBox: return 7;      // ATSPI_ROLE_CHECK_BOX
    case AccessibilityRole::Radio: return 44;        // ATSPI_ROLE_RADIO_BUTTON
    case AccessibilityRole::Slider: return 51;       // ATSPI_ROLE_SLIDER
    case AccessibilityRole::TextField: return 79;    // ATSPI_ROLE_ENTRY
    case AccessibilityRole::Label: return 29;        // ATSPI_ROLE_LABEL
    case AccessibilityRole::List: return 31;          // ATSPI_ROLE_LIST
    case AccessibilityRole::Tree: return 65;          // ATSPI_ROLE_TREE
    case AccessibilityRole::Menu: return 33;          // ATSPI_ROLE_MENU
    case AccessibilityRole::Table: return 55;         // ATSPI_ROLE_TABLE
    case AccessibilityRole::ListItem: return 32;      // ATSPI_ROLE_LIST_ITEM
    case AccessibilityRole::ScrollView: return 49;    // ATSPI_ROLE_SCROLL_PANE
    case AccessibilityRole::Dialog: return 16;        // ATSPI_ROLE_DIALOG
    case AccessibilityRole::Group: return 39;         // ATSPI_ROLE_PANEL
    case AccessibilityRole::Generic:
    default: return 67;                                // ATSPI_ROLE_UNKNOWN
  }
}

std::string pathForWindow(const uint64_t id) {
  return std::string(kWindowPrefix) + std::to_string(id);
}

bool isPrefix(const std::string& path, const std::string& prefix) {
  return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string_view> splitPath(const std::string& path) {
  std::vector<std::string_view> segments;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t end = path.find('/', start);
    const std::string_view segment = std::string_view(path).substr(start,
      end == std::string::npos ? path.size() - start : end - start);
    if (!segment.empty()) segments.push_back(segment);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return segments;
}

struct Snapshot {
  AccessibilityNode root;
};

struct Located {
  const AccessibilityNode* node = nullptr;
  std::string parent;
  int index = -1;
  bool windowRoot = false;
};

bool locate(const AccessibilityNode& root, const std::string& relative, Located* located) {
  if (located == nullptr) return false;
  if (relative.empty()) {
    located->node = &root;
    located->windowRoot = true;
    located->index = -1;
    return true;
  }
  const std::vector<std::string_view> segments = splitPath(relative);
  if (segments.empty()) return false;
  const AccessibilityNode* node = &root;
  for (const std::string_view segment: segments) {
    size_t index = 0;
    for (const char ch: segment) {
      if (ch < '0' || ch > '9') return false;
      index = index * 10 + static_cast<size_t>(ch - '0');
    }
    if (index >= node->children.size()) return false;
    node = &node->children[index];
  }
  located->node = node;
  located->windowRoot = false;
  located->index = 0;
  for (size_t index = 1; index < segments.size(); ++index) {
    located->index = located->index * 10 + static_cast<int>(segments[index][0] - '0');
  }
  // The loop above is only correct for single-digit segments; recompute the last one exactly.
  const std::string_view last = segments.back();
  int value = 0;
  for (const char ch: last) value = value * 10 + (ch - '0');
  located->index = value;
  located->parent = relative.substr(0, relative.size() - last.size() - (relative.size() > last.size() ? 1 : 0));
  return true;
}

std::string childPath(const std::string& base, const size_t index) {
  if (base.empty()) return std::to_string(index);
  return base + "/" + std::to_string(index);
}

void appendObjectRef(DBusMessageIter* iter, const std::string& bus, const std::string& path) {
  DBusMessageIter structure;
  dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, nullptr, &structure);
  const char* busName = bus.c_str();
  const char* objectPath = path.c_str();
  dbus_message_iter_append_basic(&structure, DBUS_TYPE_STRING, &busName);
  dbus_message_iter_append_basic(&structure, DBUS_TYPE_OBJECT_PATH, &objectPath);
  dbus_message_iter_close_container(iter, &structure);
}

const char* kIntrospectionXml = R"XML(<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.a11y.atspi.Accessible">
    <method name="GetChildAtIndex"><arg direction="in" type="i"/><arg direction="out" type="(so)"/></method>
    <method name="GetChildren"><arg direction="out" type="a(so)"/></method>
    <method name="GetIndexInParent"><arg direction="out" type="i"/></method>
    <method name="GetRole"><arg direction="out" type="u"/></method>
    <method name="GetRoleName"><arg direction="out" type="s"/></method>
    <method name="GetLocalizedRoleName"><arg direction="out" type="s"/></method>
    <method name="GetState"><arg direction="out" type="au"/></method>
    <method name="GetAttributes"><arg direction="out" type="a{ss}"/></method>
    <method name="GetApplication"><arg direction="out" type="(so)"/></method>
    <method name="GetInterfaces"><arg direction="out" type="as"/></method>
    <method name="GetRelationSet"><arg direction="out" type="a(ua(so))"/></method>
  </interface>
  <interface name="org.a11y.atspi.Component">
    <method name="Contains"><arg direction="in" type="i"/><arg direction="in" type="i"/><arg direction="in" type="u"/><arg direction="out" type="b"/></method>
    <method name="GetAccessibleAtPoint"><arg direction="in" type="i"/><arg direction="in" type="i"/><arg direction="in" type="u"/><arg direction="out" type="(so)"/></method>
    <method name="GetExtents"><arg direction="in" type="u"/><arg direction="out" type="(iiii)"/></method>
    <method name="GetPosition"><arg direction="in" type="u"/><arg direction="out" type="i"/><arg direction="out" type="i"/></method>
    <method name="GetSize"><arg direction="out" type="i"/><arg direction="out" type="i"/></method>
    <method name="GetLayer"><arg direction="out" type="u"/></method>
    <method name="GetMDIZOrder"><arg direction="out" type="n"/></method>
    <method name="GrabFocus"><arg direction="out" type="b"/></method>
    <method name="GetAlpha"><arg direction="out" type="d"/></method>
  </interface>
  <interface name="org.a11y.atspi.Action">
    <method name="GetActions"><arg direction="out" type="a(sss)"/></method>
    <method name="DoAction"><arg direction="in" type="i"/><arg direction="out" type="b"/></method>
    <method name="GetName"><arg direction="in" type="i"/><arg direction="out" type="s"/></method>
    <method name="GetDescription"><arg direction="in" type="i"/><arg direction="out" type="s"/></method>
    <method name="GetKeyBinding"><arg direction="in" type="i"/><arg direction="out" type="s"/></method>
  </interface>
  <interface name="org.a11y.atspi.Application">
    <method name="GetApplicationBusName"><arg direction="out" type="s"/></method>
    <method name="GetVersion"><arg direction="out" type="s"/></method>
    <method name="GetToolkitName"><arg direction="out" type="s"/></method>
    <method name="GetToolkitVersion"><arg direction="out" type="s"/></method>
    <method name="GetLocale"><arg direction="out" type="s"/></method>
  </interface>
  <interface name="org.a11y.atspi.Cache">
    <method name="GetItems"><arg direction="out" type="a((so)(so)(so)iiassusau)"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect"><arg direction="out" type="s"/></method>
  </interface>
  <interface name="org.freedesktop.DBus.Properties">
    <method name="Get"><arg direction="in" type="s"/><arg direction="in" type="s"/><arg direction="out" type="v"/></method>
    <method name="GetAll"><arg direction="in" type="s"/><arg direction="out" type="a{sv}"/></method>
    <method name="Set"><arg direction="in" type="s"/><arg direction="in" type="s"/><arg direction="in" type="v"/></method>
  </interface>
</node>)XML";

class Server;

class WindowBridge final : public AtspiBridge {
 public:
  WindowBridge(Server* server, uint64_t id, std::function<AccessibilityNode()> provider,
    AccessibilityActionInvoker invoker, std::string title);
  ~WindowBridge() override;
  void pump() override;
  void setTitle(std::string title) override;
  void raiseFocusEvent() override;
  void raisePropertyEvent(std::string_view relativePath, AccessibilityEvent event) override;
  [[nodiscard]] uint64_t id() const { return mId; }
  [[nodiscard]] const std::string& title() const { return mTitle; }
  [[nodiscard]] const std::function<AccessibilityNode()>& provider() const { return mProvider; }
  [[nodiscard]] const AccessibilityActionInvoker& invoker() const { return mInvoker; }

 private:
  Server* mServer = nullptr;
  uint64_t mId = 0;
  std::function<AccessibilityNode()> mProvider;
  AccessibilityActionInvoker mInvoker;
  std::string mTitle;
};

class Server {
 public:
  static Server& instance() {
    static Server server;
    return server;
  }

  ~Server() { stop(); }

  std::unique_ptr<AtspiBridge> addWindow(std::function<AccessibilityNode()> provider,
      AccessibilityActionInvoker invoker, std::string title) {
    if (!ensureStarted()) return nullptr;
    const uint64_t id = mNextId.fetch_add(1);
    auto window = std::make_unique<WindowBridge>(this, id, std::move(provider), std::move(invoker),
      std::move(title));
    mWindows.push_back(window.get());
    return window;
  }

  void removeWindow(const uint64_t id) {
    mWindows.erase(std::remove_if(mWindows.begin(), mWindows.end(),
      [id](const WindowBridge* window) { return window->id() == id; }), mWindows.end());
  }

  void pump() {
    if (mConnection == nullptr) return;
    // One non-blocking batch: a `while` here spun forever because the a11y bus keeps the connection
    // readable, starving the window loop and every test that follows.
    (void)dbus_connection_read_write_dispatch(mConnection, 0);
  }

  [[nodiscard]] const std::string& busName() const { return mBusName; }

  void emitFocus(WindowBridge* window) {
    if (window == nullptr || mConnection == nullptr) return;
    Snapshot snapshot;
    snapshot.root = window->provider()();
    std::string relative;
    std::string focusedPath;
    if (!findFocused(snapshot.root, std::string(), &relative)) return;
    focusedPath = relative.empty() ? pathForWindow(window->id())
      : pathForWindow(window->id()) + "/" + relative;
    sendStateChanged(focusedPath, "focused", 1);
  }

  void emitProperty(WindowBridge* window, const std::string_view relativePath,
      const AccessibilityEvent event) {
    if (window == nullptr || mConnection == nullptr) return;
    Snapshot snapshot;
    snapshot.root = window->provider()();
    const std::string path = relativePath.empty()
      ? pathForWindow(window->id())
      : pathForWindow(window->id()) + "/" + std::string(relativePath);
    Located located;
    if (!locate(snapshot.root, std::string(relativePath), &located)) return;
    switch (event) {
      case AccessibilityEvent::ValueChanged:
        sendPropertyChanged(path, "value", located.node->value);
        break;
      case AccessibilityEvent::ToggleChanged:
        sendStateChanged(path, "checked", located.node->selected ? 1 : 0);
        break;
      case AccessibilityEvent::Selected:
        sendStateChanged(path, "selected", 1);
        break;
      case AccessibilityEvent::EnabledChanged:
        sendStateChanged(path, "enabled", located.node->enabled ? 1 : 0);
        break;
      case AccessibilityEvent::ChildrenAdded:
        sendChildrenChanged(path, "add");
        break;
      case AccessibilityEvent::ChildrenRemoved:
        sendChildrenChanged(path, "remove");
        break;
    }
  }

  WindowBridge* findWindow(const uint64_t id) {
    for (WindowBridge* window: mWindows) {
      if (window->id() == id) return window;
    }
    return nullptr;
  }

  DBusHandlerResult handle(DBusMessage* message);

 private:
  Server() = default;

  bool ensureStarted() {
    if (std::getenv("CORAL_UI_ATSPI_DISABLE") != nullptr) return false;
    if (mStarted) return mConnection != nullptr;
    mStarted = true;
    DBusError error;
    dbus_error_init(&error);
    mSession = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (mSession == nullptr) {
      dbus_error_free(&error);
      return false;
    }
    DBusMessage* call = dbus_message_new_method_call("org.a11y.Bus", "/org/a11y/bus",
      "org.a11y.Bus", "GetAddress");
    DBusMessage* reply = call != nullptr
      ? dbus_connection_send_with_reply_and_block(mSession, call, 3000, &error) : nullptr;
    if (call != nullptr) dbus_message_unref(call);
    const char* address = nullptr;
    if (reply != nullptr) {
      DBusMessageIter iter;
      if (dbus_message_iter_init(reply, &iter) &&
          dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&iter, &address);
      }
      dbus_message_unref(reply);
    }
    if (address == nullptr) {
      dbus_error_free(&error);
      stop();
      return false;
    }
    mConnection = dbus_connection_open_private(address, &error);
    if (mConnection == nullptr) {
      dbus_error_free(&error);
      stop();
      return false;
    }
    dbus_connection_set_exit_on_disconnect(mConnection, FALSE);
    if (!dbus_bus_register(mConnection, &error)) {
      dbus_error_free(&error);
      stop();
      return false;
    }
    const char* unique = dbus_bus_get_unique_name(mConnection);
    mBusName = unique != nullptr ? unique : "";
    if (std::getenv("CORAL_UI_ATSPI_DEBUG") != nullptr) {
      std::fprintf(stderr, "coral atspi: bus=%s session=%s root=%s connected=%d\n",
        mBusName.c_str(), dbus_bus_get_unique_name(mSession), kAppPath,
        mConnection != nullptr && dbus_connection_get_is_connected(mConnection));
    }
    DBusObjectPathVTable vtable{};
    vtable.message_function = &Server::messageThunk;
    if (!dbus_connection_register_fallback(mConnection, kAccessiblePrefix, &vtable, this)) {
      stop();
      return false;
    }
    if (!embed()) {
      stop();
      return false;
    }
    return true;
  }

  bool embed() {
    DBusError error;
    dbus_error_init(&error);
    DBusMessage* message = dbus_message_new_method_call(kBusName, kRegistryPath, kSocketInterface,
      "Embed");
    if (message == nullptr) return false;
    DBusMessageIter iter;
    DBusMessageIter structure;
    dbus_message_iter_init_append(message, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &structure);
    const char* name = mBusName.c_str();
    const char* path = kAppPath;
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_STRING, &name);
    dbus_message_iter_append_basic(&structure, DBUS_TYPE_OBJECT_PATH, &path);
    dbus_message_iter_close_container(&iter, &structure);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(mConnection, message, 3000,
      &error);
    dbus_message_unref(message);
    const bool ok = reply != nullptr;
    if (reply != nullptr) dbus_message_unref(reply);
    if (!ok && std::getenv("CORAL_UI_ATSPI_DEBUG") != nullptr) {
      std::fprintf(stderr, "coral atspi: Embed failed: %s: %s\n", error.name, error.message);
    }
    dbus_error_free(&error);
    return ok;
  }

  void stop() {
    mWindows.clear();
    if (mConnection != nullptr) {
      dbus_connection_close(mConnection);
      dbus_connection_unref(mConnection);
      mConnection = nullptr;
    }
    if (mSession != nullptr) {
      dbus_connection_close(mSession);
      dbus_connection_unref(mSession);
      mSession = nullptr;
    }
    mBusName.clear();
  }

  static bool findFocused(const AccessibilityNode& node, const std::string& base,
      std::string* found) {
    if (node.focused) {
      *found = base;
      return true;
    }
    for (size_t index = 0; index < node.children.size(); ++index) {
      if (findFocused(node.children[index], childPath(base, index), found)) return true;
    }
    return false;
  }

  void sendStateChanged(const std::string& path, const std::string& detail, const int enabled) {
    DBusMessage* signal = dbus_message_new_signal(path.c_str(), "org.a11y.atspi.Event.Object",
      "StateChanged");
    if (signal == nullptr) return;
    DBusMessageIter iter;
    dbus_message_iter_init_append(signal, &iter);
    const char* text = detail.c_str();
    const char* empty = "";
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &text);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &enabled);
    int zero = 0;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &zero);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &empty);
    dbus_message_iter_close_container(&iter, &variant);
    DBusMessageIter properties;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &properties);
    dbus_message_iter_close_container(&iter, &properties);
    dbus_connection_send(mConnection, signal, nullptr);
    dbus_connection_flush(mConnection);
    dbus_message_unref(signal);
  }

  void sendPropertyChanged(const std::string& path, const std::string& property,
      const std::string& value) {
    DBusMessage* signal = dbus_message_new_signal(path.c_str(), "org.a11y.atspi.Event.Object",
      "PropertyChange");
    if (signal == nullptr) return;
    DBusMessageIter iter;
    dbus_message_iter_init_append(signal, &iter);
    const char* name = property.c_str();
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &name);
    int zero = 0;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &zero);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &zero);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant);
    const char* text = value.c_str();
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &text);
    dbus_message_iter_close_container(&iter, &variant);
    DBusMessageIter properties;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &properties);
    dbus_message_iter_close_container(&iter, &properties);
    dbus_connection_send(mConnection, signal, nullptr);
    dbus_connection_flush(mConnection);
    dbus_message_unref(signal);
  }

  void sendChildrenChanged(const std::string& path, const std::string& detail) {
    DBusMessage* signal = dbus_message_new_signal(path.c_str(), "org.a11y.atspi.Event.Object",
      "ChildrenChanged");
    if (signal == nullptr) return;
    DBusMessageIter iter;
    dbus_message_iter_init_append(signal, &iter);
    const char* text = detail.c_str();
    int zero = 0;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &text);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &zero);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &zero);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant);
    const char* empty = "";
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &empty);
    dbus_message_iter_close_container(&iter, &variant);
    DBusMessageIter properties;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &properties);
    dbus_message_iter_close_container(&iter, &properties);
    dbus_connection_send(mConnection, signal, nullptr);
    dbus_connection_flush(mConnection);
    dbus_message_unref(signal);
  }

  static DBusHandlerResult messageThunk(DBusConnection*, DBusMessage* message, void* data) {
    auto* server = static_cast<Server*>(data);
    return server != nullptr ? server->handle(message) : DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  DBusConnection* mSession = nullptr;
  DBusConnection* mConnection = nullptr;
  bool mStarted = false;
  std::string mBusName;
  std::atomic<uint64_t> mNextId{1};
  std::vector<WindowBridge*> mWindows;
};

WindowBridge::WindowBridge(Server* server, const uint64_t id,
    std::function<AccessibilityNode()> provider, AccessibilityActionInvoker invoker,
    std::string title)
    : mServer(server), mId(id), mProvider(std::move(provider)), mInvoker(std::move(invoker)),
      mTitle(std::move(title)) {}

WindowBridge::~WindowBridge() {
  if (mServer != nullptr) mServer->removeWindow(mId);
}

void WindowBridge::pump() {
  if (mServer != nullptr) mServer->pump();
}

void WindowBridge::setTitle(std::string title) {
  mTitle = std::move(title);
}

void WindowBridge::raiseFocusEvent() {
  if (mServer != nullptr) mServer->emitFocus(this);
}

void WindowBridge::raisePropertyEvent(const std::string_view relativePath,
    const AccessibilityEvent event) {
  if (mServer != nullptr) mServer->emitProperty(this, relativePath, event);
}

struct Reply {
  DBusMessage* message = nullptr;
  DBusMessageIter iter;
  bool ok = false;

  explicit Reply(DBusMessage* request) {
    message = dbus_message_new_method_return(request);
    ok = message != nullptr;
    if (ok) dbus_message_iter_init_append(message, &iter);
  }
  ~Reply() {
    if (message != nullptr) dbus_message_unref(message);
  }
  void send(DBusConnection* connection) {
    if (message != nullptr && connection != nullptr) {
      dbus_connection_send(connection, message, nullptr);
      dbus_connection_flush(connection);
    }
  }
};

void sendError(DBusConnection* connection, DBusMessage* request, const char* name,
    const char* text) {
  DBusMessage* error = dbus_message_new_error(request, name, text);
  if (error != nullptr) {
    dbus_connection_send(connection, error, nullptr);
    dbus_connection_flush(connection);
    dbus_message_unref(error);
  }
}

bool getNode(WindowBridge* window, const std::string& relative, Snapshot* snapshot,
    Located* located) {
  if (window == nullptr || snapshot == nullptr || located == nullptr) return false;
  snapshot->root = window->provider()();
  return locate(snapshot->root, relative, located);
}

std::string referencePath(const uint64_t id, const std::string& relative) {
  if (relative.empty()) return pathForWindow(id);
  return pathForWindow(id) + "/" + relative;
}

void appendChildren(Reply& reply, const uint64_t id, const std::string& base,
    const AccessibilityNode& node, const std::string& busName) {
  DBusMessageIter array;
  dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "(so)", &array);
  for (size_t index = 0; index < node.children.size(); ++index) {
    appendObjectRef(&array, busName, referencePath(id, childPath(base, index)));
  }
  dbus_message_iter_close_container(&reply.iter, &array);
}

void appendState(Reply& reply, const AccessibilityNode& node, const bool windowRoot) {
  uint32_t words[2] = { 0, 0 };
  const auto set = [&words](const unsigned bit) {
    words[bit / 32] |= (1u << (bit % 32));
  };
  if (node.enabled || windowRoot) {
    set(kStateEnabled);
    set(kStateSensitive);
  }
  if (node.focusable || windowRoot) set(kStateFocusable);
  if (node.focused) set(kStateFocused);
  if (node.selected) set(kStateSelected);
  if (node.role == AccessibilityRole::CheckBox || node.role == AccessibilityRole::Radio) {
    if (node.selected) set(kStateChecked);
  }
  if (node.editable) set(kStateEditable);
  set(kStateShowing);
  set(kStateVisible);
  DBusMessageIter array;
  dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "u", &array);
  for (const uint32_t word: words) dbus_message_iter_append_basic(&array, DBUS_TYPE_UINT32, &word);
  dbus_message_iter_close_container(&reply.iter, &array);
}

void appendInterfaces(Reply& reply, const AccessibilityNode& node, const bool appRoot,
    const bool windowRoot) {
  DBusMessageIter array;
  dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "s", &array);
  const auto add = [&array](const char* name) {
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &name);
  };
  add("org.a11y.atspi.Accessible");
  add("org.a11y.atspi.Component");
  if (appRoot) add("org.a11y.atspi.Application");
  const std::vector<std::string> interfaces = atspiInterfaces(node);
  for (const std::string& name: interfaces) {
    // Only advertise what this bridge really answers. Text/Value/Selection/Table are mapped in
    // `AccessibilityAtSpi` but not implemented here yet; advertising them made clients call methods
    // that returned no reply.
    if (name == "org.a11y.atspi.Action") add(name.c_str());
  }
  (void)windowRoot;
  dbus_message_iter_close_container(&reply.iter, &array);
}

std::array<uint32_t, 2> stateWords(const AccessibilityNode& node, const bool windowRoot) {
  std::array<uint32_t, 2> words{ 0, 0 };
  const auto set = [&words](const unsigned bit) {
    words[bit / 32] |= (1u << (bit % 32));
  };
  if (node.enabled || windowRoot) {
    set(kStateEnabled);
    set(kStateSensitive);
  }
  if (node.focusable || windowRoot) set(kStateFocusable);
  if (node.focused) set(kStateFocused);
  if (node.selected) set(kStateSelected);
  if (node.role == AccessibilityRole::CheckBox || node.role == AccessibilityRole::Radio) {
    if (node.selected) set(kStateChecked);
  }
  if (node.editable) set(kStateEditable);
  set(kStateShowing);
  set(kStateVisible);
  return words;
}

void appendInterfaceNames(DBusMessageIter* parent, const AccessibilityNode& node,
    const bool appRoot) {
  DBusMessageIter array;
  dbus_message_iter_open_container(parent, DBUS_TYPE_ARRAY, "s", &array);
  const auto add = [&array](const char* name) {
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &name);
  };
  add("org.a11y.atspi.Accessible");
  add("org.a11y.atspi.Component");
  if (appRoot) add("org.a11y.atspi.Application");
  const std::vector<std::string> interfaces = atspiInterfaces(node);
  if (std::find(interfaces.begin(), interfaces.end(), "org.a11y.atspi.Action") !=
      interfaces.end()) {
    add("org.a11y.atspi.Action");
  }
  dbus_message_iter_close_container(parent, &array);
}

void appendCacheItem(DBusMessageIter* array, const std::string& bus,
    const std::string& mainPath, const std::string& appPath, const std::string& parentPath,
    const int index, const int childCount, const AccessibilityNode& node, const bool appRoot,
    const bool windowRoot, const std::string& name, const uint32_t role) {
  DBusMessageIter entry;
  dbus_message_iter_open_container(array, DBUS_TYPE_STRUCT, nullptr, &entry);
  appendObjectRef(&entry, bus, mainPath);
  appendObjectRef(&entry, bus, appPath);
  appendObjectRef(&entry, bus, parentPath);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_INT32, &index);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_INT32, &childCount);
  appendInterfaceNames(&entry, node, appRoot);
  const char* text = name.c_str();
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &text);
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_UINT32, &role);
  const std::string& description = node.description;
  const char* descriptionText = description.c_str();
  dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &descriptionText);
  const std::array<uint32_t, 2> states = stateWords(node, windowRoot);
  DBusMessageIter stateArray;
  dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "u", &stateArray);
  for (const uint32_t word: states) {
    dbus_message_iter_append_basic(&stateArray, DBUS_TYPE_UINT32, &word);
  }
  dbus_message_iter_close_container(&entry, &stateArray);
  dbus_message_iter_close_container(array, &entry);
}

void appendCacheChildren(DBusMessageIter* array, const std::string& bus, const uint64_t windowId,
    const std::string& base, const AccessibilityNode& parent) {
  for (size_t index = 0; index < parent.children.size(); ++index) {
    const AccessibilityNode& node = parent.children[index];
    const std::string relative = childPath(base, index);
    appendCacheItem(array, bus, referencePath(windowId, relative), pathForWindow(windowId),
      base.empty() ? pathForWindow(windowId) : referencePath(windowId, base),
      static_cast<int>(index), static_cast<int>(node.children.size()), node, false, false,
      atspiName(node), roleNumber(node.role));
    appendCacheChildren(array, bus, windowId, relative, node);
  }
}

std::string roleName(const uint32_t role) {
  // The string is only a convenience for clients; numeric roles are authoritative.
  switch (role) {
    case 7: return "check box";
    case 16: return "dialog";
    case 23: return "frame";
    case 29: return "label";
    case 31: return "list";
    case 32: return "list item";
    case 33: return "menu";
    case 39: return "panel";
    case 43: return "push button";
    case 44: return "radio button";
    case 49: return "scroll pane";
    case 51: return "slider";
    case 55: return "table";
    case 61: return "text";
    case 65: return "tree";
    case 75: return "application";
    case 79: return "entry";
    default: return "unknown";
  }
}

} // namespace

DBusHandlerResult Server::handle(DBusMessage* message) {
  if (message == nullptr) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char* path = dbus_message_get_path(message);
  if (path == nullptr) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Cache", "GetItems")) {
    Reply reply(message);
    DBusMessageIter array;
    dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY,
      "((so)(so)(so)iiassusau)", &array);
    AccessibilityNode applicationRoot;
    applicationRoot.role = AccessibilityRole::Generic;
    appendCacheItem(&array, mBusName, kAppPath, kAppPath, kAppPath, -1,
      static_cast<int>(mWindows.size()), applicationRoot, true, false, "Coral", 75);
    for (WindowBridge* window: mWindows) {
      Snapshot snapshot;
      snapshot.root = window->provider()();
      const std::string windowPath = pathForWindow(window->id());
      appendCacheItem(&array, mBusName, windowPath, kAppPath, kAppPath, -1,
        static_cast<int>(snapshot.root.children.size()), snapshot.root, false, true,
        window->title().empty() ? "Coral window" : window->title(), 23);
      appendCacheChildren(&array, mBusName, window->id(), std::string(), snapshot.root);
    }
    dbus_message_iter_close_container(&reply.iter, &array);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Introspectable", "Introspect")) {
    Reply reply(message);
    const char* xml = kIntrospectionXml;
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &xml);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Cache", "GetItems")) {
    Reply reply(message);
    DBusMessageIter array;
    dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY,
      "((so)(so)(so)iiassusau)", &array);
    dbus_message_iter_close_container(&reply.iter, &array);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "GetAll")) {
    Reply reply(message);
    DBusMessageIter dict;
    dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&reply.iter, &dict);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "Get") ||
      dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "Set")) {
    sendError(mConnection, message, "org.freedesktop.DBus.Error.NotSupported",
      "property is not supported");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  const std::string objectPath(path);
  const bool appRoot = objectPath == kAppPath;
  WindowBridge* window = nullptr;
  std::string relative;
  if (isPrefix(objectPath, kWindowPrefix)) {
    size_t start = std::strlen(kWindowPrefix);
    size_t end = objectPath.find('/', start);
    const std::string idText = objectPath.substr(start, end == std::string::npos
      ? std::string::npos : end - start);
    uint64_t id = 0;
    for (const char ch: idText) {
      if (ch < '0' || ch > '9') return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
      id = id * 10 + static_cast<uint64_t>(ch - '0');
    }
    window = findWindow(id);
    if (window == nullptr) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if (end != std::string::npos) relative = objectPath.substr(end + 1);
  } else if (!appRoot) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  Snapshot snapshot;
  Located located;
  if (!appRoot && !getNode(window, relative, &snapshot, &located)) {
    sendError(mConnection, message, "org.freedesktop.DBus.Error.InvalidArgs", "no such accessible");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetChildren")) {
    Reply reply(message);
    if (appRoot) {
      DBusMessageIter array;
      dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "(so)", &array);
      for (WindowBridge* entry: mWindows) {
        appendObjectRef(&array, mBusName, pathForWindow(entry->id()));
      }
      dbus_message_iter_close_container(&reply.iter, &array);
    } else {
      appendChildren(reply, window->id(), relative, *located.node, mBusName);
    }
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetChildCount")) {
    Reply reply(message);
    int count = 0;
    if (appRoot) {
      count = static_cast<int>(mWindows.size());
    } else {
      count = static_cast<int>(located.node->children.size());
    }
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT32, &count);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetChildAtIndex")) {
    int index = -1;
    if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_INT32, &index, DBUS_TYPE_INVALID) ||
        index < 0) {
      sendError(mConnection, message, "org.freedesktop.DBus.Error.InvalidArgs", "bad index");
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    Reply reply(message);
    if (appRoot) {
      if (static_cast<size_t>(index) < mWindows.size()) {
        appendObjectRef(&reply.iter, mBusName, pathForWindow(mWindows[static_cast<size_t>(index)]->id()));
      } else {
        appendObjectRef(&reply.iter, mBusName, kNullPath);
      }
    } else if (static_cast<size_t>(index) < located.node->children.size()) {
      appendObjectRef(&reply.iter, mBusName,
        referencePath(window->id(), childPath(relative, static_cast<size_t>(index))));
    } else {
      appendObjectRef(&reply.iter, mBusName, kNullPath);
    }
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetIndexInParent")) {
    Reply reply(message);
    int index = appRoot ? -1 : located.index;
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT32, &index);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetRole")) {
    Reply reply(message);
    uint32_t role = 75; // ATSPI_ROLE_APPLICATION
    if (!appRoot) {
      role = relative.empty() ? 23u /* frame */ : roleNumber(located.node->role);
    }
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_UINT32, &role);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetRoleName") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Accessible",
        "GetLocalizedRoleName")) {
    Reply reply(message);
    const uint32_t role = appRoot ? 75u : (relative.empty() ? 23u : roleNumber(located.node->role));
    const std::string name = roleName(role);
    const char* text = name.c_str();
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &text);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetName")) {
    Reply reply(message);
    std::string name = "Coral";
    if (!appRoot) {
      if (!window->title().empty()) name = window->title();
      if (relative.empty()) {
        name = window->title().empty() ? "Coral window" : window->title();
      } else {
        name = atspiName(*located.node);
      }
    }
    const char* text = name.c_str();
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &text);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetDescription")) {
    Reply reply(message);
    const std::string description = appRoot ? "" : located.node->description;
    const char* text = description.c_str();
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &text);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetState")) {
    Reply reply(message);
    appendState(reply, appRoot ? AccessibilityNode{} : *located.node, relative.empty());
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetInterfaces")) {
    Reply reply(message);
    const AccessibilityNode node = appRoot ? AccessibilityNode{} : *located.node;
    appendInterfaces(reply, node, appRoot, relative.empty());
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetApplication")) {
    Reply reply(message);
    appendObjectRef(&reply.iter, mBusName, kAppPath);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetParent")) {
    Reply reply(message);
    if (appRoot) {
      // The application root's parent is the registry's desktop root, not a null reference; Orca
      // uses this link to decide whether an application entry is still alive.
      appendObjectRef(&reply.iter, "org.a11y.atspi.Registry", kAppPath);
    } else if (relative.empty()) {
      appendObjectRef(&reply.iter, mBusName, kAppPath);
    } else {
      appendObjectRef(&reply.iter, mBusName, referencePath(window->id(), located.parent));
    }
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetAttributes") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Accessible", "GetRelationSet")) {
    Reply reply(message);
    if (dbus_message_get_member(message) != nullptr &&
        std::strcmp(dbus_message_get_member(message), "GetAttributes") == 0) {
      DBusMessageIter dict;
      dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "{ss}", &dict);
      dbus_message_iter_close_container(&reply.iter, &dict);
    } else {
      DBusMessageIter array;
      dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "(ua(so))", &array);
      dbus_message_iter_close_container(&reply.iter, &array);
    }
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  // Component
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetExtents")) {
    Reply reply(message);
    const Rect bounds = appRoot ? Rect{} : located.node->bounds;
    DBusMessageIter structure;
    dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_STRUCT, nullptr, &structure);
    const int values[4] = { static_cast<int>(bounds.getLeft()), static_cast<int>(bounds.getTop()),
      static_cast<int>(bounds.getWidth()), static_cast<int>(bounds.getHeight()) };
    for (const int value: values) dbus_message_iter_append_basic(&structure, DBUS_TYPE_INT32, &value);
    dbus_message_iter_close_container(&reply.iter, &structure);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetPosition")) {
    Reply reply(message);
    const Rect bounds = appRoot ? Rect{} : located.node->bounds;
    const int x = static_cast<int>(bounds.getLeft());
    const int y = static_cast<int>(bounds.getTop());
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT32, &x);
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT32, &y);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetSize")) {
    Reply reply(message);
    const Rect bounds = appRoot ? Rect{} : located.node->bounds;
    const int width = static_cast<int>(bounds.getWidth());
    const int height = static_cast<int>(bounds.getHeight());
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT32, &width);
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT32, &height);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Component", "Contains")) {
    int x = 0;
    int y = 0;
    uint32_t coord = 0;
    dbus_bool_t inside = FALSE;
    if (dbus_message_get_args(message, nullptr, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y,
        DBUS_TYPE_UINT32, &coord, DBUS_TYPE_INVALID)) {
      const Rect bounds = appRoot ? Rect{} : located.node->bounds;
      inside = !appRoot && x >= static_cast<int>(bounds.getLeft()) &&
        y >= static_cast<int>(bounds.getTop()) &&
        x < static_cast<int>(bounds.getLeft() + bounds.getWidth()) &&
        y < static_cast<int>(bounds.getTop() + bounds.getHeight());
    }
    Reply reply(message);
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_BOOLEAN, &inside);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetAccessibleAtPoint")) {
    Reply reply(message);
    appendObjectRef(&reply.iter, mBusName, appRoot ? kAppPath : referencePath(window->id(), relative));
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetLayer") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetMDIZOrder") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GrabFocus") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Component", "GetAlpha")) {
    Reply reply(message);
    const char* member = dbus_message_get_member(message);
    if (member != nullptr && std::strcmp(member, "GetLayer") == 0) {
      const uint32_t layer = 0;
      dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_UINT32, &layer);
    } else if (member != nullptr && std::strcmp(member, "GetMDIZOrder") == 0) {
      const int16_t order = 0;
      dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_INT16, &order);
    } else if (member != nullptr && std::strcmp(member, "GrabFocus") == 0) {
      const dbus_bool_t grabbed = FALSE;
      dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_BOOLEAN, &grabbed);
    } else {
      const double alpha = 1.0;
      dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_DOUBLE, &alpha);
    }
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  // Action
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Action", "GetActions")) {
    Reply reply(message);
    DBusMessageIter array;
    dbus_message_iter_open_container(&reply.iter, DBUS_TYPE_ARRAY, "(sss)", &array);
    const bool actionable = !appRoot && (static_cast<uint8_t>(located.node->actions) != 0);
    if (actionable) {
      DBusMessageIter structure;
      dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, nullptr, &structure);
      const char* name = (static_cast<uint8_t>(located.node->actions) &
        static_cast<uint8_t>(AccessibilityAction::Toggle)) != 0 ? "toggle" : "click";
      const char* description = name;
      const char* binding = "";
      dbus_message_iter_append_basic(&structure, DBUS_TYPE_STRING, &name);
      dbus_message_iter_append_basic(&structure, DBUS_TYPE_STRING, &description);
      dbus_message_iter_append_basic(&structure, DBUS_TYPE_STRING, &binding);
      dbus_message_iter_close_container(&array, &structure);
    }
    dbus_message_iter_close_container(&reply.iter, &array);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Action", "DoAction")) {
    int index = 0;
    dbus_bool_t done = FALSE;
    if (!appRoot && dbus_message_get_args(message, nullptr, DBUS_TYPE_INT32, &index,
        DBUS_TYPE_INVALID) && index == 0) {
      const AccessibilityAction action = (static_cast<uint8_t>(located.node->actions) &
        static_cast<uint8_t>(AccessibilityAction::Toggle)) != 0
        ? AccessibilityAction::Toggle : AccessibilityAction::Invoke;
      done = window->invoker() ? window->invoker()(located.node->path, action, "") : FALSE;
    }
    Reply reply(message);
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_BOOLEAN, &done);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Action", "GetName") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Action", "GetDescription") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Action", "GetKeyBinding")) {
    Reply reply(message);
    const char* text = dbus_message_is_method_call(message, "org.a11y.atspi.Action", "GetName")
      ? "click" : "";
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &text);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  // Application (only meaningful on the app root, but harmless elsewhere).
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetApplicationBusName")) {
    Reply reply(message);
    const char* text = mBusName.c_str();
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &text);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  if (dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetVersion") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetToolkitName") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetToolkitVersion") ||
      dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetLocale")) {
    Reply reply(message);
    const char* text = "1.0";
    if (dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetToolkitName")) {
      text = "CoralUI";
    } else if (dbus_message_is_method_call(message, "org.a11y.atspi.Application", "GetLocale")) {
      text = "C";
    }
    dbus_message_iter_append_basic(&reply.iter, DBUS_TYPE_STRING, &text);
    reply.send(mConnection);
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

std::unique_ptr<AtspiBridge> createAtspiBridge(std::function<AccessibilityNode()> provider,
    AccessibilityActionInvoker invoker, std::string title) {
  return Server::instance().addWindow(std::move(provider), std::move(invoker), std::move(title));
}

} // namespace coral::ui::detail
