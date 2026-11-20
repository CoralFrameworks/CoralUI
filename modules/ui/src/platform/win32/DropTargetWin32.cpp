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

#include "../../WindowPlatform.h"

#include "DropTargetWin32.h"

#include <ole2.h>
#include <shellapi.h>

#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui::detail {

namespace {

[[nodiscard]] std::string utf16ToUtf8(const wchar_t* text, const size_t length) {
  if (text == nullptr || length == 0) return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), nullptr, 0,
    nullptr, nullptr);
  if (bytes <= 0) return {};
  std::string result(static_cast<size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(length), result.data(), bytes, nullptr,
    nullptr);
  return result;
}

[[nodiscard]] std::string readWideMedium(const STGMEDIUM& medium) {
  const auto* wide = static_cast<const wchar_t*>(GlobalLock(medium.hGlobal));
  if (wide == nullptr) return {};
  const SIZE_T size = GlobalSize(medium.hGlobal) / sizeof(wchar_t);
  size_t length = 0;
  while (length + 1 < size && wide[length] != L'\0') ++length;
  const std::string result = utf16ToUtf8(wide, length);
  GlobalUnlock(medium.hGlobal);
  return result;
}

[[nodiscard]] std::string readStringMedium(const STGMEDIUM& medium) {
  const auto* bytes = static_cast<const char*>(GlobalLock(medium.hGlobal));
  if (bytes == nullptr) return {};
  const SIZE_T size = GlobalSize(medium.hGlobal);
  size_t length = 0;
  while (length + 1 < size && bytes[length] != '\0') ++length;
  std::string result(bytes, length);
  GlobalUnlock(medium.hGlobal);
  return result;
}

/** Reads the data object as the same `DragPayload` an internal drag uses. */
[[nodiscard]] DragPayload readPayload(IDataObject* dataObject) {
  if (dataObject == nullptr) return {};
  FORMATETC format{ CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  STGMEDIUM medium{};
  if (dataObject->GetData(&format, &medium) == S_OK) {
    const auto drop = static_cast<HDROP>(medium.hGlobal);
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
    std::vector<std::string> paths;
    paths.reserve(count);
    for (UINT index = 0; index < count; ++index) {
      const UINT length = DragQueryFileW(drop, index, nullptr, 0);
      if (length == 0) continue;
      std::wstring path(static_cast<size_t>(length) + 1, L'\0');
      if (DragQueryFileW(drop, index, path.data(), length + 1) == 0) continue;
      path.resize(length);
      paths.push_back(utf16ToUtf8(path.c_str(), path.size()));
    }
    ReleaseStgMedium(&medium);
    if (!paths.empty()) return DragPayload::files(std::move(paths));
  }

  format = FORMATETC{ CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  if (dataObject->GetData(&format, &medium) == S_OK) {
    const std::string text = readWideMedium(medium);
    ReleaseStgMedium(&medium);
    if (!text.empty()) return DragPayload::text(text);
  }

  static const CLIPFORMAT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
  if (htmlFormat != 0) {
    format = FORMATETC{ htmlFormat, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    if (dataObject->GetData(&format, &medium) == S_OK) {
      const std::string html = readStringMedium(medium);
      ReleaseStgMedium(&medium);
      if (!html.empty()) return DragPayload::custom("text/html", html);
    }
  }
  return {};
}

[[nodiscard]] DWORD toOleEffect(const DragEffect effect) {
  switch (effect) {
    case DragEffect::Copy: return DROPEFFECT_COPY;
    case DragEffect::Move: return DROPEFFECT_MOVE;
    case DragEffect::Link: return DROPEFFECT_LINK;
    case DragEffect::None:
    default: return DROPEFFECT_NONE;
  }
}

/**
 * The COM object OLE talks to.
 *
 * Refcount starts at one for the wrapper; `RegisterDragDrop` adds its own reference and
 * `RevokeDragDrop` releases it, so the wrapper's `Release` is the last one.
 */
class DropTargetImpl final : public IDropTarget {
 public:
  DropTargetImpl(HWND window, WindowHost* host) : mWindow(window), mHost(host) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (out == nullptr) return E_POINTER;
    if (iid == IID_IUnknown || iid == IID_IDropTarget) {
      *out = static_cast<IDropTarget*>(this);
      AddRef();
      return S_OK;
    }
    *out = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return ++mRefs;
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG refs = --mRefs;
    if (refs == 0) delete this;
    return refs;
  }

  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* dataObject, DWORD, POINTL point,
      DWORD* effect) override {
    mPayload = readPayload(dataObject);
    if (mHost != nullptr && !mPayload.empty()) {
      DragEvent event = makeEvent(point);
      *effect = toOleEffect(mHost->dispatchDragEnter(event));
    } else {
      *effect = DROPEFFECT_NONE;
      if (mHost != nullptr) mHost->dispatchDragLeave();
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL point, DWORD* effect) override {
    if (mHost == nullptr || mPayload.empty()) {
      *effect = DROPEFFECT_NONE;
      return S_OK;
    }
    DragEvent event = makeEvent(point);
    *effect = toOleEffect(mHost->dispatchDragOver(event));
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE DragLeave() override {
    if (mHost != nullptr) mHost->dispatchDragLeave();
    mPayload = {};
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Drop(IDataObject*, DWORD, POINTL point, DWORD* effect) override {
    if (mHost == nullptr || mPayload.empty()) {
      *effect = DROPEFFECT_NONE;
      return S_OK;
    }
    DragEvent event = makeEvent(point);
    *effect = toOleEffect(mHost->dispatchDragDrop(event));
    mPayload = {};
    return S_OK;
  }

 private:
  [[nodiscard]] DragEvent makeEvent(const POINTL& point) const {
    POINT client{ point.x, point.y };
    ScreenToClient(mWindow, &client);
    DragEvent event;
    event.position = { static_cast<float>(client.x), static_cast<float>(client.y) };
    event.payload = mPayload;
    event.external = true;
    return event;
  }

  std::atomic<ULONG> mRefs{ 1 };
  HWND mWindow = nullptr;
  WindowHost* mHost = nullptr;
  DragPayload mPayload;
};

} // namespace

std::shared_ptr<Win32DropTarget> createWin32DropTarget(const HWND window, WindowHost* host) {
  if (window == nullptr) return nullptr;
  // OLE is refcounted per thread: a false result here means the thread was already initialized,
  // which is fine (the matching OleUninitialize is refcounted too).
  const HRESULT initialized = OleInitialize(nullptr);
  if (initialized != S_OK && initialized != S_FALSE) return nullptr;

  auto* target = new DropTargetImpl(window, host);
  if (RegisterDragDrop(window, target) != S_OK) {
    target->Release();
    OleUninitialize();
    return nullptr;
  }

  struct Registration final : Win32DropTarget {
    ~Registration() override {
      if (window != nullptr) RevokeDragDrop(window);
      OleUninitialize();
      target->Release();
    }
    HWND window = nullptr;
    DropTargetImpl* target = nullptr;
  };
  auto registration = std::make_shared<Registration>();
  registration->window = window;
  registration->target = target;
  return registration;
}

} // namespace coral::ui::detail
