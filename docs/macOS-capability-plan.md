# macOS 通用 GUI 能力补全计划

分支：`feature/macos-gui-capabilities`

基线提交：`1a6a656 Add a Cocoa window backend for macOS`

本文档记录 CoralUI 在 macOS 上距离“通用桌面 GUI 框架”的差距、分阶段任务、开发进度和遇到的问题。每完成一项，更新对应复选框和“进度记录”。

## 1. 当前基线

已经可用的能力：

- 标准 `NSWindow`（系统标题栏、交通灯、窗口拖拽、边缘 resize、zoom、fullscreen、Spaces）；
- `CAMetalLayer` 内容视图，Metal 渲染，Retina 2x；
- same-thread / render-thread 渲染宿主，snapshot；
- 鼠标、右键、中键、滚轮/触控板、基础键盘事件；
- live resize 全分辨率、自适应内容帧节流、系统窗口边框原生平滑；
- `NSPasteboard` 剪贴板（文本/HTML/PNG/文件 URL）；
- `NSOpenPanel` / `NSSavePanel`；
- 系统深浅色、高对比度、reduce motion 查询；
- 系统字体目录发现；
- headless/raster、`ui_native_smoke`、`test_ui`、examples checks。

明确的产品化短板：macOS 后端目前是“原生窗口 + 自绘内容运行时”，还不是完整的 AppKit 级通用 GUI 框架。

## 2. 分阶段任务

### 阶段一：系统集成基础（P0）

目标：让一个真实 macOS 应用可以输入、被辅助功能访问、使用系统菜单、接收拖放并打包发布。

#### 1.1 输入法 / 文本服务（`NSTextInputClient`）

- [x] `CoralUIView` 实现 `NSTextInputClient`；
- [x] `interpretKeyEvents:` 接入 AppKit 文本系统（文本焦点走 IME，非文本焦点/快捷键走原 KeyEvent 路径）；
- [x] marked text / unmark / commit 映射到 `WindowHost::dispatchComposition/Commit`；
- [x] 候选窗定位：`firstRectForCharacterRange:` 使用 `WindowHost::imeCursorRect()` 并转换到屏幕坐标；
- [x] `doCommandBySelector:` 把方向键、退格、删除、回车、Tab、PageUp/Down 等翻译成 `KeyEvent`；
- [x] surrounding text / selected range：UTF-8 字节偏移转换为 UTF-16 范围，供 IME 查询；
- [x] 自动化 native smoke：`ui_native_ax_smoke` 验证 AX role/AXPress 与 marked-text/commit；
- [ ] 中文、日文、韩文输入法手工验证；
- [ ] 死键、Option 变音、emoji picker 验证；
- [ ] marked text 内部 selection / replacementRange / secure input / spell check 仍待补。

验收：在 `ui_demo` 的 `TextInput` 中可以用 macOS 简体中文输入法输入、选择候选词、删除组合文本；`NSTextInputClient` 的标记文本与 CoralUI 的 composition 显示一致。

#### 1.2 Accessibility（NSAccessibility / AXUIElement）

- [x] `CoralUIView` 提供 `CoralAXElement`（`NSAccessibilityElement` 子类）子树；
- [x] 把 `AccessibilityNode` 的 role/name/description/value/state/actions/bounds 映射到 AX 属性；
- [x] 焦点、值变化、选中变化、结构变化事件（`NSAccessibilityPostNotification`，节点路径粒度待细化）；
- [x] 动作回传：AXPress、AXIncrement/AXDecrement、AXScrollToVisible；AXConfirm/AXCancel 待补；
- [x] 自动化 native smoke：`ui_native_ax_smoke` 遍历 AX 树并触发 AXPress；
- [ ] VoiceOver 手工验证；
- [ ] 开启/关闭 accessibility bridge 的策略（类似 `CORAL_UI_UIA=0`）；
- [ ] AXSetValue / 文本编辑动作、AX 行列/表格语义、hit test 与 AXFocus 的完整映射。

基础树已验证：`AXGroup` → `AXButton`/`AXTextField`，frame 为屏幕坐标，AXPress 能触发按钮回调。

验收：VoiceOver 能读出 `ui_demo` 的按钮、输入框、列表项，并执行按下/输入焦点跳转。

#### 1.3 应用生命周期与原生菜单

- [x] `NSApplicationDelegate`（`CoralAppDelegate`，单例 app delegate，不覆盖 window delegate）；
- [x] app menu（About / Services / Hide / Hide Others / Show All / Quit；Preferences 待命令 API）；
- [x] Edit menu（Undo/Redo/Cut/Copy/Paste/Delete/Select All，转译为 `KeyEvent` 交给 `TextInput`）；
- [x] Window menu（Minimize / Zoom / Bring All to Front）和 File > Close；
- [ ] `CommandRegistry` → `NSMenu` 的映射（需要命令显示名/分组 API；目前只发布标准菜单）；
- [x] 基础菜单状态校验：Edit 项按 `hasTextFocus()` 启用，其余始终可用；
- [ ] `applicationShouldTerminate` / 完整 last-window/reopen/open-files 策略；
- [ ] 原生右键菜单（`NSMenu`）接入 `Menu`/`ContextMenu`；
- [x] Cmd+W / Cmd+Q / Cmd+M / Cmd+Z / Cmd+X/C/V/A 等标准快捷键；
- [ ] 自动化测试：菜单项触发与禁用态（目前手工/代码路径验证）。

验收：菜单栏有 App/File/Edit/Window 菜单；编辑菜单能对 `TextInput` 生效；Cmd+Q/Cmd+W 行为稳定（基本完成，待补 CommandRegistry 和生命周期回调）。

#### 1.4 拖放（NSDraggingSource / NSDraggingDestination）

- [x] 注册 `NSPasteboardTypeFileURL` / `NSPasteboardTypeURL` / `NSPasteboardTypeString`；
- [x] `draggingEntered/Updated/Exited/performDragOperation` → `DragEvent`/`DropEvent`；
- [x] 外部落入路径、位置、effect、拖拽光标（文件 URL 主类型 + text/plain secondary）；
- [x] 外部 drag 进入 `WindowImpl::routeExternalDrag()`，与内部 drag 共用目标命中；
- [x] 从 CoralUI 拖出到 Finder/其他 app（`NSDraggingSource` + `NSPasteboardWriting`，文本/文件 URL/custom MIME）；
- [ ] file promise / 多文件（多文件 URL 已解析并以多个 dragging item 发出，promise 未支持）；
- [ ] 验收 demo：把文件拖进窗口，拖出文本/文件（需要真实 Finder 手工测试）。

destination 和基础 source 都已实现；source 的拖拽图像、file promise、取消/多文件体验仍需在真实 Finder 会话中验收。

#### 1.5 Sheet、原生弹窗与文件对话框增强

- [ ] `NSAlert` 封装（message/informative/buttons）；
- [x] window-attached sheet（`beginSheetModalForWindow:` + 主 runloop 等待，非活动窗口回退 app-modal）；
- [x] `NSOpenPanel`/`NSSavePanel` 的 sheet/同步包装；`FileDialogOptions.parent` 通过当前 key/main window 解析；
- [x] UTType 过滤（macOS 11+ `allowedContentTypes`，旧系统 `allowedFileTypes` fallback）；
- [x] 目录选择（open panel 仍固定不允许目录；需要 `FileDialogOptions` 扩展）、多选、默认路径；
- [ ] accessory view；
- [ ] sandbox security-scoped bookmark（如启用 App Sandbox）。

#### 1.6 macOS 打包与发布

- [x] `ui_demo` 构建 `.app` bundle（`MACOSX_BUNDLE` + Info.plist 模板）；
- [x] bundle id、版本、display name、`NSHighResolutionCapable`、`NSPrincipalClass`；
- [ ] 图标、minimum system version；
- [~] universal binary：通过 `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` 配置，尚未做成默认 preset/CI；
- [x] 代码签名与 hardened runtime 的 CMake 钩子：`CORAL_MACOS_CODE_SIGN_IDENTITY`、`CORAL_MACOS_HARDENED_RUNTIME`、`CORAL_MACOS_ENTITLEMENTS`；
- [ ] notarization 的 CI 脚本/命令；
- [ ] App Sandbox entitlements 模板；
- [ ] 文档类型 / URL scheme / open files 声明示例。

验收：`open out/examples/ui/ui_demo.app` 正常启动；签名/公证待后续。

### 阶段二：macOS 原生体验（P1）

- [ ] Toolbar / Sidebar / 统一标题栏 + 内容区；
- [ ] SF Symbols、template image、vibrancy/material、系统 focus ring、动态 accent color；
- [ ] 原生控件外观对齐 HIG：按钮、搜索框、下拉框、日期选择、Stepper、 segmented、color well；
- [ ] Popover、Sheet、TabView、Outline/Source list、可调整表头；
- [x] 窗口 frame autosave（`Window::Options::frameAutosaveName`）与原生 tabbing 基础（`TabbingMode`、Window 菜单 Show Tab Bar/Merge All Windows）；
- [ ] 完整窗口 restoration（restorationClass/state restoration）、titlebar accessory；
- [ ] 键盘输入源/布局变化、死键、secure input；
- [x] Trackpad 手势（magnify/rotate/swipe）→ `GestureEvent`/`Layer::onGesture`；
- [x] Scroll phase/momentum/precise 字段，Cocoa 从 `NSEvent.phase`/`momentumPhase` 映射；
- [ ] Force Touch、pen/pressure；
- [ ] 基于 phase 的滚动/手势识别器（当前 ScrollView 仍只消费 delta，phase 供控件使用）；
- [ ] CoreText 系统字体、spell check、autocorrect、字体面板；
- [ ] 系统 locale / 24 小时制 / 首日星期变化通知。

### 阶段三：性能、画质与工程化（P2）

- [ ] Cocoa 接入 retained/damage/frame-plan，减少全量 blit；
- [ ] CVDisplayLink / CAMetalDisplayLink vblank 调度，ProMotion/120Hz；
- [ ] occlusion/最小化暂停渲染，降低能耗；
- [ ] Display P3 / EDR / HDR10；
- [ ] window server 截图/打印/PDF 导出；
- [ ] 通知中心、Dock badge、Share Sheet、Services、打印、屏幕录制权限；
- [ ] macOS CI、原生 UI 自动化、DPR2 golden、性能/内存/能耗基准；
- [ ] 崩溃上报、App Sandbox、公证、自动更新接入。

## 3. 设计决策

- **窗口 chrome**：macOS 始终使用标准 titled `NSWindow`；`Decorations::Client` 映射为 `System`。系统窗口 server 负责拖拽、边缘 resize、zoom、Spaces。自绘标题栏留待阶段二用 titlebar accessory 方案评估。
- **内容视图**：`NSView` 不 flipped，`CAMetalLayer.geometryFlipped = NO`；输入坐标在 `devicePointForEvent` 中翻转。避免纵向 resize 时 drawable 从错误边锚定。
- **resize**：全分辨率渲染，不做半分辨率；内容帧按实际耗时自适应节流；resize 开始不切换 layer 属性，避免闪烁。
- **Metal only**：Cocoa 窗口目前只接 Metal；OpenGL 窗口路径未接。Vulkan 在 Apple 不可用。

## 4. 进度记录

| 日期 | 内容 | 状态 |
|---|---|---|
| 2026-11-23 | 创建分支与本文档；梳理三阶段任务 | 进行中 |
| 2026-11-23 | macOS 原生窗口、系统 chrome、clipboard、file dialog、live resize 基线（`1a6a656`） | 已完成 |
| 2026-11-23 | 创建 `feature/macos-gui-capabilities` 与本文档；阶段一任务拆解 | 已完成 |
| 2026-11-23 | 阶段 1.3：`CoralAppDelegate`、App/File/Edit/Window 菜单、Edit 命令转译、基础菜单校验 | 已完成（CommandRegistry/右键菜单/完整生命周期待补） |
| 2026-11-23 | 阶段 1.6：`ui_demo.app` bundle、Info.plist、版本/标识；headless CTest 从 bundle 内可执行文件运行 | 已完成（签名/公证/universal 待补） |
| 2026-11-23 | 阶段 1.1：`NSTextInputClient`、interpretKeyEvents、marked/commit、候选窗 anchor、doCommandBySelector、UTF-16 selectedRange | 代码完成，待真实中日韩输入法手工验收 |
| 2026-11-23 | 阶段 1.4：`NSDraggingDestination` 外部文件/文本落入、effect 映射、drag cursor | destination 完成，source/file promise 待补 |
| 2026-11-23 | 阶段 1.2：`CoralAXElement` 子树、role/value/frame/state 映射、AXPress/Increment/Decrement/ScrollToVisible、通知 | 基础 bridge 完成，VoiceOver/Text AX 待补 |
| 2026-11-23 | 阶段 1.4 source：`CoralDragSource`、文本/文件/custom MIME、拖拽图像 | 基础 source 完成，file promise/真实 Finder 验收待补 |
| 2026-11-23 | 阶段 1.5：文件对话框 sheet、UTType 过滤（UniformTypeIdentifiers）、默认路径/多选 | 部分完成，NSAlert/accessory/sandbox bookmark 待补 |
| 2026-11-23 | 阶段 1.6：`CORAL_MACOS_CODE_SIGN_IDENTITY` / `CORAL_MACOS_HARDENED_RUNTIME` / entitlements 签名钩子；ad-hoc 签名实测通过 | 签名钩子完成，notarization/sandbox/universal preset 待补 |
| 2026-11-23 | 新增 macOS native smoke：`ui_native_ax_smoke`（AX role/tree/AXPress + NSTextInputClient marked/commit） | 已加入 CTest native label |
| 2026-11-23 | 阶段 2 起步：`Window::Options::frameAutosaveName`/`TabbingMode`、AppKit frame autosave、tabbingIdentifier、Window 菜单 tab 项 | 基础完成，完整 restoration/titlebar accessory 待补 |
| 2026-11-23 | 阶段 2 输入：`GestureEvent`/`ScrollPhase`/`onGesture`，Cocoa magnify/rotate/swipe 与 scroll phase/momentum 映射 | 基础完成，Force Touch/pen 与基于 phase 的识别器待补 |

## 5. 问题记录

### 已解决

- `CMAKE_OBJCXX_COMPILE_OBJECT`：CoralCore 的 `.mm` 在 `OBJCXX` 启用前进入目标；在 CoralUI 根先 `enable_language(OBJCXX)`。
- 自绘标题栏边缘 resize 困难 / 窗口不跟鼠标：改为标准 `NSWindow`，系统 chrome。
- 上下 resize 时画面抖动：content view 之前是 flipped，导致 `CAMetalLayer.geometryFlipped = 1`；改为不 flipped + 手动翻转输入，并关闭 layer 隐式动画。
- 横向缩小时输入框 placeholder 越界：demo 控件改用 cross-axis stretch；`TextInput` 增加 `setClip(true)`。
- live resize 帧过长导致窗口边框卡顿：窗口改原生 chrome 后，内容帧不再决定边框流畅度；交互帧跳过 FrameClock 等待、全分辨率、自适应节流。
- 初始闪烁：resize 开始不再切换分辨率/gravity/displaySync。

### 待解决 / 已知限制

- 原生菜单目前是标准 App/File/Edit/Window；`CommandRegistry` 命令还未生成菜单项，需要“命令显示名/分组/排序”的公开元数据。
- 菜单编辑动作通过转译成 `KeyEvent` 走 `TextInput`，没有基于 responder 的文本编辑器；撤销/重做等只对焦点控件有效。
- `CoralAppDelegate` 目前只实现 secure restorable state 和 last-window 策略；open files、reopen、termination 拦截仍待补。
- IME：`setMarkedText` 忽略 marked text 内部的 selection 和 replacementRange；`selectedRange` 只做 UTF-8→UTF-16 的 caret/selection 映射；尚未验证真实中文/日文/韩文候选窗和死键。CJK 输入法需要在真实前台会话中手工测试。
- IME 的 `validAttributesForMarkedText` 只声明了下划线，marked text 仍由 CoralUI 的 composition 绘制；没有使用 AppKit 的 attributed marked text 渲染。
- 拖放目前只实现 destination；`NSDraggingSource`（从 layer 拖到 Finder/其他 app）和 file promise 未实现。外部拖放的 UI 光标/落点需要真实 Finder 手工验收。
- AX 目前是查询时重建整棵 `NSAccessibilityElement` 树；没有稳定的 element 身份，也没有 AXSetValue/文本编辑/表格行列的完整映射。焦点/值通知目前 post 到根 view，VoiceOver 可能需要更细的 element 通知。
- AX bounds 通过父元素逻辑偏移累加得到屏幕 frame；需要确认多层嵌套/Flex 布局下的偏移和 Retina 取整。
- 文件对话框 sheet 的 parent 通过当前 `NSApp.keyWindow`/`mainWindow` 解析，`FileDialogOptions.parent` 还没有直接映射到对应 `NSWindow`；非活动 app 会回退 `runModal`。`NSOpenPanel.canChooseDirectories` 仍固定 false，需要扩展 `FileDialogOptions`。
- 拖放 source 的拖拽图像对文件使用 `NSWorkspace iconForFile`，文本使用自绘标签；file promise、拖拽取消反馈、真实 Finder 多文件拖出尚未验收。
- 代码签名钩子只在显式传入 `CORAL_MACOS_CODE_SIGN_IDENTITY` 时执行；默认不签名。ad-hoc 签名已验证，Developer ID 证书与 notarization 需要真实账号。
- 窗口 autosave/tabbing 目前是 AppKit 原生基础：`frameAutosaveName` 恢复 frame，`TabbingMode` 设置 `NSWindow.tabbingMode`；还没有 Apple 的 state restoration（`restorationClass`/`encodeRestorableState`），也没有 tab 内容/新 tab 的 Window API。
- 手势路由按 pointer 命中层向父链冒泡；没有独立的 gesture recognizer/冲突仲裁，也没有 Force Touch、pen pressure 或 swipe 的完整方向/阶段语义。ScrollView 目前只消费 delta，phase 字段供上层控件使用。

- 全量 UI target + blit 每帧，Cocoa 未启用 damage/retained/frame-plan。
- 无 CVDisplayLink，`refreshHertz` 来自 `PresentFrameClock`，不代表真实显示刷新。
- `screenBounds`/`setPosition` 对副屏 origin 处理不完整。
- 无 IME、Accessibility、拖放、原生菜单、应用生命周期。
- 文件对话框使用 `allowedFileTypes`（deprecated），未使用 UTType。
- 剪贴板无 NSPasteboard 推送通知，只有 changeCount 轮询。
- OpenGL 窗口路径未接；仅 Metal。
- 尚无 `.app` 打包、签名、公证、sandbox。

## 6. 验收与回归

每阶段至少保持：

```sh
cmake --build out/examples -j8
ctest --test-dir out/examples -L examples --output-on-failure
ctest --test-dir out/tests -R "^test_ui$|^test_ui_shared_device$|^ui_native_smoke$|^ui_native_ax_smoke$" --output-on-failure
```

新增能力要补对应的 native smoke 或手工验收步骤，并记录到本文档。
