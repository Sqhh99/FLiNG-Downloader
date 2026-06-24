# =============================================================================
# FetchONNXRuntime.cmake
# Automatically download and configure ONNX Runtime (CPU) based on platform
# =============================================================================

# Version configuration (can be overridden before including this module)
if(NOT DEFINED ONNXRUNTIME_VERSION)
    set(ONNXRUNTIME_VERSION "1.27.0")
endif()

# Shared SDK architecture selector across third-party fetch modules.
# Supported values: x64, arm64. Default is x64.
if(NOT DEFINED LINKS_SDK_ARCH)
    set(LINKS_SDK_ARCH "x64")
endif()
string(TOLOWER "${LINKS_SDK_ARCH}" ONNXRUNTIME_ARCH)
if(NOT ONNXRUNTIME_ARCH MATCHES "^(x64|arm64)$")
    message(FATAL_ERROR "Unsupported LINKS_SDK_ARCH: ${LINKS_SDK_ARCH}. Expected x64 or arm64.")
endif()

# =============================================================================
# Platform / package mapping (official CPU prebuilt releases)
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(ONNXRUNTIME_ARCH STREQUAL "arm64")
        set(ONNXRUNTIME_PKG "onnxruntime-win-arm64-${ONNXRUNTIME_VERSION}")
    else()
        set(ONNXRUNTIME_PKG "onnxruntime-win-x64-${ONNXRUNTIME_VERSION}")
    endif()
    set(ONNXRUNTIME_ARCHIVE_EXT "zip")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(ONNXRUNTIME_ARCH STREQUAL "arm64")
        set(ONNXRUNTIME_PKG "onnxruntime-linux-aarch64-${ONNXRUNTIME_VERSION}")
    else()
        set(ONNXRUNTIME_PKG "onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}")
    endif()
    set(ONNXRUNTIME_ARCHIVE_EXT "tgz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(ONNXRUNTIME_ARCH STREQUAL "arm64")
        set(ONNXRUNTIME_PKG "onnxruntime-osx-arm64-${ONNXRUNTIME_VERSION}")
    else()
        set(ONNXRUNTIME_PKG "onnxruntime-osx-x86_64-${ONNXRUNTIME_VERSION}")
    endif()
    set(ONNXRUNTIME_ARCHIVE_EXT "tgz")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# =============================================================================
# SDK Paths
# =============================================================================
set(ONNXRUNTIME_INSTALL_DIR "${CMAKE_SOURCE_DIR}/third_party/${ONNXRUNTIME_PKG}")
set(ONNXRUNTIME_ARCHIVE "${ONNXRUNTIME_PKG}.${ONNXRUNTIME_ARCHIVE_EXT}")
set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_ARCHIVE}")

# Resolve the SDK root among candidate layouts: a clean extract places
# include/ + lib/ directly under ${ONNXRUNTIME_PKG}, while extracting the
# archive into a same-named folder nests them one level deeper.
function(resolve_onnxruntime_root out_var)
    set(candidates
        "${ONNXRUNTIME_INSTALL_DIR}"
        "${ONNXRUNTIME_INSTALL_DIR}/${ONNXRUNTIME_PKG}"
    )
    foreach(candidate IN LISTS candidates)
        if(EXISTS "${candidate}/include" AND EXISTS "${candidate}/lib")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

# =============================================================================
# Download and Extract SDK if not present
# =============================================================================
resolve_onnxruntime_root(ONNXRUNTIME_ROOT)

if(ONNXRUNTIME_ROOT STREQUAL "")
    message(STATUS "ONNX Runtime not found under ${ONNXRUNTIME_INSTALL_DIR}")
    message(STATUS "Downloading ONNX Runtime v${ONNXRUNTIME_VERSION} for ${CMAKE_SYSTEM_NAME}-${ONNXRUNTIME_ARCH}...")

    set(ONNXRUNTIME_DOWNLOAD_PATH "${CMAKE_SOURCE_DIR}/third_party/${ONNXRUNTIME_ARCHIVE}")

    # Download the archive
    file(DOWNLOAD
        "${ONNXRUNTIME_URL}"
        "${ONNXRUNTIME_DOWNLOAD_PATH}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )

    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR_CODE)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR_MESSAGE)

    if(NOT DOWNLOAD_ERROR_CODE EQUAL 0)
        file(REMOVE "${ONNXRUNTIME_DOWNLOAD_PATH}")
        message(FATAL_ERROR "Failed to download ONNX Runtime: ${DOWNLOAD_ERROR_MESSAGE}")
    endif()

    message(STATUS "Extracting ONNX Runtime...")

    # The archive contains a top-level folder named exactly ${ONNXRUNTIME_PKG},
    # so extract directly into third_party (mirrors the LiveKit SDK layout).
    file(ARCHIVE_EXTRACT
        INPUT "${ONNXRUNTIME_DOWNLOAD_PATH}"
        DESTINATION "${CMAKE_SOURCE_DIR}/third_party"
    )

    # Clean up the archive
    file(REMOVE "${ONNXRUNTIME_DOWNLOAD_PATH}")

    resolve_onnxruntime_root(ONNXRUNTIME_ROOT)
    if(ONNXRUNTIME_ROOT STREQUAL "")
        message(FATAL_ERROR
            "Failed to extract ONNX Runtime from ${ONNXRUNTIME_ARCHIVE}. "
            "Could not find a directory containing include/ and lib/ under ${ONNXRUNTIME_INSTALL_DIR}.")
    endif()

    message(STATUS "ONNX Runtime v${ONNXRUNTIME_VERSION} installed successfully")
else()
    message(STATUS "Found ONNX Runtime at ${ONNXRUNTIME_ROOT}")
endif()

# =============================================================================
# Configure SDK paths
# =============================================================================
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_ROOT}/lib")

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib")
    set(ONNXRUNTIME_SHARED_LIB "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll")
    set(ONNXRUNTIME_BIN_DIR "${ONNXRUNTIME_LIB_DIR}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.so")
    set(ONNXRUNTIME_SHARED_LIB "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.so.${ONNXRUNTIME_VERSION}")
    set(ONNXRUNTIME_BIN_DIR "${ONNXRUNTIME_LIB_DIR}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ONNXRUNTIME_LIBRARY "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib")
    set(ONNXRUNTIME_SHARED_LIB "${ONNXRUNTIME_LIBRARY}")
    set(ONNXRUNTIME_BIN_DIR "${ONNXRUNTIME_LIB_DIR}")
endif()

if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}")
    message(FATAL_ERROR "ONNX Runtime include directory not found: ${ONNXRUNTIME_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${ONNXRUNTIME_LIBRARY}")
    message(FATAL_ERROR "ONNX Runtime library not found: ${ONNXRUNTIME_LIBRARY}")
endif()

message(STATUS "ONNX Runtime Configuration:")
message(STATUS "  Root: ${ONNXRUNTIME_ROOT}")
message(STATUS "  Arch: ${ONNXRUNTIME_ARCH}")
message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_DIR}")
message(STATUS "  Library: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "  Shared lib: ${ONNXRUNTIME_SHARED_LIB}")
