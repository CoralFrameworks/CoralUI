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

#include "UiaProvider.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
// The property and control-type ids providers report live in the client header; only the constants are
// used here, so the provider does not depend on the client library.
#include <uiautomationclient.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui::detail {

namespace {

/** The layers flattened once into an index-addressed tree, so providers stay valid across calls. */
struct FlatSnapshot {
  struct Node {
    std::string name;
    std::string value;
    std::string path;
    AccessibilityRole role = AccessibilityRole::Generic;
    AccessibilityAction actions = AccessibilityAction::None;
    Rect bounds{};
    bool focused = false;
    bool focusable = false;
    bool selected = false;
    bool editable = false;
    bool enabled = true;
    std::optional<AccessibleRange> range;
    std::optional<AccessibleScroll> scroll;
    std::optional<AccessibleGrid> grid;
    std::optional<AccessibleTextSelection> textSelection;
    AccessibilityPattern patterns = AccessibilityPattern::None;
    int parent = -1;
    std::vector<int> children;
  };
  std::vector<Node> nodes;
  int focused = -1;
  AccessibilityActionInvoker invoker;
};

void flattenInto(FlatSnapshot& flat, const AccessibilityNode& node, const int parent) {
  const int index = static_cast<int>(flat.nodes.size());
  flat.nodes.push_back({});
  auto& entry = flat.nodes.back();
  entry.name = node.name;
  entry.value = node.value;
  entry.path = node.path;
  entry.role = node.role;
  entry.actions = node.actions;
  entry.bounds = node.bounds;
  entry.focused = node.focused;
  entry.focusable = node.focusable;
  entry.selected = node.selected;
  entry.editable = node.editable;
  entry.enabled = node.enabled;
  entry.range = node.range;
  entry.scroll = node.scroll;
  entry.grid = node.grid;
  entry.textSelection = node.textSelection;
  entry.patterns = accessibilityPatterns(node);
  entry.parent = parent;
  if (node.focused) flat.focused = index;
  if (parent >= 0) flat.nodes[parent].children.push_back(index);
  for (const AccessibilityNode& child : node.children) {
    flattenInto(flat, child, index);
  }
}

std::shared_ptr<FlatSnapshot> flatten(const AccessibilityNode& root) {
  auto flat = std::make_shared<FlatSnapshot>();
  flattenInto(*flat, root, -1);
  return flat;
}

// Four property ids live in the client header rather than the provider header; their values are fixed
// by the UI Automation specification.
constexpr PROPERTYID kIsContentElementPropertyId = 30017;
constexpr PROPERTYID kIsControlElementPropertyId = 30016;
constexpr PROPERTYID kValueValuePropertyId = 30045;
constexpr PROPERTYID kBoundingRectanglePropertyId = 30001;
// Pattern availability: UIA clients ask these before they hand out a pattern object.
constexpr PROPERTYID kIsInvokePatternAvailablePropertyId = 30031;
constexpr PROPERTYID kIsTogglePatternAvailablePropertyId = 30041;
constexpr PROPERTYID kIsValuePatternAvailablePropertyId = 30043;
constexpr PROPERTYID kIsSelectionItemPatternAvailablePropertyId = 30036;
constexpr PROPERTYID kIsSelectionPatternAvailablePropertyId = 30037;
constexpr PROPERTYID kIsGridItemPatternAvailablePropertyId = 30029;
constexpr PROPERTYID kIsGridPatternAvailablePropertyId = 30030;
constexpr PROPERTYID kIsRangeValuePatternAvailablePropertyId = 30033;
constexpr PROPERTYID kIsTextPatternAvailablePropertyId = 30040;
constexpr PROPERTYID kIsScrollPatternAvailablePropertyId = 30034;CONTROLTYPEID controlTypeFor(const AccessibilityRole role) {
  switch (role) {
    case AccessibilityRole::Button: return UIA_ButtonControlTypeId;
    case AccessibilityRole::CheckBox: return UIA_CheckBoxControlTypeId;
    case AccessibilityRole::Radio: return UIA_RadioButtonControlTypeId;
    case AccessibilityRole::Slider: return UIA_SliderControlTypeId;
    case AccessibilityRole::TextField: return UIA_EditControlTypeId;
    case AccessibilityRole::Label: return UIA_TextControlTypeId;
    case AccessibilityRole::List: return UIA_ListControlTypeId;
    case AccessibilityRole::Tree: return UIA_TreeControlTypeId;
    case AccessibilityRole::Menu: return UIA_MenuControlTypeId;
    case AccessibilityRole::Table: return UIA_TableControlTypeId;
    case AccessibilityRole::ListItem: return UIA_ListItemControlTypeId;
    case AccessibilityRole::ScrollView: return UIA_PaneControlTypeId;
    case AccessibilityRole::Dialog: return UIA_PaneControlTypeId;
    case AccessibilityRole::Group: return UIA_GroupControlTypeId;
    default: return UIA_CustomControlTypeId;
  }
}

/** ISelectionProvider for a list/tree/table; defined after UiaElement because it returns elements. */
IUnknown* createSelectionProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index,
  HWND hwnd);
/** An element provider for one snapshot index; defined after UiaElement for the same reason. */
IUnknown* createElementProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index,
  HWND hwnd);
/** An IGridItemProvider for a cell; defined after UiaElement. */
IUnknown* createGridItemProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index,
  HWND hwnd);

/**
 * The Grid pattern: row/column counts plus `GetItem`.
 *
 * The snapshot is a flat tree, so `GetItem` looks the cell up by the row/column the tree builder
 * annotated; a grid whose cells do not carry that annotation simply reports no item.
 */
/** Client-space bounds to screen space; defined after the element class. */
void screenBounds(HWND hwnd, const Rect& bounds, RECT* out);

/** Widens the node's UTF-8 text once; UIA addresses it in UTF-16 code units. */
std::wstring wideText(const std::string& text) {
  if (text.empty()) return {};
  const int length = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
    static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring wide(static_cast<std::size_t>(length), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), length);
  return wide;
}

/**
 * A text range over one node's text: a pair of UTF-16 offsets into the snapshot's value.
 *
 * The control is reached only through `Select` and `SetText`, which go through the invoker like every
 * other action; the bridge never touches the control's buffers.
 */
class UiaTextRange final : public ITextRangeProvider {
 public:
  UiaTextRange(std::shared_ptr<const FlatSnapshot> snapshot, const int index, const HWND hwnd,
      const int start, const int end)
      : mSnapshot(std::move(snapshot)), mIndex(index), mHwnd(hwnd), mStart(start), mEnd(end) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(ITextRangeProvider)) {
      *out = static_cast<ITextRangeProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE Clone(ITextRangeProvider** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = new UiaTextRange(mSnapshot, mIndex, mHwnd, mStart, mEnd);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Compare(ITextRangeProvider* other, BOOL* out) override {
    if (other == nullptr || out == nullptr) return E_INVALIDARG;
    const auto* peer = dynamic_cast<const UiaTextRange*>(other);
    if (peer == nullptr || peer->mIndex != mIndex) return E_INVALIDARG;
    *out = peer->mStart == mStart && peer->mEnd == mEnd ? TRUE : FALSE;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE CompareEndpoints(TextPatternRangeEndpoint endpoint,
      ITextRangeProvider* other, TextPatternRangeEndpoint targetEndpoint, int* out) override {
    if (other == nullptr || out == nullptr) return E_INVALIDARG;
    const auto* peer = dynamic_cast<const UiaTextRange*>(other);
    if (peer == nullptr || peer->mIndex != mIndex) return E_INVALIDARG;
    const int mine = endpoint == TextPatternRangeEndpoint_Start ? mStart : mEnd;
    const int theirs = targetEndpoint == TextPatternRangeEndpoint_Start ? peer->mStart : peer->mEnd;
    *out = mine < theirs ? -1 : (mine > theirs ? 1 : 0);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(TextUnit unit) override {
    (void)unit;
    // There is no word/line index in the snapshot yet, so the document range is the enclosing unit.
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID attributeId, VARIANT value,
      BOOL backward, ITextRangeProvider** out) override {
    (void)attributeId;
    (void)value;
    (void)backward;
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    return S_FALSE;
  }
  HRESULT STDMETHODCALLTYPE FindText(BSTR text, BOOL backward, BOOL ignoreCase,
      ITextRangeProvider** out) override {
    (void)text;
    (void)backward;
    (void)ignoreCase;
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    return S_FALSE;
  }
  HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID attributeId, VARIANT* out) override {
    (void)attributeId;
    if (out == nullptr) return E_INVALIDARG;
    VariantInit(out);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetBoundingRectangles(SAFEARRAY** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    const Rect bounds = mSnapshot->nodes[mIndex].bounds;
    if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f) return S_OK;
    RECT screen{};
    screenBounds(mHwnd, bounds, &screen);
    double values[4] = { static_cast<double>(screen.left), static_cast<double>(screen.top),
      static_cast<double>(screen.right), static_cast<double>(screen.bottom) };
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_R8, 0, 4);
    if (array == nullptr) return E_OUTOFMEMORY;
    void* data = nullptr;
    ::SafeArrayAccessData(array, &data);
    std::memcpy(data, values, sizeof(values));
    ::SafeArrayUnaccessData(array);
    *out = array;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetEnclosingElement(IRawElementProviderSimple** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = static_cast<IRawElementProviderSimple*>(createElementProvider(mSnapshot, mIndex, mHwnd));
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetText(int maxLength, BSTR* out) override {
    if (out == nullptr) return E_INVALIDARG;
    const std::wstring text = wideText(mSnapshot->nodes[mIndex].value);
    const int length = static_cast<int>(text.size());
    const int start = std::clamp(mStart, 0, length);
    int end = std::clamp(mEnd, start, length);
    if (maxLength >= 0) end = std::min(end, start + maxLength);
    *out = ::SysAllocStringLen(text.data() + start, static_cast<UINT>(end - start));
    return *out != nullptr ? S_OK : E_OUTOFMEMORY;
  }
  HRESULT STDMETHODCALLTYPE Move(TextUnit unit, int count, int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    const int length = static_cast<int>(wideText(mSnapshot->nodes[mIndex].value).size());
    // Without a word/line index only character movement is exact; larger units collapse to the document
    // edges, which is what a reader asking to move by "page" in a single-line field expects.
    int next = mStart;
    int moved = 0;
    if (unit == TextUnit_Character) {
      next = std::clamp(mStart + count, 0, length);
      moved = std::abs(next - mStart);
    } else if (count != 0) {
      next = count < 0 ? 0 : length;
      moved = 1;
    }
    *out = moved;
    mStart = next;
    mEnd = next;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE MoveEndpointByRange(TextPatternRangeEndpoint endpoint,
      ITextRangeProvider* other, TextPatternRangeEndpoint targetEndpoint) override {
    (void)endpoint;
    (void)other;
    (void)targetEndpoint;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(TextPatternRangeEndpoint endpoint, TextUnit unit,
      int count, int* out) override {
    (void)endpoint;
    (void)unit;
    (void)count;
    if (out == nullptr) return E_INVALIDARG;
    *out = 0;
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE AddToSelection() override {
    // The control is single-selection, so adding the range is the same as selecting it.
    return Select();
  }
  HRESULT STDMETHODCALLTYPE RemoveFromSelection() override { return S_OK; }
  HRESULT STDMETHODCALLTYPE GetChildren(SAFEARRAY** out) override {
    // Text runs have no element children of their own; the edit's children come from the tree.
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL alignToTop) override {
    (void)alignToTop;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Select() override {
    if (!mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    const std::string command = std::to_string(mStart) + "," + std::to_string(mEnd);
    const std::string path = mSnapshot->nodes[mIndex].path;
    return mSnapshot->invoker(path, AccessibilityAction::Select, command) ? S_OK : E_FAIL;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
  HWND mHwnd = nullptr;
  int mStart = 0;
  int mEnd = 0;
};

/** The Text pattern: the document range plus the current selection of an editable control. */
class UiaText final : public ITextProvider {
 public:
  UiaText(std::shared_ptr<const FlatSnapshot> snapshot, const int index, const HWND hwnd)
      : mSnapshot(std::move(snapshot)), mIndex(index), mHwnd(hwnd) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(ITextProvider)) {
      *out = static_cast<ITextProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE GetSelection(SAFEARRAY** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    const int length = static_cast<int>(wideText(mSnapshot->nodes[mIndex].value).size());
    const AccessibleTextSelection selection =
      mSnapshot->nodes[mIndex].textSelection.value_or(AccessibleTextSelection{ 0, 0 });
    ITextRangeProvider* range = new UiaTextRange(mSnapshot, mIndex, mHwnd,
      std::clamp(selection.start, 0, length), std::clamp(selection.end, 0, length));
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
    if (array == nullptr) {
      range->Release();
      return E_OUTOFMEMORY;
    }
    LONG index = 0;
    ::SafeArrayPutElement(array, &index, range);
    range->Release();
    *out = array;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetVisibleRanges(SAFEARRAY** out) override {
    return documentRangeArray(out);
  }
  HRESULT STDMETHODCALLTYPE RangeFromChild(IRawElementProviderSimple* child,
      ITextRangeProvider** out) override {
    (void)child;
    // An edit has no inline child elements, so there is no range to hand back.
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    return E_INVALIDARG;
  }
  HRESULT STDMETHODCALLTYPE RangeFromPoint(UiaPoint point, ITextRangeProvider** out) override {
    (void)point;
    // Without a per-glyph index the best honest answer is the whole document.
    if (out == nullptr) return E_INVALIDARG;
    *out = documentRange();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_DocumentRange(ITextRangeProvider** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = documentRange();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_SupportedTextSelection(SupportedTextSelection* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = SupportedTextSelection_Single;
    return S_OK;
  }

 private:
  ITextRangeProvider* documentRange() const {
    const int length = static_cast<int>(wideText(mSnapshot->nodes[mIndex].value).size());
    return new UiaTextRange(mSnapshot, mIndex, mHwnd, 0, length);
  }
  HRESULT documentRangeArray(SAFEARRAY** out) {
    *out = nullptr;
    ITextRangeProvider* range = documentRange();
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_UNKNOWN, 0, 1);
    if (array == nullptr) {
      range->Release();
      return E_OUTOFMEMORY;
    }
    LONG index = 0;
    ::SafeArrayPutElement(array, &index, range);
    range->Release();
    *out = array;
    return S_OK;
  }
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
  HWND mHwnd = nullptr;
};

class UiaGrid final : public IGridProvider {
 public:
  UiaGrid(std::shared_ptr<const FlatSnapshot> snapshot, const int index, HWND hwnd)
      : mSnapshot(std::move(snapshot)), mIndex(index), mHwnd(hwnd) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IGridProvider)) {
      *out = static_cast<IGridProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE GetItem(int row, int column, IRawElementProviderSimple** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    const AccessibleGrid grid = mSnapshot->nodes[mIndex].grid.value_or(AccessibleGrid{});
    if (row < 0 || column < 0 || row >= grid.rows || column >= grid.columns) return E_INVALIDARG;
    for (const int child: mSnapshot->nodes[mIndex].children) {
      const AccessibleGrid cell = mSnapshot->nodes[child].grid.value_or(AccessibleGrid{});
      if (cell.row != row || cell.column != column) continue;
      *out = static_cast<IRawElementProviderSimple*>(createElementProvider(mSnapshot, child, mHwnd));
      return S_OK;
    }
    return UIA_E_ELEMENTNOTAVAILABLE;
  }
  HRESULT STDMETHODCALLTYPE get_RowCount(int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].grid.value_or(AccessibleGrid{}).rows;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_ColumnCount(int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].grid.value_or(AccessibleGrid{}).columns;
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
  HWND mHwnd = nullptr;
};

/** The GridItem pattern: which cell a node is, and the grid that owns it. */
class UiaGridItem final : public IGridItemProvider {
 public:
  UiaGridItem(std::shared_ptr<const FlatSnapshot> snapshot, const int index, HWND hwnd)
      : mSnapshot(std::move(snapshot)), mIndex(index), mHwnd(hwnd) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IGridItemProvider)) {
      *out = static_cast<IGridItemProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE get_Row(int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].grid.value_or(AccessibleGrid{}).row;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_Column(int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].grid.value_or(AccessibleGrid{}).column;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_RowSpan(int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = 1;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_ColumnSpan(int* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = 1;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_ContainingGrid(IRawElementProviderSimple** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    const int parent = mSnapshot->nodes[mIndex].parent;
    if (parent < 0) return UIA_E_ELEMENTNOTAVAILABLE;
    *out = static_cast<IRawElementProviderSimple*>(createElementProvider(mSnapshot, parent, mHwnd));
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
  HWND mHwnd = nullptr;
};

/** ISelectionItemProvider for a row whose owner keeps the selection. */
class UiaSelectionItem final : public ISelectionItemProvider {
 public:
  UiaSelectionItem(std::shared_ptr<const FlatSnapshot> snapshot, const int index)
      : mSnapshot(std::move(snapshot)), mIndex(index) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(ISelectionItemProvider)) {
      *out = static_cast<ISelectionItemProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE Select() override {
    if (!mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    return mSnapshot->invoker(mSnapshot->nodes[mIndex].path, AccessibilityAction::Select,
      std::string_view{}) ? S_OK : E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE AddToSelection() override { return Select(); }
  HRESULT STDMETHODCALLTYPE RemoveFromSelection() override { return UIA_E_NOTSUPPORTED; }
  HRESULT STDMETHODCALLTYPE get_IsSelected(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].selected ? TRUE : FALSE;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_SelectionContainer(IRawElementProviderSimple** out) override {
    if (out == nullptr) return E_INVALIDARG;
    // The owner is a plain list; clients that need it can walk up with Navigate(Parent).
    *out = nullptr;
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
};

/** Unicode conversion for the Value pattern, sized for the terminator Windows writes. */
bool toBstr(const std::string& text, BSTR* out) {
  const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
    static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
  if (wideLength > 0) {
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
      wide.data(), wideLength);
  }
  *out = ::SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
  return *out != nullptr;
}

/** The Toggle pattern: state is read from the snapshot, the toggle is queued on the UI thread. */
class UiaToggle final : public IToggleProvider {
 public:
  UiaToggle(std::shared_ptr<const FlatSnapshot> snapshot, const int index)
      : mSnapshot(std::move(snapshot)), mIndex(index) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IToggleProvider)) {
      *out = static_cast<IToggleProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE get_ToggleState(ToggleState* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].selected ? ToggleState_On : ToggleState_Off;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Toggle() override {
    if (!mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    const std::string path = mSnapshot->nodes[mIndex].path;
    return mSnapshot->invoker(path, AccessibilityAction::Toggle, std::string_view{}) ? S_OK : E_FAIL;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
};

/** The Value pattern: a text field value can be read and replaced. */
class UiaValue final : public IValueProvider {
 public:
  UiaValue(std::shared_ptr<const FlatSnapshot> snapshot, const int index)
      : mSnapshot(std::move(snapshot)), mIndex(index) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IValueProvider)) {
      *out = static_cast<IValueProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) override {
    if (!mSnapshot->invoker || value == nullptr) return E_INVALIDARG;
    const int length = static_cast<int>(::WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0,
      nullptr, nullptr));
    std::string text(static_cast<std::size_t>(length > 0 ? length : 0), '\0');
    if (length > 1) {
      ::WideCharToMultiByte(CP_UTF8, 0, value, -1, text.data(), length, nullptr, nullptr);
      text.resize(static_cast<std::size_t>(length - 1));
    }
    const std::string path = mSnapshot->nodes[mIndex].path;
    return mSnapshot->invoker(path, AccessibilityAction::SetValue, text) ? S_OK : E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE get_Value(BSTR* out) override {
    if (out == nullptr) return E_INVALIDARG;
    return toBstr(mSnapshot->nodes[mIndex].value, out) ? S_OK : E_OUTOFMEMORY;
  }
  HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].editable ? FALSE : TRUE;
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
};

/** The RangeValue pattern: a slider or progress bar with a numeric value and bounds. */
class UiaRangeValue final : public IRangeValueProvider {
 public:
  UiaRangeValue(std::shared_ptr<const FlatSnapshot> snapshot, const int index)
      : mSnapshot(std::move(snapshot)), mIndex(index) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IRangeValueProvider)) {
      *out = static_cast<IRangeValueProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE SetValue(double value) override {
    const auto& range = mSnapshot->nodes[mIndex].range;
    if (!range.has_value() || !mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    if (value < range->minimum || value > range->maximum) return E_INVALIDARG;
    // The control parses its own textual value; the snapshot's string value is the canonical spelling,
    // so the same format round-trips.
    const std::string text = std::to_string(value);
    const std::string path = mSnapshot->nodes[mIndex].path;
    return mSnapshot->invoker(path, AccessibilityAction::SetValue, text) ? S_OK : E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE get_Value(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].range.value_or(AccessibleRange{}).value;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_Maximum(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].range.value_or(AccessibleRange{}).maximum;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_Minimum(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].range.value_or(AccessibleRange{}).minimum;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_LargeChange(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    const AccessibleRange range = mSnapshot->nodes[mIndex].range.value_or(AccessibleRange{});
    *out = range.step > 0.0 ? range.step * 5.0 : (range.maximum - range.minimum) / 10.0;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_SmallChange(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    const AccessibleRange range = mSnapshot->nodes[mIndex].range.value_or(AccessibleRange{});
    *out = range.step > 0.0 ? range.step : (range.maximum - range.minimum) / 100.0;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].enabled ? FALSE : TRUE;
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
};

/**
 * The Scroll pattern: percentages and relative movements, both expressed through the invoker string
 * contract documented on `ScrollView::performAccessibilityAction`.
 */
class UiaScroll final : public IScrollProvider {
 public:
  UiaScroll(std::shared_ptr<const FlatSnapshot> snapshot, const int index)
      : mSnapshot(std::move(snapshot)), mIndex(index) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IScrollProvider)) {
      *out = static_cast<IScrollProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE Scroll(ScrollAmount horizontal, ScrollAmount vertical) override {
    const auto& scroll = mSnapshot->nodes[mIndex].scroll;
    if (!scroll.has_value() || !mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    const auto delta = [](const ScrollAmount amount, const char* up, const char* down) {
      switch (amount) {
        case ScrollAmount_SmallIncrement: return std::string("line") + up;
        case ScrollAmount_LargeIncrement: return std::string("page") + up;
        case ScrollAmount_SmallDecrement: return std::string("line") + down;
        case ScrollAmount_LargeDecrement: return std::string("page") + down;
        default: return std::string();
      }
    };
    const std::string horizontalCommand = delta(horizontal, "Right", "Left");
    const std::string verticalCommand = delta(vertical, "Down", "Up");
    const std::string path = mSnapshot->nodes[mIndex].path;
    bool any = false;
    if (!horizontalCommand.empty()) {
      any = mSnapshot->invoker(path, AccessibilityAction::Scroll, horizontalCommand) || any;
    }
    if (!verticalCommand.empty()) {
      any = mSnapshot->invoker(path, AccessibilityAction::Scroll, verticalCommand) || any;
    }
    return any ? S_OK : UIA_E_NOTSUPPORTED;
  }
  HRESULT STDMETHODCALLTYPE SetScrollPercent(double horizontal, double vertical) override {
    const auto& scroll = mSnapshot->nodes[mIndex].scroll;
    if (!scroll.has_value() || !mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    const auto clamp = [](const double value) {
      return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
    };
    const std::string command = "set:" + std::to_string(clamp(horizontal)) + "," +
      std::to_string(clamp(vertical));
    const std::string path = mSnapshot->nodes[mIndex].path;
    return mSnapshot->invoker(path, AccessibilityAction::Scroll, command) ? S_OK : E_FAIL;
  }
  HRESULT STDMETHODCALLTYPE get_HorizontalScrollPercent(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].scroll.value_or(AccessibleScroll{}).horizontalPercent;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_VerticalScrollPercent(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].scroll.value_or(AccessibleScroll{}).verticalPercent;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_HorizontalViewSize(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].scroll.value_or(AccessibleScroll{}).horizontalViewSize;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_VerticalViewSize(double* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].scroll.value_or(AccessibleScroll{}).verticalViewSize;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_HorizontallyScrollable(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].scroll.value_or(AccessibleScroll{}).canScrollHorizontally
      ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_VerticallyScrollable(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = mSnapshot->nodes[mIndex].scroll.value_or(AccessibleScroll{}).canScrollVertically
      ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
};

/** The Invoke pattern: a button click is queued on the UI thread by the installed invoker. */
class UiaInvoke final : public IInvokeProvider {
 public:
  UiaInvoke(std::shared_ptr<const FlatSnapshot> snapshot, const int index)
      : mSnapshot(std::move(snapshot)), mIndex(index) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IInvokeProvider)) {
      *out = static_cast<IInvokeProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE Invoke() override {
    if (!mSnapshot->invoker) return UIA_E_NOTSUPPORTED;
    const std::string path = mSnapshot->nodes[mIndex].path;
    return mSnapshot->invoker(path, AccessibilityAction::Invoke, std::string_view{}) ? S_OK : E_FAIL;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
};

/** Screen-space rectangle UIA expects, computed from the client-space layer bounds. */
void screenBounds(const HWND hwnd, const Rect& bounds, RECT* out) {
  POINT origin{ static_cast<LONG>(bounds.getLeft()), static_cast<LONG>(bounds.getTop()) };
  POINT corner{ static_cast<LONG>(bounds.getRight()), static_cast<LONG>(bounds.getBottom()) };
  if (hwnd != nullptr) {
    ::ClientToScreen(hwnd, &origin);
    ::ClientToScreen(hwnd, &corner);
  }
  out->left = origin.x;
  out->top = origin.y;
  out->right = corner.x;
  out->bottom = corner.y;
}

class UiaElement final : public IRawElementProviderSimple,
                         public IRawElementProviderFragment,
                         public IRawElementProviderFragmentRoot {
 public:
  UiaElement(std::shared_ptr<const FlatSnapshot> snapshot, const int index, HWND hwnd)
      : mSnapshot(std::move(snapshot)), mIndex(index), mHwnd(hwnd) {}

  // ---- IUnknown ---------------------------------------------------------
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IRawElementProviderSimple)) {
      *out = static_cast<IRawElementProviderSimple*>(this);
    } else if (riid == __uuidof(IRawElementProviderFragment)) {
      *out = static_cast<IRawElementProviderFragment*>(this);
    } else if (riid == __uuidof(IRawElementProviderFragmentRoot) && isRoot()) {
      *out = static_cast<IRawElementProviderFragmentRoot*>(this);
    } else {
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }

  // ---- IRawElementProviderSimple ---------------------------------------
  HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions* out) override {
    if (out == nullptr) return E_INVALIDARG;
    // Com threading: UIA may call the provider from its own thread, and the snapshot it reads is
    // immutable, so no marshalling is needed.
    *out = static_cast<ProviderOptions>(ProviderOptions_UseComThreading | ProviderOptions_ServerSideProvider);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, IUnknown** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    // The pattern set comes from the UI library (`accessibilityPatterns`), so a Windows client and any
    // future bridge agree on what a control offers.
    const AccessibilityPattern patterns = currentNode().patterns;
    if (patternId == UIA_InvokePatternId && hasAccessibilityPattern(patterns, AccessibilityPattern::Invoke)) {
      *out = static_cast<IUnknown*>(new UiaInvoke(mSnapshot, mIndex));
    } else if (patternId == UIA_TogglePatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::Toggle)) {
      *out = static_cast<IUnknown*>(new UiaToggle(mSnapshot, mIndex));
    } else if (patternId == UIA_ValuePatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::Value)) {
      *out = static_cast<IUnknown*>(new UiaValue(mSnapshot, mIndex));
    } else if (patternId == UIA_RangeValuePatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::RangeValue)) {
      *out = static_cast<IUnknown*>(new UiaRangeValue(mSnapshot, mIndex));
    } else if (patternId == UIA_ScrollPatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::Scroll)) {
      *out = static_cast<IUnknown*>(new UiaScroll(mSnapshot, mIndex));
    } else if (patternId == UIA_TextPatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::Text)) {
      *out = static_cast<IUnknown*>(new UiaText(mSnapshot, mIndex, mHwnd));
    } else if (patternId == UIA_GridPatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::Grid)) {
      *out = static_cast<IUnknown*>(new UiaGrid(mSnapshot, mIndex, mHwnd));
    } else if (patternId == UIA_GridItemPatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::GridItem)) {
      *out = createGridItemProvider(mSnapshot, mIndex, mHwnd);
    } else if (patternId == UIA_SelectionItemPatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::SelectionItem)) {
      *out = static_cast<IUnknown*>(new UiaSelectionItem(mSnapshot, mIndex));
    } else if (patternId == UIA_SelectionPatternId &&
        hasAccessibilityPattern(patterns, AccessibilityPattern::Selection)) {
      *out = createSelectionProvider(mSnapshot, mIndex, mHwnd);
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT* out) override {
    if (out == nullptr) return E_INVALIDARG;
    VariantInit(out);
    const FlatSnapshot::Node& node = currentNode();
    // `UIA_*PropertyId` are `static const` variables rather than macros in this SDK, so they cannot be
    // case labels; the comparisons are a chain for that reason.
    if (propertyId == UIA_NamePropertyId) return setString(out, node.name);
    if (propertyId == UIA_AutomationIdPropertyId) {
      return setString(out, node.name.empty() ? std::to_string(mIndex) : node.name);
    }
    if (propertyId == UIA_ControlTypePropertyId) {
      out->vt = VT_I4;
      out->lVal = controlTypeFor(node.role);
      return S_OK;
    }
    if (propertyId == UIA_IsEnabledPropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = node.enabled ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == UIA_IsKeyboardFocusablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = node.focusable ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == UIA_HasKeyboardFocusPropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = node.focused ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsSelectionPatternAvailablePropertyId) {
      const AccessibilityRole role = node.role;
      const bool container = role == AccessibilityRole::List || role == AccessibilityRole::Tree ||
        role == AccessibilityRole::Table;
      out->vt = VT_BOOL;
      out->boolVal = container ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsSelectionItemPatternAvailablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityPattern(node.patterns, AccessibilityPattern::SelectionItem)
        ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsTextPatternAvailablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityPattern(node.patterns, AccessibilityPattern::Text)
        ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsRangeValuePatternAvailablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityPattern(node.patterns, AccessibilityPattern::RangeValue)
        ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsScrollPatternAvailablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityPattern(node.patterns, AccessibilityPattern::Scroll)
        ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsGridPatternAvailablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityPattern(node.patterns, AccessibilityPattern::Grid)
        ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsGridItemPatternAvailablePropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityPattern(node.patterns, AccessibilityPattern::GridItem)
        ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsInvokePatternAvailablePropertyId ||
        propertyId == kIsTogglePatternAvailablePropertyId ||
        propertyId == kIsValuePatternAvailablePropertyId) {
      const AccessibilityAction wanted = propertyId == kIsInvokePatternAvailablePropertyId
        ? AccessibilityAction::Invoke
        : (propertyId == kIsTogglePatternAvailablePropertyId
          ? AccessibilityAction::Toggle : AccessibilityAction::SetValue);
      out->vt = VT_BOOL;
      out->boolVal = hasAccessibilityAction(node.actions, wanted) ? VARIANT_TRUE : VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == UIA_IsOffscreenPropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = VARIANT_FALSE;
      return S_OK;
    }
    if (propertyId == kIsContentElementPropertyId || propertyId == kIsControlElementPropertyId) {
      out->vt = VT_BOOL;
      out->boolVal = VARIANT_TRUE;
      return S_OK;
    }
    if (propertyId == kValueValuePropertyId) return setString(out, node.value);
    if (propertyId == kBoundingRectanglePropertyId) {
      RECT screen{};
      screenBounds(mHwnd, node.bounds, &screen);
      double values[4] = { static_cast<double>(screen.left), static_cast<double>(screen.top),
        static_cast<double>(screen.right), static_cast<double>(screen.bottom) };
      SAFEARRAY* array = ::SafeArrayCreateVector(VT_R8, 0, 4);
      if (array == nullptr) return E_OUTOFMEMORY;
      void* data = nullptr;
      ::SafeArrayAccessData(array, &data);
      std::memcpy(data, values, sizeof(values));
      ::SafeArrayUnaccessData(array);
      out->vt = VT_ARRAY | VT_R8;
      out->parray = array;
      return S_OK;
    }
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (!isRoot()) return S_OK;
    return ::UiaHostProviderFromHwnd(mHwnd, out);
  }

  // ---- IRawElementProviderFragment -------------------------------------
  HRESULT STDMETHODCALLTYPE Navigate(NavigateDirection direction,
    IRawElementProviderFragment** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    const FlatSnapshot::Node& node = currentNode();
    int target = -1;
    switch (direction) {
      case NavigateDirection_Parent:
        target = node.parent;
        break;
      case NavigateDirection_FirstChild:
        target = node.children.empty() ? -1 : node.children.front();
        break;
      case NavigateDirection_LastChild:
        target = node.children.empty() ? -1 : node.children.back();
        break;
      case NavigateDirection_NextSibling:
        target = sibling(1);
        break;
      case NavigateDirection_PreviousSibling:
        target = sibling(-1);
        break;
      default:
        break;
    }
    if (target < 0) return S_OK;
    *out = static_cast<IRawElementProviderFragment*>(new UiaElement(mSnapshot, target, mHwnd));
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY** out) override {
    if (out == nullptr) return E_INVALIDARG;
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_I4, 0, 3);
    if (array == nullptr) return E_OUTOFMEMORY;
    void* data = nullptr;
    ::SafeArrayAccessData(array, &data);
    int* values = static_cast<int*>(data);
    values[0] = UiaAppendRuntimeId;
    values[1] = static_cast<int>(reinterpret_cast<intptr_t>(mHwnd));
    values[2] = mIndex;
    ::SafeArrayUnaccessData(array);
    *out = array;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE get_BoundingRectangle(UiaRect* out) override {
    if (out == nullptr) return E_INVALIDARG;
    RECT screen{};
    screenBounds(mHwnd, currentNode().bounds, &screen);
    out->left = static_cast<double>(screen.left);
    out->top = static_cast<double>(screen.top);
    out->width = static_cast<double>(screen.right - screen.left);
    out->height = static_cast<double>(screen.bottom - screen.top);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(SAFEARRAY** out) override {
    if (out != nullptr) *out = nullptr;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetFocus() override { return S_OK; }

  HRESULT STDMETHODCALLTYPE get_FragmentRoot(IRawElementProviderFragmentRoot** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = new UiaElement(mSnapshot, 0, mHwnd);
    return S_OK;
  }

  // ---- IRawElementProviderFragmentRoot ---------------------------------
  HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(double x, double y,
    IRawElementProviderFragment** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    POINT point{ static_cast<LONG>(x), static_cast<LONG>(y) };
    if (mHwnd != nullptr) ::ScreenToClient(mHwnd, &point);
    int best = -1;
    for (std::size_t index = 0; index < mSnapshot->nodes.size(); ++index) {
      const Rect& bounds = mSnapshot->nodes[index].bounds;
      if (point.x >= bounds.getLeft() && point.x < bounds.getRight() &&
          point.y >= bounds.getTop() && point.y < bounds.getBottom()) {
        // Deeper nodes come later in the flattened order, so the last match is the innermost one.
        best = static_cast<int>(index);
      }
    }
    if (best < 0) best = 0;
    *out = static_cast<IRawElementProviderFragment*>(new UiaElement(mSnapshot, best, mHwnd));
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetFocus(IRawElementProviderFragment** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = static_cast<IRawElementProviderFragment*>(
      new UiaElement(mSnapshot, mSnapshot->focused >= 0 ? mSnapshot->focused : 0, mHwnd));
    return S_OK;
  }

 private:
  bool isRoot() const { return mIndex == 0; }
  const FlatSnapshot::Node& currentNode() const { return mSnapshot->nodes[mIndex]; }
  int sibling(const int delta) const {
    const int parent = currentNode().parent;
    if (parent < 0) return -1;
    const std::vector<int>& siblings = mSnapshot->nodes[parent].children;
    for (std::size_t index = 0; index < siblings.size(); ++index) {
      if (siblings[index] != mIndex) continue;
      const long next = static_cast<long>(index) + delta;
      if (next < 0 || next >= static_cast<long>(siblings.size())) return -1;
      return siblings[static_cast<std::size_t>(next)];
    }
    return -1;
  }

  static HRESULT setString(VARIANT* out, const std::string& text) {
    const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
      static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    if (wideLength > 0) {
      ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        wide.data(), wideLength);
    }
    out->vt = VT_BSTR;
    out->bstrVal = ::SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
    return out->bstrVal != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
  HWND mHwnd = nullptr;
};

/** The container side: which children are selected, for a list/tree/table. */
class UiaSelectionProvider final : public ISelectionProvider {
 public:
  UiaSelectionProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index, const HWND hwnd)
      : mSnapshot(std::move(snapshot)), mIndex(index), mHwnd(hwnd) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = nullptr;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(ISelectionProvider)) {
      *out = static_cast<ISelectionProvider*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++mRefCount; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG count = --mRefCount;
    if (count == 0) delete this;
    return count;
  }
  HRESULT STDMETHODCALLTYPE GetSelection(SAFEARRAY** out) override {
    if (out == nullptr) return E_INVALIDARG;
    std::vector<IRawElementProviderSimple*> selected;
    for (const int child : mSnapshot->nodes[mIndex].children) {
      if (!mSnapshot->nodes[child].selected) continue;
      selected.push_back(static_cast<IRawElementProviderSimple*>(
        new UiaElement(mSnapshot, child, mHwnd)));
    }
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_UNKNOWN, 0,
      static_cast<ULONG>(selected.size()));
    if (array == nullptr) {
      for (auto* element : selected) element->Release();
      return E_OUTOFMEMORY;
    }
    for (LONG index = 0; index < static_cast<LONG>(selected.size()); ++index) {
      ::SafeArrayPutElement(array, &index, selected[static_cast<std::size_t>(index)]);
      selected[static_cast<std::size_t>(index)]->Release();
    }
    *out = array;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_CanSelectMultiple(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = FALSE;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_IsSelectionRequired(BOOL* out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = FALSE;
    return S_OK;
  }

 private:
  std::atomic<ULONG> mRefCount{ 1 };
  std::shared_ptr<const FlatSnapshot> mSnapshot;
  int mIndex = 0;
  HWND mHwnd = nullptr;
};

IUnknown* createElementProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index,
    const HWND hwnd) {
  return static_cast<IUnknown*>(
    static_cast<IRawElementProviderSimple*>(new UiaElement(std::move(snapshot), index, hwnd)));
}

IUnknown* createGridItemProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index,
    const HWND hwnd) {
  return static_cast<IUnknown*>(new UiaGridItem(std::move(snapshot), index, hwnd));
}
IUnknown* createSelectionProvider(std::shared_ptr<const FlatSnapshot> snapshot, const int index,
    const HWND hwnd) {
  return static_cast<ISelectionProvider*>(
    new UiaSelectionProvider(std::move(snapshot), index, hwnd));
}

} // namespace

void* createUiaElementProvider(void* hwnd, AccessibilityNode snapshot, const std::string& path) {
  auto flat = flatten(snapshot);
  for (std::size_t index = 0; index < flat->nodes.size(); ++index) {
    if (flat->nodes[index].path != path) continue;
    return static_cast<IRawElementProviderSimple*>(
      new UiaElement(std::move(flat), static_cast<int>(index), static_cast<HWND>(hwnd)));
  }
  return nullptr;
}

void* createUiaRootProvider(void* hwnd, AccessibilityNode snapshot,
    AccessibilityActionInvoker invoker) {
  auto flat = flatten(snapshot);
  flat->invoker = std::move(invoker);
  if (flat->nodes.empty()) return nullptr;
  return static_cast<IRawElementProviderSimple*>(
    new UiaElement(std::move(flat), 0, static_cast<HWND>(hwnd)));
}

} // namespace coral::ui::detail
