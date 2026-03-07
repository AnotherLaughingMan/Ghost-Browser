cmake_minimum_required(VERSION 3.21)
project(GhostBrowser VERSION 0.1.1 LANGUAGES CXX)

# ---------- x64 Assembly support ----------
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(MSVC)
        enable_language(ASM_MASM)
    else()
        enable_language(ASM)
    endif()
    set(GHOST_ASM_ENABLED TRUE)
else()
    set(GHOST_ASM_ENABLED FALSE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------- Qt 6.9+ ----------
find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets WebChannel WebEngineCore WebEngineWidgets)

# ---------- Sources ----------
set(GHOST_SOURCES
    src/app/main.cpp
    src/browser/GhostRequestInterceptor.cpp
    src/browser/GhostRequestInterceptor.h
    src/core/asm_fallbacks.cpp
    src/core/asm_fallbacks.h
    src/core/CookieManager.cpp
    src/core/CookieManager.h
    src/core/HistoryManager.cpp
    src/core/HistoryManager.h
    src/core/SettingsManager.cpp
    src/core/SettingsManager.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
)

# ---------- Qt Resources ----------
set(GHOST_RESOURCES
    assets/ghost_icons.qrc
)

# ---------- Assembly hot-path sources (x64 only, per-platform) ----------
# Each assembly routine MUST have a portable C++ fallback.
set(GHOST_ASM_SOURCES "")
if(GHOST_ASM_ENABLED)
    if(MSVC)
        set(GHOST_ASM_SOURCES
            asm/win/memset_u32.asm
            asm/win/crc32_url.asm
        )
    elseif(UNIX AND NOT APPLE)
        set(GHOST_ASM_SOURCES "")   # asm/linux/*.s when added
    elseif(APPLE)
        set(GHOST_ASM_SOURCES "")   # asm/macos/*.s when added
    endif()
endif()

# ---------- JSON config files (install alongside exe) ----------
set(GHOST_CONFIG_FILES
    config/defaults.json
    config/settings-schema.json
)

# ---------- TypeScript compilation (web/ -> assets/js/) ----------
find_program(TSC_EXECUTABLE tsc HINTS "$ENV{APPDATA}/npm")

# ---------- Executable ----------
qt_add_executable(GhostBrowser ${GHOST_SOURCES} ${GHOST_RESOURCES} ${GHOST_ASM_SOURCES})

set_target_properties(GhostBrowser PROPERTIES
    OUTPUT_NAME "Ghost Browser"
)

# ---------- Platform-specific executable properties ----------
if(WIN32)
    set_target_properties(GhostBrowser PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(GhostBrowser PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# ---------- Link ----------
target_link_libraries(GhostBrowser PRIVATE
    Qt6::WebChannel
    Qt6::WebEngineCore
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

# ---------- Include paths ----------
target_include_directories(GhostBrowser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# ---------- TypeScript build (web/ -> assets/js/) ----------
if(TSC_EXECUTABLE)
    add_custom_target(GhostWebBuild ALL
        COMMAND "${TSC_EXECUTABLE}" --project "${CMAKE_SOURCE_DIR}/web/tsconfig.json"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
        COMMENT "Compiling TypeScript (web/) ..."
        SOURCES web/src/settings-bridge.ts web/tsconfig.json
    )
    add_dependencies(GhostBrowser GhostWebBuild)
else()
    message(STATUS "tsc not found — TypeScript compilation skipped. Install via: npm i -g typescript")
endif()

# ---------- Copy JSON config files next to exe ----------
foreach(_cfg ${GHOST_CONFIG_FILES})
    add_custom_command(TARGET GhostBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/${_cfg}"
            "$<TARGET_FILE_DIR:GhostBrowser>/${_cfg}"
    )
endforeach()

# ---------- Platform-specific resources ----------
if(WIN32)
    set(GHOST_RC ${CMAKE_SOURCE_DIR}/assets/ghost.rc)
    if(EXISTS ${GHOST_RC})
        target_sources(GhostBrowser PRIVATE ${GHOST_RC})
    endif()
endif()

# ---------- Deploy Qt runtime libs alongside exe ----------
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET GhostBrowser POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:GhostBrowser>"
            COMMENT "Running windeployqt..."
        )
    endif()

    # Overwrite WebEngine DLLs with custom-codecs build (proprietary codecs)
    set(CUSTOM_CODECS_DIR "C:/Qt/6.9.3-custom-codecs")
    if(IS_DIRECTORY "${CUSTOM_CODECS_DIR}/bin")
        set(_WE_DLLS
            Qt6WebEngineCore    Qt6WebEngineCored
            Qt6WebEngineWidgets Qt6WebEngineWidgetsd
            Qt6Pdf              Qt6Pdfd
            Qt6PdfWidgets       Qt6PdfWidgetsd
        )
        foreach(_dll ${_WE_DLLS})
            if(EXISTS "${CUSTOM_CODECS_DIR}/bin/${_dll}.dll")
                add_custom_command(TARGET GhostBrowser POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CUSTOM_CODECS_DIR}/bin/${_dll}.dll"
                        "$<TARGET_FILE_DIR:GhostBrowser>/${_dll}.dll"
                )
            endif()
        endforeach()

        # Overwrite WebEngine resource/pak files
        file(GLOB _WE_RESOURCES "${CUSTOM_CODECS_DIR}/resources/*")
        foreach(_res ${_WE_RESOURCES})
            get_filename_component(_fname "${_res}" NAME)
            add_custom_command(TARGET GhostBrowser POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_res}"
                    "$<TARGET_FILE_DIR:GhostBrowser>/${_fname}"
            )
        endforeach()
    endif()
endif()
