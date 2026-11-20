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

/**
 * @file ui_mvvm_demo.cpp
 *
 * A small reference page for the declarative slice: a view model is put in a `DataContext`, the
 * page is rebuilt as an immutable `Element` tree, and `Reconciler` patches the existing `Layer`
 * tree. `When` chooses a branch; `ForEach` maps keyed rows; `Component` rebuilds a subtree from the
 * model without replacing its outer layer.
 *
 * The demo is headless by default so it can run in CI:
 *
 *   ui_mvvm_demo [--out FILE] [--frames N]
 */

#include <ui/Application.h>
#include <ui/Button.h>
#include <ui/Element.h>
#include <ui/Reconciler.h>
#include <ui/Window.h>
#include <ui/mvvm/DataContext.h>

#include <mvvm/Command.h>

#include <graphics/Bitmap.h>
#include <graphics/Data.h>
#include <graphics/Font.h>
#include <graphics/Image.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace coral::ui;
using coral::ui::mvvm::DataContext;

struct Row {
  std::string label;
  bool highlighted = false;
};

struct PageModel {
  std::string title = "Coral MVVM + Element";
  bool showDetail = true;
  std::vector<Row> rows {
    Row{ "Clip A", true },
    Row{ "Clip B", false },
    Row{ "Clip C", false },
  };
};

bool writePng(const graphics::Bitmap& bitmap, const std::string& path) {
  if (bitmap.isEmpty()) return false;
  std::string error;
  const graphics::Data encoded = graphics::Image::fromBitmap(bitmap).encode(
    graphics::ImageFormat::PNG, 100, &error);
  if (encoded.size() == 0) {
    std::fprintf(stderr, "could not encode the UI frame: %s\n", error.c_str());
    return false;
  }
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(encoded.bytes()),
    static_cast<std::streamsize>(encoded.size()));
  return stream.good();
}

} // namespace

int main(int argc, char** argv) {
  std::string output = "ui_mvvm_demo.png";
  int frames = 1;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--out" && i + 1 < argc) {
      output = argv[++i];
    } else if (argument == "--frames" && i + 1 < argc) {
      frames = std::max(1, std::atoi(argv[++i]));
    } else if (argument == "--help") {
      std::printf("usage: ui_mvvm_demo [--out FILE] [--frames N]\n");
      return 0;
    }
  }

  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    std::fprintf(stderr, "could not load the reference font %s\n", fontPath.c_str());
    return 1;
  }
  const auto font = graphics::Font::create(typeface, 16.0f);
  if (font == nullptr) return 1;

  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 420;
  options.height = 240;
  auto window = application->createWindow(options);
  if (window == nullptr) {
    std::fprintf(stderr, "could not create the headless window\n");
    return 1;
  }

  auto model = std::make_shared<PageModel>();
  DataContext context;
  context.set(model);

  Reconciler reconciler;
  std::function<ElementPtr()> build;
  auto refresh = std::make_shared<coral::mvvm::Command>([&] {
    model->title = "Refreshed from a Command";
    if (build) reconciler.update(build());
  });
  build = [&] {
    const auto& page = *model;
    std::vector<ElementPtr> children;
    children.push_back(std::make_shared<TextElement>(page.title, font,
      Color::MakeRGBA8(235, 238, 245, 255), "title"));
    ElementProps searchProps;
    searchProps.bounds = Rect::MakeWH(220.0f, 28.0f);
    children.push_back(components::WithProps(
      components::TextInput("Search clips", font, [](const std::string&) {}, "search"),
      searchProps, "search-props"));
    ElementProps refreshProps;
    refreshProps.bounds = Rect::MakeWH(90.0f, 28.0f);
    children.push_back(components::WithProps(
      components::Button("Refresh", font, [refresh] { refresh->execute(); }, "refresh",
        refresh->canExecute()), refreshProps, "refresh-props"));
    children.push_back(components::When(page.showDetail,
      std::make_shared<TextElement>("Select a clip to inspect its properties.", font,
        Color::MakeRGBA8(160, 168, 180, 255), "detail"),
      {}, "detail"));
    std::vector<std::string> rowKeys;
    rowKeys.reserve(page.rows.size());
    for (size_t i = 0; i < page.rows.size(); ++i) rowKeys.push_back("row-" + std::to_string(i));
    ElementProps listProps;
    listProps.bounds = Rect::MakeWH(220.0f, 78.0f);
    children.push_back(components::WithProps(
      components::RecyclingList(std::move(rowKeys),
        [model, font](const std::string&, const size_t index, const RecyclingRowState&) {
          const auto& rows = model->rows;
          const Row& row = rows[std::min(index, rows.size() - 1)];
          return std::make_shared<TextElement>(row.label, font,
            row.highlighted ? Color::MakeRGBA8(120, 180, 255, 255)
                            : Color::MakeRGBA8(200, 205, 215, 255),
            "row-" + std::to_string(index));
        }, 22.0f, "rows"), listProps, "rows-props"));
    return std::make_shared<StackElement>(Axis::Column, std::move(children), "page", 10.0f,
      Insets{}.all(16.0f));
  };
  const auto root = reconciler.mount(build());
  window->setRoot(root);

  // Headless invocation of the reference page: activate the Command-backed button exactly as a
  // keyboard user would, then render the patched tree. Real windows deliver this through input.
  if (root->children().size() >= 3) {
    if (const auto button = std::static_pointer_cast<Button>(root->children()[2])) {
      KeyEvent space;
      space.key = Key::Space;
      button->onKeyDown(space);
    }
  }

  for (int i = 0; i < frames; ++i) {
    if (!window->renderFrame()) {
      std::fprintf(stderr, "the frame could not be rendered\n");
      return 1;
    }
  }

  graphics::Bitmap bitmap;
  if (!window->snapshot(&bitmap) || !writePng(bitmap, output)) {
    std::fprintf(stderr, "could not write %s\n", output.c_str());
    return 1;
  }
  std::printf("ui_mvvm_demo wrote %s (%dx%d)\n", output.c_str(), bitmap.width(),
    bitmap.height());
  return 0;
}
