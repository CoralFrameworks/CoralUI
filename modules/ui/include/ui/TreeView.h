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

#ifndef CORAL_UI_TREEVIEW_H
#define CORAL_UI_TREEVIEW_H

#include <ui/Types.h>

#include <ui/ScrollView.h>


#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace graphics {
class Image;
}

namespace coral::ui {

struct Theme;
class TextPainter;

/**
 * A small hierarchical list with expandable folder rows.
 *
 * Nodes are addressed by stable integer ids returned from `addRoot`/`addChild`; ids are never
 * reused until `clear()`. The visible rows live in a flat vector and one content layer measures the
 * full height and draws only the rows intersecting the viewport, so a large media tree costs the
 * same as the visible part rather than one Layer per node.
 */
class TreeView final : public ScrollView {
 public:
  TreeView(std::shared_ptr<const Font> font, float rowHeight = 28.0f);
  ~TreeView() override;

  int addRoot(std::string label, bool expanded = true);
  int addChild(int parent, std::string label, bool folder = false);
  void clear();

  void setExpanded(int id, bool expanded);
  void setSelected(int id);
  [[nodiscard]] int selected() const noexcept {
    return mSelected;
  }
  [[nodiscard]] std::string label(int id) const;
  [[nodiscard]] bool isFolder(int id) const;
  [[nodiscard]] size_t visibleCount() const noexcept {
    return mRows.size();
  }
  /** Optional row thumbnail; drawn at 18x18 before the label. */
  void setNodeIcon(int id, std::shared_ptr<const Image> icon);
  /** Optional right-aligned metadata such as "1.2 MB". */
  void setNodeDetail(int id, std::string detail);

  void setOnSelection(std::function<void(int, const std::string&)> callback);
  void setOnActivated(std::function<void(int, const std::string&)> callback);

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  struct Node {
    std::string label;
    std::string detail;
    std::shared_ptr<const Image> icon;
    int parent = -1;
    bool folder = false;
    bool expanded = false;
    std::vector<int> children;
  };

  struct Row {
    int id = -1;
    int depth = 0;
  };

  friend class TreeVirtualContent;
  void rebuildRows();
  void toggle(int id);
  void select(int id);
  void activate(int id);
  void ensureVisible(int row);
  [[nodiscard]] bool visible(int id) const;

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<class TreeVirtualContent> mVirtual;
  std::vector<Node> mNodes;
  std::vector<Row> mRows;
  std::function<void(int, const std::string&)> mOnSelection;
  std::function<void(int, const std::string&)> mOnActivated;
  float mRowHeight = 28.0f;
  int mSelected = -1;
  Color mNormalColor = Color::MakeRGBA8(35, 38, 46, 255);
  Color mSelectedColor = Color::MakeRGBA8(52, 56, 66, 255);
  Color mTextColor = Color::MakeRGBA8(226, 232, 240, 255);
  Color mMutedColor = Color::MakeRGBA8(148, 163, 184, 255);
};

} // namespace coral::ui

#endif // CORAL_UI_TREEVIEW_H
