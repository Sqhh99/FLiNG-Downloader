cmake_minimum_required(VERSION 3.25)

# ==============================================================================
# Vcpkg Toolchain Configuration
# Note: Must be set before project() command
# ==============================================================================

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    else()
        message(WARNING "VCPKG_ROOT environment variable not set. Vcpkg integration might fail.")
    endif()
endif()

set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg triplet")
set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed" CACHE PATH "Vcpkg installed directory")

# ==============================================================================
# Project Definition
# ==============================================================================

project(DownloadIntegrator LANGUAGES CXX)

# ==============================================================================
# Build Configuration
# ==============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Static runtime library for MSVC (MT/MTd)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# UTF-8 encoding for MSVC
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==============================================================================
# Dependencies
# ==============================================================================

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

# ==============================================================================
# Source Files
# ==============================================================================

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

set(PROJECT_RESOURCES
    resources/resources.qrc
    qml/qml.qrc
)

set(PROJECT_RC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/icon.rc
)

# ==============================================================================
# Target Definition
# ==============================================================================

add_executable(${PROJECT_NAME}
    WIN32
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_RESOURCES}
    ${PROJECT_RC_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src/include
)

# ==============================================================================
# Link Libraries
# ==============================================================================

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
)

# ==============================================================================
# MSVC-Specific Configuration
# ==============================================================================

if(MSVC)
    # Runtime library
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
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
        # Exclude dynamic runtime libraries
        /NODEFAULTLIB:msvcrt.lib
        /NODEFAULTLIB:msvcrtd.lib
        /NODEFAULTLIB:msvcprt.lib
        /NODEFAULTLIB:msvcprtd.lib
        
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

# ==============================================================================
# Installation (Optional)
# ==============================================================================

# install(TARGETS ${PROJECT_NAME}
#     RUNTIME DESTINATION bin
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
# )