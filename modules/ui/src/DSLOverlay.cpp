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

#include <ui/DSLOverlay.h>

#include <ui/Dialog.h>
#include <ui/ElementContentLayer.h>
#include <ui/Element.h>
#include <ui/Menu.h>
#include <ui/Popup.h>
#include <ui/Toast.h>
#include <ui/Tooltip.h>
#include <ui/Window.h>

#include <algorithm>
#include <utility>

namespace coral::ui::dsl {

Overlay::Overlay(Window& window, const Theme& theme)
    : mWindow(&window), mTheme(theme), mHost(window) {}

Overlay::~Overlay() = default;

uint64_t Overlay::Toast(std::string text, const std::chrono::milliseconds duration) {
  auto toast = std::make_shared<coral::ui::Toast>(std::move(text), mTheme.body);
  toast->showFor(toast->text(), duration);

  OverlayOptions options;
  options.modal = false;
  options.dismissOnOutsideClick = false;
  options.passThroughPointer = true;
  const Rect area = mWindow->bounds();
  options.anchor = Rect::MakeXYWH(area.getLeft() + 24.0f, area.getBottom() - 96.0f,
    std::max(0.0f, area.getWidth() - 48.0f), 32.0f);
  options.preferred = Placement::Center;

  const uint64_t id = mHost.show(components::Layer(toast, "dsl.toast"), options);
  if (id != 0) {
    toast->setOnDismissed([this, id] { close(id); });
  }
  return id;
}

uint64_t Overlay::Tooltip(std::string text, const Point anchor) {
  auto tooltip = std::make_shared<coral::ui::Tooltip>(std::move(text), mTheme.body);
  tooltip->showAt(anchor);

  OverlayOptions options;
  options.modal = false;
  options.dismissOnOutsideClick = true;
  options.anchor = Rect::MakeXYWH(anchor.x, anchor.y, 1.0f, 1.0f);
  options.preferred = Placement::BelowStart;
  return mHost.show(components::Layer(tooltip, "dsl.tooltip"), options);
}

uint64_t Overlay::Dialog(std::string title, std::string message,
    std::shared_ptr<Layer> content, std::vector<DialogButton> buttons) {
  auto dialog = std::make_shared<coral::ui::Dialog>(std::move(title), mTheme.title);
  if (!message.empty()) dialog->setMessage(std::move(message));
  if (content != nullptr) dialog->setContent(std::move(content));

  auto handle = std::make_shared<uint64_t>(0);
  for (DialogButton& button: buttons) {
    dialog->addButton(std::move(button.label), [this, handle, action = std::move(button.action)] {
      if (action) action();
      if (*handle != 0) close(*handle);
    });
  }
  dialog->show();

  OverlayOptions options;
  options.modal = true;
  options.drawScrim = true;
  const uint64_t id = mHost.show(components::Layer(dialog, "dsl.dialog"), options);
  *handle = id;
  return id;
}

uint64_t Overlay::DialogElement(std::string title, std::string message, ElementPtr content,
    std::vector<DialogButton> buttons) {
  std::shared_ptr<Layer> layer;
  if (content != nullptr) layer = std::make_shared<ElementContentLayer>(std::move(content));
  return Dialog(std::move(title), std::move(message), std::move(layer), std::move(buttons));
}

uint64_t Overlay::PopupElement(ElementPtr content, OverlayOptions options) {
  if (content == nullptr) return 0;
  return Popup(std::make_shared<ElementContentLayer>(std::move(content)), std::move(options));
}
uint64_t Overlay::Popup(std::shared_ptr<Layer> content, OverlayOptions options) {
  if (content == nullptr) return 0;
  auto popup = std::make_shared<coral::ui::Popup>(std::move(content));
  if (!options.anchor.isEmpty()) popup->showAt(options.anchor);
  return mHost.show(components::Layer(popup, "dsl.popup"), std::move(options));
}

uint64_t Overlay::Menu(std::vector<std::string> items, const Point anchor,
    std::function<void(int)> onSelected) {
  auto menu = std::make_shared<coral::ui::Menu>(mTheme.body);
  auto handle = std::make_shared<uint64_t>(0);
  for (std::size_t i = 0; i < items.size(); ++i) {
    menu->addItem(std::move(items[i]), [this, handle, onSelected, i] {
      if (onSelected) onSelected(static_cast<int>(i));
      if (*handle != 0) close(*handle);
    });
  }
  menu->showAt(anchor);

  OverlayOptions options;
  options.modal = false;
  options.dismissOnOutsideClick = true;
  options.anchor = Rect::MakeXYWH(anchor.x, anchor.y, 1.0f, 1.0f);
  options.preferred = Placement::BelowStart;
  const uint64_t id = mHost.show(components::Layer(menu, "dsl.menu"), options);
  *handle = id;
  return id;
}

bool Overlay::close(const uint64_t id) {
  return mHost.dismiss(id);
}

void Overlay::closeAll() {
  mHost.dismissAll();
}

} // namespace coral::ui::dsl
