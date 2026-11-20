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

#ifndef CORAL_UI_LISTVIEW_H
#define CORAL_UI_LISTVIEW_H

#include <ui/Types.h>

#include <ui/ScrollView.h>


#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;

/**
 * A virtualized selectable list inside a scroll view.
 *
 * Unlike the earlier implementation this does not allocate one Layer per item. A single content
 * layer measures the full list height and draws only the rows intersecting the viewport; pointer and
 * keyboard input resolve to an item index. The public API is unchanged.
 */
class ListView final : public ScrollView {
 public:
  ListView(std::shared_ptr<const Font> font, float rowHeight = 30.0f);
  ~ListView() override;

  void addItem(std::string text);
  void clear();
  /** Replaces the whole model; a selection outside the new range is cleared. */
  void setItems(std::vector<std::string> items);
  [[nodiscard]] const std::vector<std::string>& items() const noexcept {
    return mItems;
  }
  [[nodiscard]] size_t itemCount() const noexcept {
    return mItems.size();
  }

  void setSelectedIndex(int index);
  [[nodiscard]] int selectedIndex() const noexcept {
    return mSelected;
  }

  void setOnSelection(std::function<void(int, const std::string&)> callback);
  void setRowColors(Color normal, Color selected);

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  class VirtualContent;
  friend class VirtualContent;

  void select(int index);
  void ensureVisible(int index);

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<VirtualContent> mVirtual;
  std::vector<std::string> mItems;
  std::function<void(int, const std::string&)> mOnSelection;
  float mRowHeight = 30.0f;
  int mSelected = -1;
  Color mNormalColor = Color::MakeRGBA8(35, 38, 46, 255);
  Color mSelectedColor = Color::MakeRGBA8(52, 56, 66, 255);
  Color mTextColor = Color::MakeRGBA8(226, 232, 240, 255);
};

} // namespace coral::ui

#endif // CORAL_UI_LISTVIEW_H
