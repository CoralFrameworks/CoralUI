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

#ifndef CORAL_UI_NATIVEPROGRESSINDICATORINTERNAL_H
#define CORAL_UI_NATIVEPROGRESSINDICATORINTERNAL_H

namespace coral::ui::macos {

void* createNativeProgressIndicatorView(bool indeterminate, bool spinning);
void setNativeProgressIndicatorIndeterminate(void* nativeView, bool indeterminate);
bool nativeProgressIndicatorIndeterminate(void* nativeView);
void setNativeProgressIndicatorValue(void* nativeView, double value);
double nativeProgressIndicatorValue(void* nativeView);
void setNativeProgressIndicatorSize(void* nativeView, int size);
int nativeProgressIndicatorSize(void* nativeView);
void setNativeProgressIndicatorDisplayedWhenStopped(void* nativeView, bool displayed);
bool nativeProgressIndicatorDisplayedWhenStopped(void* nativeView);
void setNativeProgressIndicatorStyle(void* nativeView, bool spinning);
bool nativeProgressIndicatorStyle(void* nativeView);

} // namespace coral::ui::macos

#endif
