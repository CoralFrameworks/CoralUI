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

#ifndef CORAL_UI_NATIVESTEPPERINTERNAL_H
#define CORAL_UI_NATIVESTEPPERINTERNAL_H

#include <functional>

namespace coral::ui::macos {

void* createNativeStepperView(double minimum, double maximum, double value,
  std::function<void(double)> onChange);
void setNativeStepperRange(void* nativeView, double minimum, double maximum);
void setNativeStepperValue(void* nativeView, double value);
double nativeStepperValue(void* nativeView);
void setNativeStepperIncrement(void* nativeView, double increment);
void setNativeStepperSize(void* nativeView, int size);
int nativeStepperSize(void* nativeView);
void setNativeStepperValueWraps(void* nativeView, bool wraps);
void setNativeStepperAutorepeat(void* nativeView, bool autorepeat);
double nativeStepperIncrement(void* nativeView);

} // namespace coral::ui::macos

#endif
