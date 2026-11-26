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

#ifndef CORAL_UI_DSL_H
#define CORAL_UI_DSL_H

/**
 * Umbrella header for the DSL.
 *
 * `DSLCore.h` is the stable API contract; `DSLOverlay.h` and `DSLStyle.h` are its stable companions.
 * Everything under `dsl::experimental` is a convenience forwarder and is not API-stable.
 */
#include <ui/DSLCore.h>
#include <ui/DSLExperimental.h>
#include <ui/DSLOverlay.h>
#include <ui/DSLStyle.h>

#endif // CORAL_UI_DSL_H