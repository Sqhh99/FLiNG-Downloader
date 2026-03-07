cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    else()
        message(WARNING "VCPKG_ROOT environment variable not set. Vcpkg integration might fail.")
    endif()
endif()

set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "Vcpkg triplet")
set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH "Vcpkg installed directory")

project("FLiNGDownloader" LANGUAGES CXX)

# Unified application version:
# 1) use -DAPP_VERSION=... when provided
# 2) otherwise derive from git tags (vX.Y.Z or vX.Y.Z-N-gHASH)
set(APP_VERSION "" CACHE STRING "Application version override (for example: 1.1.5)")
if(NOT APP_VERSION)
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*" --always
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        if(GIT_DESCRIBE_VERSION MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
            set(APP_VERSION "${CMAKE_MATCH_1}")
        elseif(GIT_DESCRIBE_VERSION MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)-([0-9]+)-g([0-9a-fA-F]+)$")
            set(APP_VERSION "${CMAKE_MATCH_1}-dev.${CMAKE_MATCH_2}+g${CMAKE_MATCH_3}")
        endif()
    endif()
endif()

if(NOT APP_VERSION)
    set(APP_VERSION "0.0.0-dev")
endif()

set(APP_VERSION "${APP_VERSION}" CACHE STRING "Application version override (for example: 1.1.5)" FORCE)

if(APP_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
    set(APP_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(APP_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(APP_VERSION_PATCH "${CMAKE_MATCH_3}")
else()
    set(APP_VERSION_MAJOR "0")
    set(APP_VERSION_MINOR "0")
    set(APP_VERSION_PATCH "0")
endif()

set(APP_VERSION_FILE_COMMA "${APP_VERSION_MAJOR},${APP_VERSION_MINOR},${APP_VERSION_PATCH},0")
set(APP_VERSION_FILE_DOT "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}.0")
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.ico" APP_ICON_RC_PATH)

message(STATUS "FLiNG Downloader version: ${APP_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Qt path - can be overridden via CMAKE_PREFIX_PATH
if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH "D:/Qt/6.10.0/msvc2022_64")
endif()
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Dynamic runtime library for MSVC (MD/MDd) - must match Qt's pre-built DLLs
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# UTF-8 encoding for MSVC
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(ZLIB REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Concurrent
    Qml
    Quick
    QuickControls2
)

set(PROJECT_SOURCES
    src/main.cpp
    src/ConfigManager.cpp
    src/ModifierParser.cpp
    src/NetworkManager.cpp
    src/ModifierManager.cpp
    src/FileSystem.cpp
    src/UpdateManager.cpp
    src/DownloadManager.cpp
    src/ModifierInfoManager.cpp
    src/SearchManager.cpp
    src/LanguageManager.cpp
    src/GameMappingManager.cpp
    src/translator.cpp
    src/CoverExtractor.cpp
    src/Backend.cpp
    src/ModifierListModel.cpp
    src/DownloadedModifierModel.cpp
)

set(PROJECT_HEADERS         
    src/include/ConfigManager.h
    src/include/ModifierParser.h
    src/include/NetworkManager.h
    src/include/ModifierManager.h
    src/include/FileSystem.h
    src/include/UpdateManager.h
    src/include/DownloadManager.h
    src/include/ModifierInfoManager.h
    src/include/SearchManager.h
    src/include/LanguageManager.h
    src/include/GameMappingManager.h
    src/include/translator.h
    src/include/CoverExtractor.h
    src/include/Backend.h
    src/include/ModifierListModel.h
    src/include/DownloadedModifierModel.h
    src/include/ThemeManager.h
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/icon.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/icon.rc
    @ONLY
)

set(PROJECT_RC_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/icon.rc
)

add_executable(${PROJECT_NAME}
    WIN32
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_RC_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "FLiNG Downloader")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    # Qt6 Components
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Concurrent
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    
    # Third-party Libraries
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pugixml::pugixml
    ${OpenCV_LIBS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CURL_STATICLIB
    FLING_APP_VERSION="${APP_VERSION}"
)

if(MSVC)
    # Runtime library (MD/MDd) - must match Qt's pre-built DLLs
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
    
    # Compiler optimizations
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/GL>    # Whole program optimization
        $<$<CONFIG:Release>:/Oi>    # Intrinsic functions
        /GS                         # Buffer security check
        /guard:cf                   # Control flow guard
    )
    
    # Linker options
    target_link_options(${PROJECT_NAME} PRIVATE
        # Optimization
        /INCREMENTAL:NO
        $<$<CONFIG:Release>:/LTCG>  # Link-time code generation
        
        # Security
        /DYNAMICBASE                # ASLR
        /NXCOMPAT                   # DEP
        /LARGEADDRESSAWARE
    )
    
    # Windows system libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32.lib
        wldap32.lib
        crypt32.lib
        normaliz.lib
        User32.lib
        Gdi32.lib
        Advapi32.lib
    )
endif()

# QML Module (Qt 6) replacing qrc/qmldir usage
set_source_files_properties(qml/themes/ThemeProvider.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(${PROJECT_NAME}
    URI FLiNGDownloader
    VERSION 1.0
    RESOURCE_PREFIX /
    NO_RESOURCE_TARGET_PATH
    QML_FILES
        qml/Main.qml
        qml/components/StyledButton.qml
        qml/components/StyledTextField.qml
        qml/components/StyledComboBox.qml
        qml/components/StyledTable.qml
        qml/components/ProgressIndicator.qml
        qml/components/IconButton.qml
        qml/components/CustomTitleBar.qml
        qml/components/DetailDrawer.qml
        qml/components/DownloadListPopup.qml
        qml/components/SettingsDialog.qml
        qml/pages/SearchPage.qml
        qml/pages/DownloadedPage.qml
        qml/pages/ModifierDetailPanel.qml
        qml/layouts/AppMenuBar.qml
        qml/layouts/AppStatusBar.qml
        qml/themes/ThemeProvider.qml
    RESOURCES
        resources/icons/app_icon.png
        resources/icons/search.png
        resources/icons/download.png
        resources/icons/settings.png
        resources/icons/folder.png
        resources/icons/delete.png
        resources/icons/exit.png
        resources/icons/app_icon.ico
        resources/icons/maximize.png
        resources/icons/maximize_restoration.png
        resources/icons/minimize.png
        resources/icons/details.png
        resources/icons/about.png
        resources/icons/language.png
        resources/icons/pause.png
        resources/icons/step-forward.png
        resources/game_mappings.json
        resources/translations/flingdownloader_en_US.qm
        resources/translations/flingdownloader_ja_JP.qm
)

set(LAUNCHER_TARGET "FLiNGLauncher")
add_executable(${LAUNCHER_TARGET}
    WIN32
    src/launcher/main.cpp
    ${PROJECT_RC_FILES}
)

set_target_properties(${LAUNCHER_TARGET} PROPERTIES OUTPUT_NAME "FLiNG Launcher")

if(MSVC)
    set_property(TARGET ${LAUNCHER_TARGET} PROPERTY
        # Keep launcher self-contained on clean Windows hosts.
        # Main app remains /MD for Qt compatibility, launcher can safely use /MT.
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(${LAUNCHER_TARGET} PRIVATE
        $<$<CONFIG:Release>:/GL>
        /GS
        /guard:cf
    )
    target_link_options(${LAUNCHER_TARGET} PRIVATE
        /INCREMENTAL:NO
        $<$<CONFIG:Release>:/LTCG>
        /DYNAMICBASE
        /NXCOMPAT
        /LARGEADDRESSAWARE
    )
endif()

target_link_libraries(${LAUNCHER_TARGET} PRIVATE
    shell32.lib
)

# install(TARGETS ${PROJECT_NAME}
#     RUNTIME DESTINATION bin
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
# )
