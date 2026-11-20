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

#include <ui/ClipboardMime.h>

#include <gtest/gtest.h>

using namespace coral::ui;

TEST(UiClipboardMime, MapsTargetsToMimeAndBack) {
  EXPECT_EQ(mimeForSelectionTarget("UTF8_STRING"), "text/plain");
  EXPECT_EQ(mimeForSelectionTarget("TEXT"), "text/plain");
  EXPECT_EQ(mimeForSelectionTarget("text/uri-list"), "text/uri-list");
  EXPECT_EQ(mimeForSelectionTarget("image/png"), "image/png");
  EXPECT_EQ(mimeForSelectionTarget("text/html"), "text/html");
  EXPECT_TRUE(mimeForSelectionTarget("COMPOUND_TEXT").empty()) << "unsupported targets map to nothing";

  EXPECT_EQ(selectionTargetForMime("text/plain"), "UTF8_STRING");
  EXPECT_EQ(selectionTargetForMime("image/png"), "image/png");
  EXPECT_TRUE(selectionTargetForMime("SAVE_TARGETS").empty())
    << "a target name without a slash cannot be offered as a MIME type";
}
TEST(UiClipboardMime, MimeShapedTargetsAreTheirOwnMapping) {
  // Some applications use the MIME type itself as the target atom, including private types that have
  // no dedicated target name; the mapping has to keep those instead of dropping them.
  EXPECT_EQ(mimeForSelectionTarget("text/plain"), "text/plain");
  EXPECT_EQ(mimeForSelectionTarget("application/x-coral-clip"), "application/x-coral-clip");
  EXPECT_EQ(selectionTargetForMime("application/x-coral-clip"), "application/x-coral-clip");
  EXPECT_TRUE(mimeForSelectionTarget("SAVE_TARGETS").empty())
    << "a target without a slash is not a MIME type";
}
TEST(UiClipboardMime, PayloadKeepsSeveralMimeTypes) {
  ClipboardPayload payload;
  EXPECT_TRUE(payload.formats().empty());
  payload.set("text/plain", "hi");
  payload.set("image/png", std::string("\x89PNG\0", 5));
  EXPECT_TRUE(payload.hasFormat("text/plain"));
  EXPECT_EQ(payload.get("text/plain"), "hi");
  EXPECT_EQ(payload.get("image/png").size(), 5u) << "binary data keeps its NUL";
  EXPECT_TRUE(payload.get("text/html").empty()) << "an absent format reads as empty";
  EXPECT_EQ(payload.formats().size(), 2u);

  payload.set("text/plain", "replaced");
  EXPECT_EQ(payload.get("text/plain"), "replaced") << "set replaces the MIME payload";
  EXPECT_TRUE(payload.erase("image/png"));
  EXPECT_FALSE(payload.erase("image/png")) << "erasing twice reports absence";
  EXPECT_EQ(payload.formats().size(), 1u);
  payload.clear();
  EXPECT_TRUE(payload.formats().empty());
}
