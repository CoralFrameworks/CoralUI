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

#ifndef CORAL_UI_UI_H
#define CORAL_UI_UI_H

/**
 * The SDK umbrella header.
 *
 * A consumer of the installed package can write `#include <ui/UI.h>` and get the retained tree, the
 * element/reconciler pair, the styling and component model, the common controls, the layout
 * containers and the animation helpers. Individual headers stay usable for a minimal include set;
 * this one exists so an application does not have to enumerate a dozen paths to get started.
 */

#include <ui/Animation.h>
#include <ui/Application.h>
#include <ui/Button.h>
#include <ui/CheckBox.h>
#include <ui/Clipboard.h>
#include <ui/ColorButton.h>
#include <ui/ColorLayer.h>
#include <ui/Command.h>
#include <ui/Component.h>
#include <ui/Curve.h>
#include <ui/DateFormat.h>
#include <ui/DockView.h>
#include <ui/DragDrop.h>
#include <ui/Element.h>
#include <ui/ElementRegistry.h>
#include <ui/ElementSpec.h>
#include <ui/Flex.h>
#include <ui/FrameStatsOverlay.h>
#include <ui/HotReload.h>
#include <ui/ProcessStats.h>
#include <ui/DSL.h>
#include <ui/DSLOverlay.h>
#include <ui/Grid.h>
#include <ui/IconButton.h>
#include <ui/Layer.h>
#include <ui/Motion.h>
#include <ui/NumberFormat.h>
#include <ui/Overlay.h>
#include <ui/OverlayHost.h>
#include <ui/ProgressBar.h>
#include <ui/RadioButton.h>
#include <ui/Reconciler.h>
#include <ui/RecyclingListView.h>
#include <ui/ScrollView.h>
#include <ui/SegmentedControl.h>
#include <ui/Slider.h>
#include <ui/Spinner.h>
#include <ui/Style.h>
#include <ui/TableView.h>
#include <ui/Table.h>
#include <ui/TextInput.h>
#include <ui/TitleBar.h>
#include <ui/Theme.h>
#include <ui/Translation.h>
#include <ui/Toggle.h>
#include <ui/Window.h>

#endif // CORAL_UI_UI_H
