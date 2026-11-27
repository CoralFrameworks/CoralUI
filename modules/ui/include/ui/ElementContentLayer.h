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

#ifndef CORAL_UI_ELEMENTCONTENTLAYER_H
#define CORAL_UI_ELEMENTCONTENTLAYER_H

#include <ui/Element.h>

#include <memory>

namespace coral::ui {

class Reconciler;

/**
 * A retained Layer whose contents are an Element tree.
 *
 * This is the bridge used when an API owns a `Layer*` slot (Dialog content, Popup content) but the
 * caller wants to describe that content with the Element/DSL model. The layer owns a Reconciler and
 * forwards measure/layout to its mounted root.
 */
class ElementContentLayer final : public Layer {
 public:
  explicit ElementContentLayer(ElementPtr content = {});
  ~ElementContentLayer() override;

  ElementContentLayer(const ElementContentLayer&) = delete;
  ElementContentLayer& operator=(const ElementContentLayer&) = delete;

  void setContent(ElementPtr content);
  [[nodiscard]] const ElementPtr& content() const noexcept {
    return mContent;
  }

  Size measure(const Size& available) override;
  void layout() override;

 private:
  std::unique_ptr<Reconciler> mReconciler;
  std::shared_ptr<Layer> mRoot;
  ElementPtr mContent;
};

} // namespace coral::ui

#endif // CORAL_UI_ELEMENTCONTENTLAYER_H
