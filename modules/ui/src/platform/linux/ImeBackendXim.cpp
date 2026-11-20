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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdlib>
#include <string>

namespace coral::ui::detail {

namespace {

class XimIme final : public ImeBackend {
 public:
  XimIme(void* display, const unsigned long window, ImeHost* host)
      : mDisplay(static_cast<Display*>(display)), mWindow(static_cast<::Window>(window)),
        mHost(host) {}

  ~XimIme() override { shutdown(); }

  bool initialize() override {
    if (mDisplay == nullptr || mWindow == 0 || mHost == nullptr) return false;
    XSetLocaleModifiers("");
    mInputMethod = XOpenIM(mDisplay, nullptr, nullptr, nullptr);
    if (mInputMethod == nullptr) return false;
    XIMCallback start{ reinterpret_cast<XPointer>(this), &XimIme::preeditStart };
    XIMCallback done{ reinterpret_cast<XPointer>(this), &XimIme::preeditDone };
    XIMCallback draw{ reinterpret_cast<XPointer>(this), &XimIme::preeditDraw };
    XVaNestedList preedit = XVaCreateNestedList(0,
      XNPreeditStartCallback, &start, XNPreeditDoneCallback, &done,
      XNPreeditDrawCallback, &draw, nullptr);
    mInputContext = XCreateIC(mInputMethod, XNInputStyle,
      XIMPreeditCallbacks | XIMStatusNothing, XNClientWindow, mWindow, XNFocusWindow, mWindow,
      XNPreeditAttributes, preedit, nullptr);
    XFree(preedit);
    if (mInputContext == nullptr) {
      XCloseIM(mInputMethod);
      mInputMethod = nullptr;
      return false;
    }
    return true;
  }

  void shutdown() override {
    if (mInputContext != nullptr) {
      XDestroyIC(mInputContext);
      mInputContext = nullptr;
    }
    if (mInputMethod != nullptr) {
      XCloseIM(mInputMethod);
      mInputMethod = nullptr;
    }
  }

  bool filterEvent(void* nativeEvent) override {
    return nativeEvent != nullptr && mInputContext != nullptr
      ? XFilterEvent(static_cast<XEvent*>(nativeEvent), mWindow) != 0
      : false;
  }

  int lookupString(void* nativeKeyEvent, char* utf8, const int capacity, uint32_t* keysym,
      int* status) override {
    auto* key = static_cast<XKeyEvent*>(nativeKeyEvent);
    if (key == nullptr || utf8 == nullptr || capacity <= 0) return 0;
    KeySym symbol = NoSymbol;
    Status lookup = 0;
    int count = 0;
    if (mInputContext != nullptr) {
      count = Xutf8LookupString(mInputContext, key, utf8, capacity, &symbol, &lookup);
    } else {
      XComposeStatus compose{};
      count = XLookupString(key, utf8, capacity, &symbol, &compose);
    }
    if (keysym != nullptr) *keysym = static_cast<uint32_t>(symbol);
    if (status != nullptr) *status = lookup;
    return count;
  }

  void focusIn() override {
    if (mInputContext != nullptr) XSetICFocus(mInputContext);
  }
  void focusOut() override {
    if (mInputContext != nullptr) XUnsetICFocus(mInputContext);
  }
  void setCursorRect(const Rect&) override {
    // The composition is drawn inline by TextInput at the caret; XIM's preedit window is not used.
  }
  [[nodiscard]] const char* name() const override { return "xim"; }

 private:
  static void preeditStart(XIM, XPointer, XPointer) {}

  static void preeditDone(XIM, XPointer clientData, XPointer) {
    auto* self = reinterpret_cast<XimIme*>(clientData);
    if (self != nullptr && self->mHost != nullptr) self->mHost->onImeComposition({});
  }

  static void preeditDraw(XIM, XPointer clientData, XPointer callData) {
    auto* self = reinterpret_cast<XimIme*>(clientData);
    auto* text = reinterpret_cast<XIMText*>(callData);
    if (self == nullptr || self->mHost == nullptr || text == nullptr) return;
    std::string value;
    if (text->encoding_is_wchar) {
      const size_t length = text->string.wide_char != nullptr
        ? std::wcstombs(nullptr, text->string.wide_char, 0) : 0;
      if (length != static_cast<size_t>(-1) && length > 0) {
        value.resize(length);
        std::wcstombs(value.data(), text->string.wide_char, length);
      }
    } else if (text->string.multi_byte != nullptr && text->length > 0) {
      value.assign(text->string.multi_byte, static_cast<size_t>(text->length));
    }
    self->mHost->onImeComposition(value);
  }

  Display* mDisplay = nullptr;
  ::Window mWindow = 0;
  ImeHost* mHost = nullptr;
  XIM mInputMethod = nullptr;
  XIC mInputContext = nullptr;
};

} // namespace

std::unique_ptr<ImeBackend> makeXimIme(void* display, const unsigned long window, ImeHost* host) {
  auto backend = std::make_unique<XimIme>(display, window, host);
  if (!backend->initialize()) return nullptr;
  return backend;
}

} // namespace coral::ui::detail
