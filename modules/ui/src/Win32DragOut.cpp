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

#include <ui/Win32DragOut.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstring>

namespace coral::ui {

namespace {
/** The CF_HDROP header (same layout as shellapi's DROPFILES, which is not always visible). */
struct DropFilesHeader {
  DWORD pFiles;
  POINT pt;
  BOOL fNC;
  BOOL fWide;
};


std::string toUtf16Block(const std::string& utf8, const bool terminate) {
  if (utf8.empty()) {
    return terminate ? std::string(sizeof(wchar_t) * 2, '\0') : std::string{};
  }
  const int characters = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
    static_cast<int>(utf8.size()), nullptr, 0);
  if (characters <= 0) return {};
  std::wstring wide(static_cast<size_t>(characters), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), characters);
  if (terminate) wide.push_back(L'\0');
  std::string block(wide.size() * sizeof(wchar_t), '\0');
  std::memcpy(block.data(), wide.data(), block.size());
  return block;
}

/** DROPFILES followed by the double-NUL-terminated wide path list. */
std::string toDropFiles(const std::vector<std::string>& paths) {
  DropFilesHeader header{};
  header.pFiles = sizeof(DropFilesHeader);
  header.fWide = TRUE;
  std::string block(reinterpret_cast<const char*>(&header), sizeof(header));
  if (paths.empty()) {
    block.append(sizeof(wchar_t), '\0');
    return block;
  }
  for (const std::string& path: paths) {
    block += toUtf16Block(path, true);
  }
  block.append(sizeof(wchar_t), '\0');
  return block;
}

unsigned int registeredFormat(const std::string& mime) {
  const int characters = MultiByteToWideChar(CP_UTF8, 0, mime.data(),
    static_cast<int>(mime.size()), nullptr, 0);
  if (characters <= 0) return 0;
  std::wstring wide(static_cast<size_t>(characters), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, mime.data(), static_cast<int>(mime.size()), wide.data(), characters);
  return ::RegisterClipboardFormatW(wide.c_str());
}

} // namespace

std::vector<Win32DragFormat> win32DragFormats(const DragPayload& payload) {
  std::vector<Win32DragFormat> formats;
  if (payload.hasType("text/plain")) {
    formats.push_back(Win32DragFormat{ CF_UNICODETEXT, toUtf16Block(payload.data(), true), true });
  } else if (payload.hasType("text/uri-list")) {
    std::vector<std::string> paths = payload.paths();
    if (paths.empty() && !payload.data().empty()) {
      size_t offset = 0;
      while (offset < payload.data().size()) {
        size_t end = payload.data().find('\n', offset);
        if (end == std::string::npos) end = payload.data().size();
        std::string line = payload.data().substr(offset, end - offset);
        offset = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.front() != '#') paths.push_back(std::move(line));
      }
    }
    formats.push_back(Win32DragFormat{ CF_HDROP, toDropFiles(paths), true });
  } else if (!payload.empty()) {
    const unsigned int format = registeredFormat(payload.type());
    if (format != 0) formats.push_back(Win32DragFormat{ format, payload.data(), false });
  }
  if (payload.hasSecondary()) {
    const unsigned int format = registeredFormat(payload.secondaryType());
    if (format != 0) formats.push_back(Win32DragFormat{ format, payload.secondaryData(), false });
  }
  return formats;
}


Win32DragDataObject::Win32DragDataObject(std::vector<Win32DragFormat> formats)
    : mFormats(std::move(formats)) {}

Win32DragDataObject* Win32DragDataObject::create(const DragPayload& payload) {
  auto* object = new Win32DragDataObject(win32DragFormats(payload));
  return object;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::QueryInterface(const REFIID iid, void** object) {
  if (object == nullptr) return E_INVALIDARG;
  if (iid == IID_IUnknown || iid == IID_IDataObject) {
    *object = static_cast<IDataObject*>(this);
    AddRef();
    return S_OK;
  }
  *object = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Win32DragDataObject::AddRef() {
  return ++mReferenceCount;
}

ULONG STDMETHODCALLTYPE Win32DragDataObject::Release() {
  const ULONG count = --mReferenceCount;
  if (count == 0) delete this;
  return count;
}

const Win32DragFormat* Win32DragDataObject::find(const FORMATETC& format) const {
  if ((format.tymed & TYMED_HGLOBAL) == 0) return nullptr;
  for (const Win32DragFormat& entry: mFormats) {
    if (entry.format == format.cfFormat) return &entry;
  }
  return nullptr;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::GetData(FORMATETC* format, STGMEDIUM* medium) {
  if (format == nullptr || medium == nullptr) return E_INVALIDARG;
  const Win32DragFormat* entry = find(*format);
  if (entry == nullptr) return DV_E_FORMATETC;
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, entry->data.size());
  if (memory == nullptr) return E_OUTOFMEMORY;
  void* target = GlobalLock(memory);
  if (target == nullptr) {
    GlobalFree(memory);
    return E_OUTOFMEMORY;
  }
  std::memcpy(target, entry->data.data(), entry->data.size());
  GlobalUnlock(memory);
  medium->tymed = TYMED_HGLOBAL;
  medium->hGlobal = memory;
  medium->pUnkForRelease = nullptr;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::GetDataHere(FORMATETC*, STGMEDIUM*) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::QueryGetData(FORMATETC* format) {
  if (format == nullptr) return E_INVALIDARG;
  return find(*format) != nullptr ? S_OK : DV_E_FORMATETC;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) {
  if (out != nullptr) out->ptd = nullptr;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::SetData(FORMATETC*, STGMEDIUM*, BOOL) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::EnumFormatEtc(DWORD, IEnumFORMATETC** enumerator) {
  if (enumerator != nullptr) *enumerator = nullptr;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) {
  return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::DUnadvise(DWORD) {
  return OLE_E_ADVISENOTSUPPORTED;
}

HRESULT STDMETHODCALLTYPE Win32DragDataObject::EnumDAdvise(IEnumSTATDATA**) {
  return OLE_E_ADVISENOTSUPPORTED;
}

Win32DragAction win32DragAction(const bool escapePressed, const bool primaryDown,
    const bool secondaryDown) {
  if (escapePressed) return Win32DragAction::Cancel;
  if (!primaryDown && !secondaryDown) return Win32DragAction::Drop;
  return Win32DragAction::Continue;
}

HRESULT STDMETHODCALLTYPE Win32DropSource::QueryInterface(const REFIID iid, void** object) {
  if (object == nullptr) return E_INVALIDARG;
  if (iid == IID_IUnknown || iid == IID_IDropSource) {
    *object = static_cast<IDropSource*>(this);
    AddRef();
    return S_OK;
  }
  *object = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE Win32DropSource::AddRef() {
  return ++mReferenceCount;
}

ULONG STDMETHODCALLTYPE Win32DropSource::Release() {
  const ULONG count = --mReferenceCount;
  if (count == 0) delete this;
  return count;
}

HRESULT STDMETHODCALLTYPE Win32DropSource::QueryContinueDrag(const BOOL escape,
    const DWORD keys) {
  const bool primaryDown = (keys & MK_LBUTTON) != 0;
  const bool secondaryDown = (keys & MK_RBUTTON) != 0;
  switch (win32DragAction(escape != FALSE, primaryDown, secondaryDown)) {
    case Win32DragAction::Cancel: return DRAGDROP_S_CANCEL;
    case Win32DragAction::Drop: return DRAGDROP_S_DROP;
    case Win32DragAction::Continue:
    default: return S_OK;
  }
}

HRESULT STDMETHODCALLTYPE Win32DropSource::GiveFeedback(const DWORD) {
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

unsigned long win32DropEffect(const DragEffect effect) {
  switch (effect) {
    case DragEffect::Copy: return DROPEFFECT_COPY;
    case DragEffect::Move: return DROPEFFECT_MOVE;
    case DragEffect::Link: return DROPEFFECT_LINK;
    case DragEffect::None:
    default: return DROPEFFECT_NONE;
  }
}

DragEffect dragEffectFromWin32(const unsigned long effect) {
  if ((effect & DROPEFFECT_MOVE) != 0) return DragEffect::Move;
  if ((effect & DROPEFFECT_COPY) != 0) return DragEffect::Copy;
  if ((effect & DROPEFFECT_LINK) != 0) return DragEffect::Link;
  return DragEffect::None;
}

DragEffect runWin32Drag(const DragPayload& payload, const DragEffect allowedEffects) {
  auto* data = Win32DragDataObject::create(payload);
  if (data == nullptr) return DragEffect::None;
  auto* source = new Win32DropSource();
  if (source == nullptr) {
    data->Release();
    return DragEffect::None;
  }
  DWORD effect = 0;
  const HRESULT result = ::DoDragDrop(data, source, win32DropEffect(allowedEffects), &effect);
  source->Release();
  data->Release();
  if (FAILED(result)) return DragEffect::None;
  const DragEffect dropped = dragEffectFromWin32(effect);
  return (win32DropEffect(dropped) & win32DropEffect(allowedEffects)) != 0 ? dropped : DragEffect::None;
}
} // namespace coral::ui

#endif // _WIN32
