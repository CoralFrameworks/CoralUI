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

#ifndef CORAL_UI_WIN32DRAGOUT_H
#define CORAL_UI_WIN32DRAGOUT_H

#include <ui/DragDrop.h>

#if defined(_WIN32)

// OLE headers pull <windows.h>; keep its min/max macros out of the UI sources that include this.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <ole2.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace coral::ui {

/**
 * One clipboard format a Win32 drag offers, already serialised the way `SetClipboardData` /
 * `IDataObject::GetData` expects.
 *
 * `format` is a CF_* constant or a registered format id; `data` is the exact HGLOBAL payload (a
 * NUL-terminated wide string for CF_UNICODETEXT, a DROPFILES block for CF_HDROP, the raw bytes for a
 * registered format).
 */
struct Win32DragFormat {
  unsigned int format = 0;
  std::string data;
  /** True when `data` is UTF-16 code units (CF_UNICODETEXT and CF_HDROP). */
  bool wide = false;
};

/**
 * Converts a drag payload into the formats an OLE drag source should advertise.
 *
 * This is the pure part of drag-out: the OLE object only has to answer `GetData` with these bytes, so
 * the mapping is unit-tested without running a real drag loop.
 */
[[nodiscard]] std::vector<Win32DragFormat> win32DragFormats(const DragPayload& payload);

/** Maps a Coral drag effect to a Win32 DROPEFFECT bit and back (the drag loop needs both). */
/** What the drag loop should do next (extracted so the keyboard/mouse rule is testable). */
enum class Win32DragAction { Continue, Drop, Cancel };
[[nodiscard]] Win32DragAction win32DragAction(bool escapePressed, bool primaryDown,
  bool secondaryDown);

/** The OLE drag source: delegates the continue/drop/cancel decision to `win32DragAction`. */
class Win32DropSource final : public IDropSource {
 public:
  Win32DropSource() = default;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override;
  HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD effect) override;
 private:
  std::atomic<ULONG> mReferenceCount{ 1 };
};

[[nodiscard]] unsigned long win32DropEffect(DragEffect effect);
[[nodiscard]] DragEffect dragEffectFromWin32(unsigned long effect);

/**
 * Runs one OLE drag with the payload; call from a mouse-down gesture after OleInitialize.
 *
 * DoDragDrop owns the mouse and blocks the calling thread until the drop, Escape or a lost
 * capture, so this is deliberately not called from tests: the tested parts are the formats,
 * the data object answers and the continue/drop/cancel rule.
 */
[[nodiscard]] DragEffect runWin32Drag(const DragPayload& payload, DragEffect allowedEffects);

/**
 * The IDataObject half of a Win32 drag source.
 *
 * It answers GetData/QueryGetData from the serialised formats above; the drag loop (IDropSource and
 * DoDragDrop) is a separate step. Reference-counted through IUnknown, as OLE requires.
 */
class Win32DragDataObject final : public IDataObject {
 public:
  static Win32DragDataObject* create(const DragPayload& payload);
  Win32DragDataObject(const Win32DragDataObject&) = delete;
  Win32DragDataObject& operator=(const Win32DragDataObject&) = delete;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) override;
  HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* format, STGMEDIUM* medium) override;
  HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) override;
  HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* format, FORMATETC* out) override;
  HRESULT STDMETHODCALLTYPE SetData(FORMATETC* format, STGMEDIUM* medium, BOOL release) override;
  HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** enumerator) override;
  HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override;
  HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override;
  HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override;

 private:
  explicit Win32DragDataObject(std::vector<Win32DragFormat> formats);
  ~Win32DragDataObject() = default;
  [[nodiscard]] const Win32DragFormat* find(const FORMATETC& format) const;

  std::atomic<ULONG> mReferenceCount{ 1 };
  std::vector<Win32DragFormat> mFormats;
};

} // namespace coral::ui

#endif // _WIN32

#endif // CORAL_UI_WIN32DRAGOUT_H
