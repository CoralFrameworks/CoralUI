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

#include "../../FileDialogPlatform.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>
#include <shobjidl.h>

#include <string>
#include <vector>

namespace coral::ui::detail {

namespace {

std::wstring toWide(const std::string& text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
    nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

std::string toUtf8(const wchar_t* text) {
  if (text == nullptr) return {};
  const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (length <= 1) return {};
  std::string result(static_cast<size_t>(length - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
  return result;
}

std::string itemPath(IShellItem* item) {
  if (item == nullptr) return {};
  PWSTR path = nullptr;
  if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path == nullptr) {
    return {};
  }
  std::string result = toUtf8(path);
  CoTaskMemFree(path);
  return result;
}

struct FilterStorage {
  std::vector<std::wstring> labels;
  std::vector<std::wstring> patterns;
  std::vector<COMDLG_FILTERSPEC> specs;
};

FilterStorage makeFilters(const std::vector<FileDialogFilter>& filters) {
  FilterStorage storage;
  for (const FileDialogFilter& filter: filters) {
    std::wstring pattern;
    for (const std::string& extension: filter.extensions) {
      if (!pattern.empty()) pattern += L";";
      pattern += L"*." + toWide(extension);
    }
    if (pattern.empty()) pattern = L"*.*";
    storage.labels.push_back(toWide(filter.label));
    storage.patterns.push_back(std::move(pattern));
  }
  storage.specs.reserve(storage.labels.size());
  for (size_t index = 0; index < storage.labels.size(); ++index) {
    storage.specs.push_back(COMDLG_FILTERSPEC{ storage.labels[index].c_str(),
      storage.patterns[index].c_str() });
  }
  return storage;
}

void configureDialog(IFileDialog* dialog, const FileDialogOptions& options,
    const bool save, FilterStorage& filters) {
  DWORD flags = 0;
  dialog->GetOptions(&flags);
  flags |= FOS_FORCEFILESYSTEM;
  if (!save && options.allowMultiple) flags |= FOS_ALLOWMULTISELECT;
  if (save) flags |= FOS_OVERWRITEPROMPT;
  dialog->SetOptions(flags);
  if (!options.title.empty()) {
    dialog->SetTitle(toWide(options.title).c_str());
  }
  if (!filters.specs.empty()) {
    dialog->SetFileTypes(static_cast<UINT>(filters.specs.size()), filters.specs.data());
  }
  if (!options.defaultPath.empty()) {
    IShellItem* folder = nullptr;
    if (SUCCEEDED(SHCreateItemFromParsingName(toWide(options.defaultPath).c_str(), nullptr,
          IID_PPV_ARGS(&folder))) && folder != nullptr) {
      dialog->SetFolder(folder);
      folder->Release();
    }
  }
}

} // namespace

FileDialogResult showPlatformFileDialog(const FileDialogKind kind,
    const FileDialogOptions& options) {
  const bool save = kind == FileDialogKind::SaveFile;
  const HRESULT initialized = CoInitializeEx(nullptr,
    COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  const bool uninitialize = SUCCEEDED(initialized);
  if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
    return {};
  }

  FileDialogResult result;
  IFileDialog* dialog = nullptr;
  const CLSID clsid = save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
  if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))) ||
      dialog == nullptr) {
    if (uninitialize) CoUninitialize();
    return result;
  }
  FilterStorage filters = makeFilters(options.filters);
  configureDialog(dialog, options, save, filters);
  const HRESULT shown = dialog->Show(nullptr);
  if (SUCCEEDED(shown)) {
    if (save) {
      IShellItem* item = nullptr;
      if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
        result.paths.push_back(itemPath(item));
        item->Release();
      }
    } else {
      IFileOpenDialog* openDialog = nullptr;
      if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&openDialog))) && openDialog != nullptr) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(openDialog->GetResults(&items)) && items != nullptr) {
          DWORD count = 0;
          items->GetCount(&count);
          for (DWORD index = 0; index < count; ++index) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(items->GetItemAt(index, &item)) && item != nullptr) {
              result.paths.push_back(itemPath(item));
              item->Release();
            }
          }
          items->Release();
        }
        openDialog->Release();
      }
    }
  }
  result.accepted = !result.paths.empty();
  dialog->Release();
  if (uninitialize) CoUninitialize();
  return result;
}

} // namespace coral::ui::detail
