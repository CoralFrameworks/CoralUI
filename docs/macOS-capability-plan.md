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
- [x] AX 文本：editable/value/selectedTextRange/selectedText、AXSetValue → `TextInput::SetValue`；
- [x] AX 表格/行列语义：rowCount/columnCount/rowIndexRange/columnIndexRange（来自 `AccessibleGrid`）；
- [x] AX hit test（`accessibilityHitTest:` 按屏幕 frame 找最深元素）与 cell/row role；
- [x] AXFocus 通知 post 到当前 focused AX element（无 element 时回退根 view）；
- [ ] VoiceOver 实测 AXFocus 行为；

基础树已验证：`AXGroup` → `AXButton`/`AXTextField`，frame 为屏幕坐标，AXPress 能触发按钮回调。

验收：VoiceOver 能读出 `ui_demo` 的按钮、输入框、列表项，并执行按下/输入焦点跳转。

#### 1.3 应用生命周期与原生菜单

- [x] `NSApplicationDelegate`（`CoralAppDelegate`，单例 app delegate，不覆盖 window delegate）；
- [x] app menu（About / Services / Hide / Hide Others / Show All / Quit；Preferences 待命令 API）；
- [x] Edit menu（Undo/Redo/Cut/Copy/Paste/Delete/Select All，转译为 `KeyEvent` 交给 `TextInput`）；
- [x] Window menu（Minimize / Zoom / Bring All to Front）和 File > Close；
- [ ] `CommandRegistry` → `NSMenu` 的映射（需要命令显示名/分组 API；目前只发布标准菜单）；
- [x] 基础菜单状态校验：Edit 项按 `hasTextFocus()` 启用，其余始终可用；
- [x] `applicationShouldTerminate` / reopen / open files 回调（`ui/ApplicationLifecycle.h`，进程级 handler，可 veto termination）；
- [x] Cmd+Q 先检查所有 Cocoa 窗口的 close interceptor，全部允许后请求关闭并 `NSTerminateLater`，最后一个窗口销毁时 reply YES；任一窗口 veto 则取消 quit；
- [ ] 多 Application 实例的进程级 handler 冲突（AppKit 进程内通常只有一个 Application）；
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

- [x] `NSAlert` 封装（title/message/buttons/defaultButton/style，sheet + app-modal fallback）；
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
- [x] universal binary：`macos-arm64` / `macos-x86_64` 两个 slice + `scripts/make-universal-demo.sh` lipo 成 universal `.app`；单次 configure 的 universal 仍被 vendored libjpeg-turbo 拒绝；
- [x] 代码签名与 hardened runtime 的 CMake 钩子：`CORAL_MACOS_CODE_SIGN_IDENTITY`、`CORAL_MACOS_HARDENED_RUNTIME`、`CORAL_MACOS_ENTITLEMENTS`；
- [x] notarization 脚本：`scripts/notarize-macos-app.sh`（notarytool submit --wait + stapler staple/validate）；
- [x] App Sandbox entitlements 模板：`examples/ui/CoralUIDemo.entitlements`；
- [x] security-scoped bookmark API：`createSecurityScopedBookmark` / `SecurityScopedAccess`；
- [ ] sandbox-enabled 签名 demo 的 bookmark 读写实测；
- [ ] 文档类型 / URL scheme / open files 声明示例。

验收：`open out/examples/ui/ui_demo.app` 正常启动；签名/公证待后续。

### 阶段二：macOS 原生体验（P1）

- [x] Native Toolbar 基础：`Window::ToolbarItem`、`setToolbarItems/Visible/ActionHandler`、NSToolbar + SF Symbol 图标；
- [x] Sidebar 基础：`Window::SidebarItem`/`setSidebarItems`/selection handler，NSSplitViewController + NSVisualEffectView + NSTableView + SF Symbol；
- [x] Sidebar section/group header（不可选、粗体小字号）；
- [x] Titlebar accessory 基础：`Window::TitlebarAccessoryItem` + NSTitlebarAccessoryViewController + NSSegmentedControl；
- [ ] Sidebar outline/可编辑/拖拽、统一标题栏 + 内容区、任意 titlebar accessory view；
- [x] Toolbar `allowsUserCustomization`/`autosavesConfiguration`；
- [ ] Toolbar overflow/fullscreen 行为和内容区尺寸联动；
- [x] SF Symbols → `graphics::Image`（`systemSymbolImage`，pointSize/scale，template 标记）；
- [ ] template tinting/动态 accent color、vibrancy/material、系统 focus ring；
- [ ] 原生控件外观对齐 HIG：按钮、搜索框、下拉框、日期选择、Stepper、 segmented、color well；
- [ ] Popover、Sheet、TabView、Outline/Source list、可调整表头；
- [x] 窗口 frame autosave（`Window::Options::frameAutosaveName`）与原生 tabbing 基础（`TabbingMode`、Window 菜单 Show Tab Bar/Merge All Windows）；
- [ ] 完整窗口 restoration（restorationClass/state restoration）、titlebar accessory；
- [x] macOS secure event input（`Window::setSecureInputEnabled`，Carbon Enable/DisableSecureEventInput，进程级计数）；
- [x] 键盘输入源查询/变化通知：`currentKeyboardInputSource`/`setInputSourceChangedHandler`（TIS distributed notification）；
- [ ] 键盘布局映射/死键；
- [x] Trackpad 手势（magnify/rotate/swipe）→ `GestureEvent`/`Layer::onGesture`；
- [x] Scroll phase/momentum/precise 字段，Cocoa 从 `NSEvent.phase`/`momentumPhase` 映射；
- [ ] Force Touch、pen/pressure；
- [ ] 基于 phase 的滚动/手势识别器（当前 ScrollView 仍只消费 delta，phase 供控件使用）；
- [x] `NSSpellChecker` spell check → UTF-8 byte ranges（`ui/SpellCheck.h`，native smoke 校验 misspelling）；
- [x] `TextInput::setSpellCheckEnabled()` 把 spell issues 画成红色下划线（单行/多行，UTF-8 byte range → 视觉 x）；
- [x] 右键菜单拼写替换建议（TextInput 在 misspelled word 上动态插入 NSSpellChecker suggestions）；
- [ ] autocorrect、字体面板；
- [ ] CoreText 系统字体高级特性（variations/features/fallback tuning）；
- [ ] 系统 locale / 24 小时制 / 首日星期变化通知。

### 阶段三：性能、画质与工程化（P2）

- [x] Cocoa 接入 retained frames 与 damage rects（`presentRetained`/`presentDamage`），native smoke 验证 clean frame 不重新录制、damage 后 snapshot 正确；
- [x] Cocoa 接入 frame-plan/boundary submission（`CORAL_UI_FRAME_PLAN_WINDOW=1` opt-in），native smoke 验证 plan items/command buffers；
- [ ] CVDisplayLink / CAMetalDisplayLink vblank 调度，ProMotion/120Hz；
- [x] occlusion 暂停自动帧（`NSWindowOcclusionState` → `WindowHost::platformOcclusionChanged`，重新可见时 damageAll + requestFrame）；
- [x] 最小化/隐藏时不再由自动循环持续渲染（显式 `Window::renderFrame()` 仍可 snapshot）；
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
| 2026-11-23 | 阶段 1.3：`ApplicationLifecycle` openURLs/reopen/shouldTerminate handler，native smoke 校验 | 回调完成，自动 quit→close 拦截与多 Application 待补 |
| 2026-11-23 | 阶段 1.3：Cocoa 窗口注册表 + `requestQuit` 尊重 close interceptor，`NSTerminateLater` 在最后一个窗口销毁时 reply；native smoke 校验窗口 veto | 完成 veto 路径，完整 quit 完成路径需手工验收 |
| 2026-11-23 | 阶段 1.6：`ui_demo.app` bundle、Info.plist、版本/标识；headless CTest 从 bundle 内可执行文件运行 | 已完成（签名/公证/universal 待补） |
| 2026-11-23 | 阶段 1.1：`NSTextInputClient`、interpretKeyEvents、marked/commit、候选窗 anchor、doCommandBySelector、UTF-16 selectedRange | 代码完成，待真实中日韩输入法手工验收 |
| 2026-11-23 | 阶段 1.4：`NSDraggingDestination` 外部文件/文本落入、effect 映射、drag cursor | destination 完成，source/file promise 待补 |
| 2026-11-23 | 阶段 1.2：`CoralAXElement` 子树、role/value/frame/state 映射、AXPress/Increment/Decrement/ScrollToVisible、通知 | 基础 bridge 完成，VoiceOver/Text AX 待补 |
| 2026-11-23 | 阶段 1.2 AX 文本：TextInput 暴露 accessibleValue，CoralAXElement 映射 editable/value/selection，AXSetValue 走 SetValue action | 文本 AX 基础完成，表格/焦点/hit test 待补 |
| 2026-11-23 | 阶段 1.2 AX 表格：grid rowCount/columnCount/index ranges，native smoke 用 Table 校验 2x2 | 表格尺寸完成，cell role/hit test 待补 |
| 2026-11-23 | 阶段 1.2 AX hit test/cell：`accessibilityHitTest:`、grid cell → AXCell、ListItem → AXRow；native smoke 校验按钮命中 | 完成基础命中，AXFocus 待补 |
| 2026-11-23 | 阶段 1.2 AXFocus：通知改为 post 到 focused AX element；无匹配时回退根 view | 完成通知目标，VoiceOver 实测待补 |
| 2026-11-23 | 阶段 1.4 source：`CoralDragSource`、文本/文件/custom MIME、拖拽图像 | 基础 source 完成，file promise/真实 Finder 验收待补 |
| 2026-11-23 | 阶段 1.5：文件对话框 sheet、UTType 过滤（UniformTypeIdentifiers）、默认路径/多选 | 部分完成，NSAlert/accessory/sandbox bookmark 待补 |
| 2026-11-23 | 阶段 1.5：`NativeAlert.h` / NSAlert sheet 封装，native smoke 校验 `supportsNativeAlerts` | NSAlert 基础完成，真实弹窗手工验收/accessory 待补 |
| 2026-11-23 | 阶段 1.6：`CORAL_MACOS_CODE_SIGN_IDENTITY` / `CORAL_MACOS_HARDENED_RUNTIME` / entitlements 签名钩子；ad-hoc 签名实测通过 | 签名钩子完成，notarization/sandbox/universal preset 待补 |
| 2026-11-23 | 阶段 1.6 universal：`CMakePresets.json` arm64/x86_64 preset、host shader compiler 复用、`scripts/make-universal-demo.sh` lipo 验证 dual-arch；notarization 脚本 | universal/notarization 脚本完成，真实公证/sandbox 待补 |
| 2026-11-23 | 工程化：`scripts/check-macos.sh` 本地 CI（preset 构建 + test_ui/native/example），实测 6/6 | 本地 CI 完成，托管 CI/性能门禁待补 |
| 2026-11-23 | 阶段 1.6 sandbox：entitlements 模板 + security-scoped bookmark API；非 sandbox 环境下创建 bookmark 按预期失败 | API/模板完成，sandbox 签名实测待补 |
| 2026-11-23 | 新增 macOS native smoke：`ui_native_ax_smoke`（AX role/tree/AXPress + NSTextInputClient marked/commit） | 已加入 CTest native label |
| 2026-11-23 | 阶段 2 起步：`Window::Options::frameAutosaveName`/`TabbingMode`、AppKit frame autosave、tabbingIdentifier、Window 菜单 tab 项 | 基础完成，完整 restoration/titlebar accessory 待补 |
| 2026-11-23 | 阶段 2 Toolbar：`Window::ToolbarItem`/`setToolbarItems`/handler、NSToolbar delegate、SF Symbol 图标；native smoke 校验 item/action | 基础完成，Sidebar/自定义/尺寸联动待补 |
| 2026-11-23 | 阶段 2 Sidebar：`Window::SidebarItem`/selection、NSSplitViewController + visual-effect sidebar + NSTableView；native smoke 校验 split item/rows/selection | 基础完成，分组/outline/titlebar accessory 待补 |
| 2026-11-23 | 阶段 2 Sidebar section header + Toolbar customization/autosave；native smoke 校验 group row 与选择 | 分组完成，outline/titlebar/custom toolbar 细节待补 |
| 2026-11-23 | 阶段 2 titlebar accessory：`TitlebarAccessoryItem`、trailing NSTitlebarAccessoryViewController、NSSegmentedControl、selection | 基础完成，任意 accessory view/leading 布局待补 |
| 2026-11-23 | 阶段 2 secure input：`setSecureInputEnabled`/`isSecureInputEnabled`，Carbon 安全事件输入，native smoke 校验 | 完成基础能力，进程级/多窗口语义待补 |
| 2026-11-23 | 阶段 2 键盘输入源：TIS current ID/name + `kTISNotifySelectedKeyboardInputSourceChanged` 主队列回调；native smoke 读取 SCIM | 查询/通知完成，布局/死键待补 |
| 2026-11-23 | 阶段 2 spell check：`ui/SpellCheck.h` + NSSpellChecker，UTF-16→UTF-8 byte ranges，native smoke 校验 | API 完成，TextInput 下划线/autocorrect/字体面板待补 |
| 2026-11-23 | 阶段 2 spell check UI：TextInput `setSpellCheckEnabled` 自动查询并绘制单行/多行红色下划线 | 下划线完成，右键建议/autocorrect/字体面板待补 |
| 2026-11-23 | 阶段 2 spell suggestions：`spellSuggestions` + TextInput 动态右键菜单替换 word range；native smoke 校验 8 个候选 | 右键建议完成，autocorrect/字体面板待补 |
| 2026-11-23 | 阶段 2 输入：`GestureEvent`/`ScrollPhase`/`onGesture`，Cocoa magnify/rotate/swipe 与 scroll phase/momentum 映射 | 基础完成，Force Touch/pen 与基于 phase 的识别器待补 |
| 2026-11-23 | 阶段 2 SF Symbols：`ui/SystemImage.h`、NSImage symbol 光栅化、非 Apple fallback、native smoke 40x40 验证 | 基础完成，tinting/accent color 待补 |
| 2026-11-23 | 阶段 3 渲染：Cocoa `supportsRetainedFrames`/`presentRetained`/`supportsDamageRects`/`presentDamage`；native smoke 验证 retained replay 与 damage snapshot | retained/damage 完成，frame-plan/boundary 待补 |
| 2026-11-23 | 阶段 3 occlusion：Cocoa occlusionState → platformOcclusionChanged；自动 tick 在隐藏时保留待渲染请求 | 基础完成，手工隐藏/最小化验收待补 |
| 2026-11-23 | 阶段 3 vblank：CoralGraphics CVDisplayLink `MacVBlankSource` + `NativeDisplayKind::MacOS`；Cocoa 返回真实 display source，native smoke 校验 refreshHertz > 0 | 基础完成，CVDisplayLink 弃用/per-window 显示切换待补 |
| 2026-11-23 | 阶段 3 HDR 基础：NSScreen `maximumExtendedDynamicRangeColorComponentValue`/Display P3 colour space → `Window::displayHeadroom`/`supportsWideColor` | 上报完成，实际 EDR/PQ 输出待 CoralGraphics |
| 2026-11-23 | 阶段 3 frame-plan：Cocoa boundary submission、WindowSurface 复用 open frame；native smoke plan items=1/buffers=1 | 基础完成，opt-in 与更多边界场景待补 |

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
- `CoralAppDelegate` 已转发 secure restorable state、last-window、open files/open URLs、reopen 和 termination veto；handler 是进程级（AppKit 只有一个 application 对象），多 `Application` 实例会共享。
- Cmd+Q 现在会先检查所有 Cocoa 窗口的 close interceptor：任一 veto 返回 `NSTerminateCancel`；全部允许则请求关闭并返回 `NSTerminateLater`，最后一个 `CocoaPlatform` 析构时 `replyToApplicationShouldTerminate:YES`。自动化只覆盖 veto 路径，完整终止路径会结束测试进程，需手工验收。
- IME：`setMarkedText` 忽略 marked text 内部的 selection 和 replacementRange；`selectedRange` 只做 UTF-8→UTF-16 的 caret/selection 映射；尚未验证真实中文/日文/韩文候选窗和死键。CJK 输入法需要在真实前台会话中手工测试。
- IME 的 `validAttributesForMarkedText` 只声明了下划线，marked text 仍由 CoralUI 的 composition 绘制；没有使用 AppKit 的 attributed marked text 渲染。
- 拖放 destination 已实现；source 支持 text/file URL/custom MIME，但 file promise、拖拽取消反馈和真实 Finder 多文件拖出仍需手工验收。
- AX 目前是查询时重建整棵 `NSAccessibilityElement` 树；没有稳定的 element 身份。文本字段映射 editable/value/selected range 并支持 AXSetValue；表格容器映射 row/column count 与 index range，grid 子节点映射 AXCell，ListItem 映射 AXRow，并实现了 `accessibilityHitTest:` 与 focused-element 通知。VoiceOver 实测仍待补。
- AX bounds 通过父元素逻辑偏移累加得到屏幕 frame；需要确认多层嵌套/Flex 布局下的偏移和 Retina 取整。
- 文件对话框 sheet 的 parent 通过当前 `NSApp.keyWindow`/`mainWindow` 解析，`FileDialogOptions.parent` 还没有直接映射到对应 `NSWindow`；非活动 app 会回退 `runModal`。`NSOpenPanel.canChooseDirectories` 仍固定 false，需要扩展 `FileDialogOptions`。
- `NativeAlert` 的 parent 同样通过当前 key/main window 解析；真实 sheet 的按钮返回/Esc 行为仍需前台手工验收。非 macOS 平台目前返回 -1，继续使用 in-window `Dialog`。
- 拖放 source 的拖拽图像对文件使用 `NSWorkspace iconForFile`，文本使用自绘标签；file promise、拖拽取消反馈、真实 Finder 多文件拖出尚未验收。
- 代码签名钩子只在显式传入 `CORAL_MACOS_CODE_SIGN_IDENTITY` 时执行；默认不签名。ad-hoc 签名已验证，Developer ID 证书与 notarization 需要真实账号。
- universal 不能一次 configure：CoralGraphics 的 vendored libjpeg-turbo 拒绝多架构；当前用 arm64/x86_64 两个 slice，x86 slice 复用 arm64 的 `shader_compiler`，再用 `lipo` 合并 demo 可执行文件。框架作为静态库随 app 链接；如果分发独立 dylib，需要为每个 slice 单独 lipo。
- `scripts/notarize-macos-app.sh` 需要已签名的 Developer ID `.app` 和 notarytool keychain profile；本机没有凭据，只做了脚本语法检查。
- `scripts/check-macos.sh` 是本地 CI 入口，依赖 sibling CoralCore/CoralGraphics；`.github/workflows/macos.yml` 使用 `macos-15`（macOS 14 GitHub runner 已于 2026-11-02 退役），显式 checkout CoralCore/CoralGraphics 的 `main` 并传入 `CORAL_CORE_DIR`/`CORAL_GRAPHICS_DIR`（`.gitmodules` 中这些子模块的 `update = none` 不会被 recursive checkout 拉取），同时把 GoogleTest checkout 到 `third_party/libgtest`。后续仍建议把外部依赖 pin 成 submodule commit 或 tag。
- Security-scoped bookmark 只有在 App Sandbox entitlement 下才有实际意义；当前未 sandbox 的 native smoke 中，`NSURLBookmarkCreationWithSecurityScope` 对 `/tmp` 按预期失败。需要用 `CoralUIDemo.entitlements` 签名后再验证创建/恢复。
- 窗口 autosave/tabbing 目前是 AppKit 原生基础：`frameAutosaveName` 恢复 frame，`TabbingMode` 设置 `NSWindow.tabbingMode`；还没有 Apple 的 state restoration（`restorationClass`/`encodeRestorableState`），也没有 tab 内容/新 tab 的 Window API。
- Native Toolbar 目前是基础 `NSToolbar`（图标+标签、无自定义/overflow 控制），工具栏显示/隐藏会改变内容区高度。
- Native Sidebar 使用 `NSSplitViewController` + `NSVisualEffectView` + `NSTableView`；目前是单列、单选、有 section header，但无 outline/拖拽/可编辑。内容视图现在嵌在 split view controller 中，测试和客户代码不应假设 `NSWindow.contentView` 就是 CoralUI 的 Metal view。
- Toolbar 已允许用户自定义/自动保存，但没有显式 toolbar item 分组、flexible space、overflow 策略或 fullscreen 行为测试。
- Titlebar accessory 目前固定为 trailing 的 `NSSegmentedControl`，API 只表达 label/symbol 段；尚未支持任意 CoralUI/NSView accessory、leading 布局或 accessory 的尺寸策略。
- Secure event input 是进程级、由 OS 引用计数；`Window` 只平衡自己的一次 enable/disable，多窗口/异常退出时需要产品侧确保最终释放。只在 AppKit 主线程上生效。
- 键盘输入源变化通过 TIS distributed notification 观察，回调 dispatch 到主队列；输入源 ID/name 已暴露，但键盘布局映射（物理 key → 字符）仍由 NSEvent characters/IME 处理，死键没有单独测试。
- Spell check 现在有查询 API、TextInput 红色下划线和动态右键替换建议；每次文本变化/右键同步调用 `NSSpellChecker`，长文档可能阻塞 UI，后续应做后台/去抖检查。autocorrect、字体面板、上下文菜单键盘导航的完整验收尚未完成。
- 手势路由按 pointer 命中层向父链冒泡；没有独立的 gesture recognizer/冲突仲裁，也没有 Force Touch、pen pressure 或 swipe 的完整方向/阶段语义。ScrollView 目前只消费 delta，phase 字段供上层控件使用。
- SF Symbol 目前以模板黑色/alpha 图像返回，尚未暴露颜色/tint 配置和动态 accent color；SF Symbol 的 symbol configuration（weight/scale/variant）只暴露 pointSize 与 raster scale。
- P3/EDR/HDR 目前只有 `NSScreen` 能力上报：测试机报告 headroom=1.00、wideColor=0（SDR），因此真实 EDR 输出没有被执行验证。CoralGraphics 仍只呈现 SDR sRGB；要真正输出 P3/PQ，需要 CAMetalLayer 10-bit/EDR 配置和 PQ/colour-space 转换阶段。
- macOS vblank 使用 CoralGraphics 的 `CVDisplayLink` 源（CoralGraphics 分支 `feature/metal-backend`，commit `e4be56e` + `20c1afa`）；plan multi-list 还需要 `b6bb18c`（复用已开启 frame）。`CVDisplayLink` 在新 SDK 已弃用，后续应迁移到 `CADisplayLink`/`CAMetalDisplayLink`；当前 link 基于 active displays，不是 per-window，跨显示器/动态刷新率切换还未处理。首次对齐会等待两个 vblank（约 33ms）。
- 当前 macOS 测试机 `Window::refreshHertz()` 报告约 74 Hz（真实 display 回调间隔），不是固定的 60/120；UI 动画和 ProMotion 调度应以该值为准。

- 全量 UI target + blit 每帧；Cocoa 已接入 retained/damage 和 opt-in frame-plan/boundary submission，但 plan 仍只在 `CORAL_UI_FRAME_PLAN_WINDOW=1` 下启用，复杂页面/滚动 viewport 的边界场景还需更多验证。
- occlusion 暂停逻辑没有自动化测试：需要真实 AppKit 前台会话通过隐藏/最小化窗口验证 frameCount 不再增长，并在恢复时重绘。`Window::renderFrame()` 显式调用仍会渲染，以便 snapshot/导出。
- `screenBounds`/`setPosition` 对副屏 origin 处理不完整。
- 文件对话框已使用 UTType（macOS 11+，旧系统 fallback `allowedFileTypes`）；`canChooseDirectories` 仍未暴露。
- 剪贴板无 NSPasteboard 推送通知，只有 changeCount 轮询。
- OpenGL 窗口路径未接；仅 Metal。
- `.app` 打包、ad-hoc/identity 签名钩子、universal lipo 脚本和 notarization 脚本已有；Developer ID 真实公证与 sandbox 实测仍待完成。


## 6. 当前交付快照（2026-11-23）

已完成并可回归：

- `NSTextInputClient` IME、marked text/commit、候选锚点；
- NSAccessibility 树、文本值/编辑、grid 行列、hit test、focused-element 通知；
- 原生菜单、open/reopen/quit 生命周期、quit 尊重 close interceptor；
- 外部拖放 destination + text/file source；
- 文件对话框 sheet + UTType；`NSAlert`；原生 toolbar/sidebar/titlebar accessory；
- secure input、输入源查询/通知、`NSSpellChecker` 下划线与右键替换建议；
- frame autosave/tabbing、SF Symbols、retained/damage、opt-in frame-plan、occlusion、CVDisplayLink vblank；
- EDR headroom/wide-colour 上报；
- ad-hoc 签名、universal 双 slice、notarization 脚本、本地 CI 和 GitHub Actions workflow。

仍需真实环境/凭据/更多回归：

- VoiceOver、真实 CJK 候选窗、真实 Finder 多文件拖出、真实 sheet/alert 交互；
- sandbox 签名下的 security-scoped bookmark；
- Developer ID + notarytool 公证；
- CoralGraphics 的 P3/EDR/PQ 输出；
- CVDisplayLink → CADisplayLink/CAMetalDisplayLink 迁移；
- frame-plan 默认启用前的滚动/裁剪/overlay 回归；
- sidebar outline/拖拽、任意 titlebar accessory、vibrancy、autocorrect/字体面板。

跨仓库依赖：CoralGraphics 的 `e4be56e`、`20c1afa`、`b6bb18c` 已合入远端 `main`；CoralUI 的 CI/workflow 使用该 `main`。之后可以再 pin 到 submodule commit 或 tag。
## 7. 验收与回归

每阶段至少保持：

```sh
cmake --build out/examples -j8
ctest --test-dir out/examples -L examples --output-on-failure
ctest --test-dir out/tests -R "^test_ui$|^test_ui_shared_device$|^ui_native_smoke$|^ui_native_ax_smoke$" --output-on-failure
```

新增能力要补对应的 native smoke 或手工验收步骤，并记录到本文档。
