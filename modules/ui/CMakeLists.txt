cmake_minimum_required(VERSION 3.19)
set(TARGET ui)
set(PUBLIC_HDR_DIR include)

set(PUBLIC_HDRS
    include/ui/Accessibility.h
    include/ui/AccessibilityAtSpi.h
    include/ui/Animation.h
    include/ui/Curve.h
    include/ui/DateFormat.h
    include/ui/Binding.h
    include/ui/Button.h
    include/ui/Canvas.h
    include/ui/CanvasBackend.h
    include/ui/CheckBox.h
    include/ui/Clipboard.h
    include/ui/ClipboardMime.h
    include/ui/Command.h
    include/ui/CrashReporter.h
    include/ui/ElementRegistry.h
    include/ui/ElementSpec.h
    include/ui/Component.h
    include/ui/Element.h
    include/ui/ElementHost.h
    include/ui/FrameStatsOverlay.h
    include/ui/Reconciler.h
    include/ui/PlatformClock.h
    include/ui/RecyclingListView.h
    include/ui/ContentFrame.h
    include/ui/ContentFrameBackend.h
    include/ui/ContentHost.h
    include/ui/ContentView.h
    include/ui/Application.h
    include/ui/CandidateListView.h
    include/ui/ColorButton.h
    include/ui/ColorDialog.h
    include/ui/ColorInput.h
    include/ui/ColorPicker.h
    include/ui/ColorLayer.h
    include/ui/Dialog.h
    include/ui/DockView.h
    include/ui/DragDrop.h
    include/ui/Event.h
    include/ui/FileDialog.h
    include/ui/Flex.h
    include/ui/Geometry.h
    include/ui/Grid.h
    include/ui/HotReload.h
    include/ui/host/FramePacket.h
    include/ui/host/PreviewScheduler.h
    include/ui/host/RenderHost.h
    include/ui/mvvm/Binding.h
    include/ui/mvvm/DataContext.h
    include/ui/mvvm/Dispatcher.h
    include/ui/host/ThreadAffinity.h
    include/ui/host/ThreadedRenderHost.h
    include/ui/IconButton.h
    include/ui/IconTheme.h
    include/ui/FramePlan.h
    include/ui/Layer.h
    include/ui/LineBreakPolicy.h
    include/ui/ListView.h
    include/ui/Menu.h
    include/ui/MenuBar.h
    include/ui/NumberField.h
    include/ui/Overlay.h
    include/ui/Popup.h
    include/ui/ProcessStats.h
    include/ui/Ref.h
    include/ui/Positioner.h
    include/ui/ProgressBar.h
    include/ui/RadioButton.h
    include/ui/RenderContext.h
    include/ui/RasterRenderer.h
    include/ui/Renderer.h
    include/ui/RendererCapabilities.h
    include/ui/RendererDevice.h
    include/ui/Types.h
    include/ui/WindowBackend.h
    include/ui/RichText.h
    include/ui/RichTextLayer.h
    include/ui/ScrollView.h
    include/ui/SearchField.h
    include/ui/SegmentedControl.h
    include/ui/Slider.h
    include/ui/SplitView.h
    include/ui/State.h
    include/ui/Style.h
    include/ui/TabBar.h
    include/ui/TextInput.h
    include/ui/TitleBar.h
    include/ui/TextLayer.h
    include/ui/TextBidi.h
    include/ui/TextLayout.h
    include/ui/TextRendererStats.h
    include/ui/SystemFonts.h
    include/ui/Motion.h
    include/ui/NumberFormat.h
    include/ui/Spinner.h
    include/ui/Table.h
    include/ui/TableView.h
    include/ui/SystemAppearance.h
    include/ui/Theme.h
    include/ui/ThemeFile.h
    include/ui/ThemeHotReload.h
    include/ui/Translation.h
    include/ui/Undo.h
    include/ui/Toast.h
    include/ui/Toggle.h
    include/ui/UI.h
    include/ui/Tooltip.h
    include/ui/TreeView.h
    include/ui/UiExecutor.h
    include/ui/Win32DragOut.h
    include/ui/Window.h
    include/ui/WindowChrome.h
    include/ui/WindowManager.h
)

set(SRCS
    src/Accessibility.cpp
    src/AccessibilityAtSpi.cpp
    src/Animation.cpp
    src/Curve.cpp
    src/DateFormat.cpp
    src/Application.cpp
    src/Binding.cpp
    src/Button.cpp
    src/CandidateListView.cpp
    src/CheckBox.cpp
    src/Clipboard.cpp
    src/ClipboardImage.cpp
    src/ClipboardMime.cpp
    src/Command.cpp
    src/CrashReporter.cpp
    src/ElementRegistry.cpp
    src/ElementSpec.cpp
    src/Component.cpp
    src/Canvas.cpp
    src/ContentFrame.cpp
    src/ContentHost.cpp
    src/ContentView.cpp
    src/ColorButton.cpp
    src/ColorDialog.cpp
    src/ColorInput.cpp
    src/ColorPicker.cpp
    src/ColorLayer.cpp
    src/Dialog.cpp
    src/DockView.cpp
    src/DragDrop.cpp
    src/FileDialog.cpp
    src/Flex.cpp
    src/FrameStatsOverlay.cpp
    src/Grid.cpp
    src/HotReload.cpp
    src/IconButton.cpp
    src/IconTheme.cpp
    src/FramePlan.cpp
    src/Layer.cpp
    src/LineBreakPolicy.cpp
    src/DictionaryBackends.cpp
    src/ListView.cpp
    src/Menu.cpp
    src/MenuBar.cpp
    src/NumberField.cpp
    src/Overlay.cpp
    src/Popup.cpp
    src/ProcessStats.cpp
    src/Positioner.cpp
    src/ProgressBar.cpp
    src/RadioButton.cpp
    src/Reconciler.cpp
    src/PlatformClock.cpp
    src/RecyclingListView.cpp
    src/RenderContext.cpp
    src/RasterRenderer.cpp
    src/RendererCapabilities.cpp
    src/RendererDevice.cpp
    src/RichText.cpp
    src/RichTextLayoutCache.cpp
    src/RichTextLayer.cpp
    src/ScrollView.cpp
    src/SearchField.cpp
    src/SegmentedControl.cpp
    src/Slider.cpp
    src/SplitView.cpp
    src/Style.cpp
    src/SystemAppearance.cpp
    src/SystemLocale.cpp
    src/TabBar.cpp
    src/Motion.cpp
    src/NumberFormat.cpp
    src/Spinner.cpp
    src/Table.cpp
    src/TableView.cpp
    src/TextInput.cpp
    src/TitleBar.cpp
    src/ThemeFile.cpp
    src/ThemeHotReload.cpp
    src/Translation.cpp
    src/TextLayer.cpp
    src/TextBidi.cpp
    src/TextLayout.cpp
    src/TextLayoutCache.cpp
    src/TextRendererStats.cpp
    src/ShapedAdvances.cpp
    src/TextShapeCache.cpp
    src/SystemFonts.cpp
    src/unicode/BidiProperties.generated.cpp
    src/unicode/BidiResolver.cpp
    src/unicode/GraphemeBreak.cpp
    src/unicode/GraphemeBreakProperties.generated.cpp
    src/unicode/Hyphenation.cpp
    src/unicode/HyphenationPatterns.generated.cpp
    src/unicode/LineBreakIterator.cpp
    src/unicode/LineBreakProperties.generated.cpp
    src/unicode/WordBreak.cpp
    src/unicode/WordBreakProperties.generated.cpp
    src/unicode/WordTrie.cpp
    src/unicode/WordTries.generated.cpp
    src/Toast.cpp
    src/Toggle.cpp
    src/Tooltip.cpp
    src/Undo.cpp
    src/TreeView.cpp
    src/Window.cpp
    src/WindowChrome.cpp
    src/WindowManager.cpp
    src/host/ThreadedRenderHost.cpp
    src/platform/WindowPlatform.cpp
    src/platform/WindowHeadless.cpp
    src/platform/WindowRaster.cpp
)

set(CLIPBOARD_PLATFORM_SOURCE src/platform/ClipboardPlatformFallback.cpp)
set(FILEDIALOG_PLATFORM_SOURCE src/platform/FileDialogPlatformFallback.cpp)

include_directories(${PUBLIC_HDR_DIR})
include_directories(src)

add_library(${TARGET} STATIC ${PUBLIC_HDRS} ${SRCS})
set(_coral_public_include "${PUBLIC_HDR_DIR}")
if (NOT IS_ABSOLUTE "${_coral_public_include}")
  set(_coral_public_include "${CMAKE_CURRENT_SOURCE_DIR}/${_coral_public_include}")
endif ()
target_include_directories(${TARGET} PUBLIC $<BUILD_INTERFACE:${_coral_public_include}> $<INSTALL_INTERFACE:include>)
set_target_properties(${TARGET} PROPERTIES FOLDER Libs)

target_link_libraries(${TARGET} PUBLIC graphics observer event)
target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:minilog>)

if (WIN32)
  # psapi/dbghelp: process counters and the minidump writer behind `ui/CrashReporter.h`.
  target_link_libraries(${TARGET} PRIVATE user32 imm32 ole32 shell32 comdlg32 advapi32 psapi dbghelp
      uiautomationcore)
  target_sources(${TARGET} PRIVATE src/platform/win32/WindowWin32.cpp
      src/Win32DragOut.cpp)
  target_sources(${TARGET} PRIVATE src/platform/win32/DropTargetWin32.cpp
      src/platform/win32/UiaProvider.cpp)
  set(CLIPBOARD_PLATFORM_SOURCE src/platform/win32/ClipboardWin32.cpp)
  set(FILEDIALOG_PLATFORM_SOURCE src/platform/win32/FileDialogWin32.cpp)
endif ()

if (APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # The Cocoa window: one NSWindow whose content view is backed by a CAMetalLayer, handed to
  # graphics::gpu::WindowSurface. AppKit's event queue is drained from the framework's own run loop,
  # so the backend never calls `[NSApp run]`.
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
  find_library(UNIFORMTYPEIDENTIFIERS_FRAMEWORK UniformTypeIdentifiers REQUIRED)
  target_sources(${TARGET} PRIVATE src/platform/darwin/WindowCocoa.mm
      src/platform/darwin/ClipboardCocoa.mm src/platform/darwin/FileDialogCocoa.mm)
  set_source_files_properties(src/platform/darwin/WindowCocoa.mm
      src/platform/darwin/ClipboardCocoa.mm src/platform/darwin/FileDialogCocoa.mm
      PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:${COCOA_FRAMEWORK}>
      $<BUILD_INTERFACE:${METAL_FRAMEWORK}> $<BUILD_INTERFACE:${QUARTZCORE_FRAMEWORK}>
      $<BUILD_INTERFACE:${UNIFORMTYPEIDENTIFIERS_FRAMEWORK}>)
  target_compile_definitions(${TARGET} PRIVATE CORAL_UI_COCOA=1)
  set(CLIPBOARD_PLATFORM_SOURCE src/platform/darwin/ClipboardCocoa.mm)
  set(FILEDIALOG_PLATFORM_SOURCE src/platform/darwin/FileDialogCocoa.mm)
endif ()

if (UNIX AND NOT APPLE)
  set(CLIPBOARD_PLATFORM_SOURCE src/platform/linux/ClipboardLinux.cpp)
  set(FILEDIALOG_PLATFORM_SOURCE src/platform/linux/FileDialogLinux.cpp)
  target_sources(${TARGET} PRIVATE src/platform/linux/LinuxAppearance.cpp)
  target_sources(${TARGET} PRIVATE src/platform/linux/FileDialogPortal.cpp)
  option(CORAL_UI_FORCE_WAYLAND "Build the UI window against Wayland only (test/dev)" OFF)
  if (NOT CORAL_UI_FORCE_WAYLAND)
    find_package(X11 QUIET)
    if (X11_FOUND)
      target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:X11::X11>)
      target_compile_definitions(${TARGET} PRIVATE CORAL_UI_X11=1)
      # XFixes turns the clipboard owner into a subscriber of selection-owner changes, so a copy from
      # another process is a push notification instead of owner polling. Optional: without the
      # extension the backend falls back to polling and the build stays identical.
      if (TARGET X11::Xfixes)
        target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:X11::Xfixes>)
        target_compile_definitions(${TARGET} PRIVATE CORAL_UI_HAS_XFIXES=1)
      endif ()
      find_package(PkgConfig QUIET)
      pkg_check_modules(CORAL_DBUS QUIET IMPORTED_TARGET dbus-1)
      if (CORAL_DBUS_FOUND)
        target_sources(${TARGET} PRIVATE src/platform/linux/AtspiBridge.cpp)
        target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:PkgConfig::CORAL_DBUS>)
        target_compile_definitions(${TARGET} PRIVATE CORAL_UI_HAS_ATSPI=1)
      endif ()
      target_sources(${TARGET} PRIVATE src/platform/linux/WindowX11.cpp
          src/platform/linux/ImeBackendXim.cpp
          src/platform/linux/ClipboardX11.cpp
          src/platform/linux/XdndSource.cpp)
      find_package(PkgConfig QUIET)
      if (PkgConfig_FOUND)
        pkg_check_modules(IBUS QUIET IMPORTED_TARGET ibus-1.0)
        if (IBUS_FOUND)
          target_sources(${TARGET} PRIVATE src/platform/linux/ImeBackendIBus.cpp)
          target_link_directories(${TARGET} PUBLIC ${IBUS_LIBRARY_DIRS})
          target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:PkgConfig::IBUS>)
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_HAS_IBUS=1)
        endif ()
      endif ()
    endif ()
  endif ()

  # Native Wayland: the client library, xkbcommon for key translation and the xdg-shell protocol
  # generated from the system wayland-protocols package.
  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
    # The UI owns the wl_surface; EGL needs a wl_egl_window wrapping it. Without this a native
    # Wayland window cannot create a GPU surface at all and every run falls back to XWayland.
    pkg_check_modules(WAYLAND_EGL QUIET IMPORTED_TARGET wayland-egl)
    pkg_check_modules(WAYLAND_CURSOR QUIET IMPORTED_TARGET wayland-cursor)
    pkg_check_modules(XKBCOMMON QUIET IMPORTED_TARGET xkbcommon)
    find_program(WAYLAND_SCANNER wayland-scanner)
    # Use the pkg-config executable CMake resolved, not a bare `pkg-config` from PATH: a sysroot or
    # cross build may only have the former.
    execute_process(COMMAND "${PKG_CONFIG_EXECUTABLE}" --variable=pkgdatadir wayland-protocols
        OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # `--variable=pkgdatadir` is not prefixed by pkg-config's sysroot; prepend it when the path only
    # exists inside the sysroot. Without this, a sysroot build silently dropped Wayland entirely.
    if (DEFINED ENV{PKG_CONFIG_SYSROOT_DIR} AND NOT EXISTS
        "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" AND
        EXISTS "$ENV{PKG_CONFIG_SYSROOT_DIR}${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
      set(WAYLAND_PROTOCOLS_DIR "$ENV{PKG_CONFIG_SYSROOT_DIR}${WAYLAND_PROTOCOLS_DIR}")
    endif ()
    if (WAYLAND_CLIENT_FOUND AND WAYLAND_EGL_FOUND AND XKBCOMMON_FOUND AND WAYLAND_SCANNER AND WAYLAND_PROTOCOLS_DIR)
      set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
      if (EXISTS "${XDG_SHELL_XML}")
        set(WAYLAND_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/wayland-generated")
        file(MAKE_DIRECTORY "${WAYLAND_GENERATED_DIR}")
        set(WAYLAND_HEADER "${WAYLAND_GENERATED_DIR}/xdg-shell-client-protocol.h")
        set(WAYLAND_CODE "${WAYLAND_GENERATED_DIR}/xdg-shell-protocol.c")
        add_custom_command(OUTPUT "${WAYLAND_HEADER}"
            COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_SHELL_XML}" "${WAYLAND_HEADER}"
            DEPENDS "${XDG_SHELL_XML}")
        add_custom_command(OUTPUT "${WAYLAND_CODE}"
            COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_SHELL_XML}" "${WAYLAND_CODE}"
            DEPENDS "${XDG_SHELL_XML}")
        set(FRACTIONAL_SCALE_XML
            "${WAYLAND_PROTOCOLS_DIR}/staging/fractional-scale/fractional-scale-v1.xml")
        set(FRACTIONAL_SCALE_HEADER
            "${WAYLAND_GENERATED_DIR}/fractional-scale-v1-client-protocol.h")
        set(FRACTIONAL_SCALE_CODE
            "${WAYLAND_GENERATED_DIR}/fractional-scale-v1-protocol.c")
        if (EXISTS "${FRACTIONAL_SCALE_XML}")
          add_custom_command(OUTPUT "${FRACTIONAL_SCALE_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${FRACTIONAL_SCALE_XML}"
              "${FRACTIONAL_SCALE_HEADER}" DEPENDS "${FRACTIONAL_SCALE_XML}")
          add_custom_command(OUTPUT "${FRACTIONAL_SCALE_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${FRACTIONAL_SCALE_XML}"
              "${FRACTIONAL_SCALE_CODE}" DEPENDS "${FRACTIONAL_SCALE_XML}")
        endif ()
        set(VIEWPORTER_XML "${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml")
        set(VIEWPORTER_HEADER "${WAYLAND_GENERATED_DIR}/viewporter-client-protocol.h")
        set(VIEWPORTER_CODE "${WAYLAND_GENERATED_DIR}/viewporter-protocol.c")
        if (EXISTS "${VIEWPORTER_XML}")
          add_custom_command(OUTPUT "${VIEWPORTER_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${VIEWPORTER_XML}"
              "${VIEWPORTER_HEADER}" DEPENDS "${VIEWPORTER_XML}")
          add_custom_command(OUTPUT "${VIEWPORTER_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${VIEWPORTER_XML}"
              "${VIEWPORTER_CODE}" DEPENDS "${VIEWPORTER_XML}")
        endif ()
        set(TEXT_INPUT_XML "${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v1.xml")
        set(TEXT_INPUT_HEADER "${WAYLAND_GENERATED_DIR}/text-input-unstable-v1-client-protocol.h")
        set(TEXT_INPUT_CODE "${WAYLAND_GENERATED_DIR}/text-input-unstable-v1-protocol.c")
        if (EXISTS "${TEXT_INPUT_XML}")
          add_custom_command(OUTPUT "${TEXT_INPUT_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${TEXT_INPUT_XML}" "${TEXT_INPUT_HEADER}"
              DEPENDS "${TEXT_INPUT_XML}")
          add_custom_command(OUTPUT "${TEXT_INPUT_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${TEXT_INPUT_XML}" "${TEXT_INPUT_CODE}"
              DEPENDS "${TEXT_INPUT_XML}")
        endif ()
        set(TEXT_INPUT_V3_XML "${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v3.xml")
        set(TEXT_INPUT_V3_HEADER "${WAYLAND_GENERATED_DIR}/text-input-unstable-v3-client-protocol.h")
        set(TEXT_INPUT_V3_CODE "${WAYLAND_GENERATED_DIR}/text-input-unstable-v3-protocol.c")
        if (EXISTS "${TEXT_INPUT_V3_XML}")
          add_custom_command(OUTPUT "${TEXT_INPUT_V3_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${TEXT_INPUT_V3_XML}" "${TEXT_INPUT_V3_HEADER}"
              DEPENDS "${TEXT_INPUT_V3_XML}")
          add_custom_command(OUTPUT "${TEXT_INPUT_V3_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${TEXT_INPUT_V3_XML}" "${TEXT_INPUT_V3_CODE}"
              DEPENDS "${TEXT_INPUT_V3_XML}")
        endif ()
        enable_language(C)
        set_source_files_properties("${WAYLAND_CODE}" PROPERTIES LANGUAGE C)
        if (EXISTS "${FRACTIONAL_SCALE_XML}")
          set_source_files_properties("${FRACTIONAL_SCALE_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${FRACTIONAL_SCALE_HEADER}"
              "${FRACTIONAL_SCALE_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_FRACTIONAL_SCALE=1)
        endif ()
        if (EXISTS "${VIEWPORTER_XML}")
          set_source_files_properties("${VIEWPORTER_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${VIEWPORTER_HEADER}" "${VIEWPORTER_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_VIEWPORTER=1)
        endif ()
        if (EXISTS "${TEXT_INPUT_XML}")
          set_source_files_properties("${TEXT_INPUT_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${TEXT_INPUT_HEADER}" "${TEXT_INPUT_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_TEXT_INPUT=1)
        endif ()
        if (EXISTS "${TEXT_INPUT_V3_XML}")
          set_source_files_properties("${TEXT_INPUT_V3_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${TEXT_INPUT_V3_HEADER}" "${TEXT_INPUT_V3_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_TEXT_INPUT_V3=1)
        endif ()
        # PRIMARY selection ("select to copy"): optional, because not every compositor implements it.
        # Alpha modifier: Wayland's per-surface opacity, used by `Window::setOpacity`.
        set(ALPHA_MODIFIER_XML
            "${WAYLAND_PROTOCOLS_DIR}/staging/alpha-modifier/alpha-modifier-v1.xml")
        set(ALPHA_MODIFIER_HEADER
            "${WAYLAND_GENERATED_DIR}/alpha-modifier-v1-client-protocol.h")
        set(ALPHA_MODIFIER_CODE
            "${WAYLAND_GENERATED_DIR}/alpha-modifier-v1-protocol.c")
        if (EXISTS "${ALPHA_MODIFIER_XML}")
          add_custom_command(OUTPUT "${ALPHA_MODIFIER_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${ALPHA_MODIFIER_XML}"
              "${ALPHA_MODIFIER_HEADER}" DEPENDS "${ALPHA_MODIFIER_XML}")
          add_custom_command(OUTPUT "${ALPHA_MODIFIER_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${ALPHA_MODIFIER_XML}"
              "${ALPHA_MODIFIER_CODE}" DEPENDS "${ALPHA_MODIFIER_XML}")
          set_source_files_properties("${ALPHA_MODIFIER_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${ALPHA_MODIFIER_HEADER}" "${ALPHA_MODIFIER_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_ALPHA_MODIFIER=1)
        endif ()
        # xdg-foreign: exports a toplevel handle a portal file dialog can use as `parent_window`.
        set(XDG_FOREIGN_XML
            "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml")
        set(XDG_FOREIGN_HEADER
            "${WAYLAND_GENERATED_DIR}/xdg-foreign-unstable-v2-client-protocol.h")
        set(XDG_FOREIGN_CODE
            "${WAYLAND_GENERATED_DIR}/xdg-foreign-unstable-v2-protocol.c")
        if (EXISTS "${XDG_FOREIGN_XML}")
          add_custom_command(OUTPUT "${XDG_FOREIGN_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_FOREIGN_XML}"
              "${XDG_FOREIGN_HEADER}" DEPENDS "${XDG_FOREIGN_XML}")
          add_custom_command(OUTPUT "${XDG_FOREIGN_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_FOREIGN_XML}"
              "${XDG_FOREIGN_CODE}" DEPENDS "${XDG_FOREIGN_XML}")
          set_source_files_properties("${XDG_FOREIGN_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${XDG_FOREIGN_HEADER}" "${XDG_FOREIGN_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_XDG_FOREIGN=1)
        endif ()
        # Server-side decorations: KDE and wlroots compositors implement this; GNOME does not, so a
        # client that wants a frame there still draws its own chrome.
        set(XDG_DECORATION_XML
            "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml")
        set(XDG_DECORATION_HEADER
            "${WAYLAND_GENERATED_DIR}/xdg-decoration-unstable-v1-client-protocol.h")
        set(XDG_DECORATION_CODE
            "${WAYLAND_GENERATED_DIR}/xdg-decoration-unstable-v1-protocol.c")
        if (EXISTS "${XDG_DECORATION_XML}")
          add_custom_command(OUTPUT "${XDG_DECORATION_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_DECORATION_XML}"
              "${XDG_DECORATION_HEADER}" DEPENDS "${XDG_DECORATION_XML}")
          add_custom_command(OUTPUT "${XDG_DECORATION_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_DECORATION_XML}"
              "${XDG_DECORATION_CODE}" DEPENDS "${XDG_DECORATION_XML}")
          set_source_files_properties("${XDG_DECORATION_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${XDG_DECORATION_HEADER}" "${XDG_DECORATION_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_XDG_DECORATION=1)
        endif ()
        # Cursor shapes: the compositor draws the theme's cursor, so the app does not need a cursor
        # theme dependency and still gets the right I-beam/resize/move shapes.
        set(CURSOR_SHAPE_XML
            "${WAYLAND_PROTOCOLS_DIR}/staging/cursor-shape/cursor-shape-v1.xml")
        set(CURSOR_SHAPE_HEADER
            "${WAYLAND_GENERATED_DIR}/cursor-shape-v1-client-protocol.h")
        set(CURSOR_SHAPE_CODE
            "${WAYLAND_GENERATED_DIR}/cursor-shape-v1-protocol.c")
        if (EXISTS "${CURSOR_SHAPE_XML}")
          add_custom_command(OUTPUT "${CURSOR_SHAPE_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${CURSOR_SHAPE_XML}"
              "${CURSOR_SHAPE_HEADER}" DEPENDS "${CURSOR_SHAPE_XML}")
          add_custom_command(OUTPUT "${CURSOR_SHAPE_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${CURSOR_SHAPE_XML}"
              "${CURSOR_SHAPE_CODE}" DEPENDS "${CURSOR_SHAPE_XML}")
          set_source_files_properties("${CURSOR_SHAPE_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${CURSOR_SHAPE_HEADER}" "${CURSOR_SHAPE_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_CURSOR_SHAPE=1)
          # cursor-shape-v1 has a get_tablet_tool_v2 request, so its generated protocol table
          # references the tablet interface even when the app never uses a pen. Compile that
          # interface in as well, or the cursor protocol object fails to link.
          set(TABLET_XML "${WAYLAND_PROTOCOLS_DIR}/unstable/tablet/tablet-unstable-v2.xml")
          set(TABLET_HEADER "${WAYLAND_GENERATED_DIR}/tablet-unstable-v2-client-protocol.h")
          set(TABLET_CODE "${WAYLAND_GENERATED_DIR}/tablet-unstable-v2-protocol.c")
          if (EXISTS "${TABLET_XML}")
            add_custom_command(OUTPUT "${TABLET_HEADER}"
                COMMAND "${WAYLAND_SCANNER}" client-header "${TABLET_XML}"
                "${TABLET_HEADER}" DEPENDS "${TABLET_XML}")
            add_custom_command(OUTPUT "${TABLET_CODE}"
                COMMAND "${WAYLAND_SCANNER}" private-code "${TABLET_XML}"
                "${TABLET_CODE}" DEPENDS "${TABLET_XML}")
            set_source_files_properties("${TABLET_CODE}" PROPERTIES LANGUAGE C)
            target_sources(${TARGET} PRIVATE "${TABLET_HEADER}" "${TABLET_CODE}")
          endif ()
        endif ()
        set(PRIMARY_SELECTION_XML
            "${WAYLAND_PROTOCOLS_DIR}/unstable/primary-selection/primary-selection-unstable-v1.xml")
        set(PRIMARY_SELECTION_HEADER
            "${WAYLAND_GENERATED_DIR}/primary-selection-unstable-v1-client-protocol.h")
        set(PRIMARY_SELECTION_CODE
            "${WAYLAND_GENERATED_DIR}/primary-selection-unstable-v1-protocol.c")
        if (EXISTS "${PRIMARY_SELECTION_XML}")
          add_custom_command(OUTPUT "${PRIMARY_SELECTION_HEADER}"
              COMMAND "${WAYLAND_SCANNER}" client-header "${PRIMARY_SELECTION_XML}"
              "${PRIMARY_SELECTION_HEADER}" DEPENDS "${PRIMARY_SELECTION_XML}")
          add_custom_command(OUTPUT "${PRIMARY_SELECTION_CODE}"
              COMMAND "${WAYLAND_SCANNER}" private-code "${PRIMARY_SELECTION_XML}"
              "${PRIMARY_SELECTION_CODE}" DEPENDS "${PRIMARY_SELECTION_XML}")
          set_source_files_properties("${PRIMARY_SELECTION_CODE}" PROPERTIES LANGUAGE C)
          target_sources(${TARGET} PRIVATE "${PRIMARY_SELECTION_HEADER}" "${PRIMARY_SELECTION_CODE}")
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_PRIMARY_SELECTION=1)
        endif ()
        target_sources(${TARGET} PRIVATE "${WAYLAND_HEADER}" "${WAYLAND_CODE}")
        target_include_directories(${TARGET} PRIVATE "${WAYLAND_GENERATED_DIR}")
        target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND=1)
        target_sources(${TARGET} PRIVATE src/platform/linux/WindowWayland.cpp
            src/platform/linux/ClipboardWayland.cpp)
        target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:PkgConfig::WAYLAND_CLIENT> $<BUILD_INTERFACE:PkgConfig::WAYLAND_EGL> $<BUILD_INTERFACE:PkgConfig::XKBCOMMON>)
        if (WAYLAND_CURSOR_FOUND)
          target_link_directories(${TARGET} PUBLIC ${WAYLAND_CURSOR_LIBRARY_DIRS})
          target_link_libraries(${TARGET} PRIVATE ${WAYLAND_CURSOR_LIBRARIES})
          target_compile_definitions(${TARGET} PRIVATE CORAL_UI_WAYLAND_CURSOR_THEME=1)
        endif ()
      endif ()
    endif ()
  endif ()
endif ()

target_sources(${TARGET} PRIVATE ${CLIPBOARD_PLATFORM_SOURCE} ${FILEDIALOG_PLATFORM_SOURCE})

if (MSVC)
  target_compile_options(${TARGET} PRIVATE /W4 /utf-8)
else ()
  target_compile_options(${TARGET} PRIVATE -Wall -Wextra)
endif ()

install(TARGETS ${TARGET} EXPORT CoralUITargets ARCHIVE DESTINATION lib)

# vendored archives that CoralUI's static libraries need at final link time but whose own third-party
# install options are disabled in this build; install the built file directly.
if (TARGET png)
  # libpng is built as a shared library here, so ship its import library and the DLL it names.
  install(FILES $<TARGET_LINKER_FILE:png> DESTINATION lib)
  install(FILES $<TARGET_FILE:png> DESTINATION bin)
endif ()
# zlib's own directory registers no install rules in this build (its generated cmake_install.cmake is
# empty), so ship the built archive by path; it is what libpng/FreeType reference at final link time.
# Use the target rather than a configure-time exists() check: on a fresh tree z.lib has not been built
# yet, the rule would be skipped, and the packaged png.lib would fail to link against zlib.
if (TARGET z)
  install(FILES $<TARGET_FILE:z> DESTINATION lib)
endif ()
# The vendored shaderc stack: only shaderc.lib has a working install rule of its own, so ship the
# archives it links against. Ask the targets for their files instead of naming build paths with a
# hard-coded debug postfix: `glslangd.lib`/`SPIRVd.lib` only exist in Debug, and the guard based on
# those names silently dropped the whole chain from a Release install (a Release consumer then failed
# to link `shaderc_*`/`glslang_*` symbols). Guarded per target so a Vulkan-less build still configures.
foreach (_target shaderc_util glslang SPIRV SPIRV-Tools SPIRV-Tools-opt)
  if (TARGET ${_target})
    install(FILES $<TARGET_FILE:${_target}> DESTINATION lib)
  endif ()
endforeach ()

install(DIRECTORY ${PUBLIC_HDR_DIR}/${TARGET} DESTINATION include)

# Translation catalogues belong to the SDK, not to the demo: `loadCatalogDirectory()` takes a
# directory, so the installed `share/coral/i18n` is the packaging contract (`CoralUI_I18N_DIR` in the
# package config points at it). Only the catalogues ship -- the rest of `resources/` is development
# material.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/i18n/ DESTINATION share/coral/i18n
    FILES_MATCHING PATTERN "*.catalog")

# find_package(CoralUI): the SDK entry point. The first-party targets are installed as an export set
# (`install(EXPORT CoralUITargets)`); the config adds what an export cannot carry -- the vendored
# archives and the platform/imported libraries those targets link in the build tree -- by appending
# them to the exported targets link interfaces. (see docs/P3_6_PACKAGING.md §3.1).
include(CMakePackageConfigHelpers)
set(CORAL_UI_CMAKE_DIR "lib/cmake/CoralUI")
set(CORAL_UI_LIB_SUBDIR "")
install(EXPORT CoralUITargets
    FILE CoralUITargets.cmake
    NAMESPACE Coral::
    DESTINATION ${CORAL_UI_CMAKE_DIR}
)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/CoralUIConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CoralUIConfig.cmake
    INSTALL_DESTINATION ${CORAL_UI_CMAKE_DIR}
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CoralUIConfigVersion.cmake
    VERSION ${CORAL_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CoralUIConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CoralUIConfigVersion.cmake
    DESTINATION ${CORAL_UI_CMAKE_DIR}
)

if (CORAL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tools)
  add_subdirectory(tests)
  add_subdirectory(benchmarks)
  # The demo and verification programs live in the repository's examples/ project, not here: the
  # framework root adds them only for a standalone configure. An application that embeds this module
  # builds its own products instead.
endif ()



