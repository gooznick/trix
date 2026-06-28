cmake_minimum_required(VERSION 3.16)

# --- Read version from trix_version.h (single source of truth) ---
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/trix/trix_version.h" _ver_lines
    REGEX "^#define TRIX_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(_line ${_ver_lines})
    if(_line MATCHES "TRIX_VERSION_MAJOR ([0-9]+)")
        set(_TRIX_MAJOR ${CMAKE_MATCH_1})
    elseif(_line MATCHES "TRIX_VERSION_MINOR ([0-9]+)")
        set(_TRIX_MINOR ${CMAKE_MATCH_1})
    elseif(_line MATCHES "TRIX_VERSION_PATCH ([0-9]+)")
        set(_TRIX_PATCH ${CMAKE_MATCH_1})
    endif()
endforeach()

project(trix VERSION "${_TRIX_MAJOR}.${_TRIX_MINOR}.${_TRIX_PATCH}" LANGUAGES C CXX)

# --- Install prefix ---
# By default, trix installs to <build>/install (a local, non-system directory).
# Pass -DCMAKE_INSTALL_SYSTEM=ON at configure time to install to the system prefix
# (/usr/local on Linux, C:/Program Files/trix on Windows) instead.
# Passing -DCMAKE_INSTALL_PREFIX=<path> at any time overrides both behaviours.
option(CMAKE_INSTALL_SYSTEM
    "Install to system prefix (e.g. /usr/local on Linux). Default: <build-dir>/install."
    OFF)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # _TRIX_INSTALL_PREFIX_MANAGED remembers the prefix we last auto-set so we can
    # undo it correctly when the user toggles CMAKE_INSTALL_SYSTEM.
    if(NOT DEFINED _TRIX_INSTALL_PREFIX_MANAGED)
        set(_TRIX_INSTALL_PREFIX_MANAGED "" CACHE INTERNAL "Last install prefix auto-set by trix")
    endif()

    if(CMAKE_INSTALL_SYSTEM)
        # Switch from local to system: only touch the prefix when it is still the value
        # we auto-set (i.e. not something the user provided explicitly).
        if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR
           CMAKE_INSTALL_PREFIX STREQUAL _TRIX_INSTALL_PREFIX_MANAGED)
            if(WIN32)
                set(CMAKE_INSTALL_PREFIX "C:/Program Files/trix"
                    CACHE PATH "Installation prefix" FORCE)
            else()
                set(CMAKE_INSTALL_PREFIX "/usr/local"
                    CACHE PATH "Installation prefix" FORCE)
            endif()
            set(_TRIX_INSTALL_PREFIX_MANAGED "" CACHE INTERNAL "Last install prefix auto-set by trix")
        endif()
    elseif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        # First configure without CMAKE_INSTALL_SYSTEM: default to local build tree.
        set(CMAKE_INSTALL_PREFIX "${PROJECT_BINARY_DIR}/install"
            CACHE PATH "Installation prefix (default: <build>/install; use -DCMAKE_INSTALL_SYSTEM=ON for system install)"
            FORCE)
        set(_TRIX_INSTALL_PREFIX_MANAGED "${PROJECT_BINARY_DIR}/install"
            CACHE INTERNAL "Last install prefix auto-set by trix")
    endif()

    message(STATUS "trix: install prefix — ${CMAKE_INSTALL_PREFIX}")
    if(NOT CMAKE_INSTALL_SYSTEM)
        message(STATUS "trix:   (use -DCMAKE_INSTALL_SYSTEM=ON to install to the system prefix instead)")
    endif()
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Platform-appropriate backend defaults ---
if(WIN32)
    set(_TRIX_FTRACE_DEFAULT OFF)
    set(_TRIX_PERF_DEFAULT   OFF)
    set(_TRIX_ITT_DEFAULT    ON)
    set(_TRIX_ETW_DEFAULT    ON)
    set(_TRIX_LTTNG_DEFAULT  OFF)
else()
    set(_TRIX_FTRACE_DEFAULT ON)
    set(_TRIX_PERF_DEFAULT   ON)
    set(_TRIX_ITT_DEFAULT    ON)
    set(_TRIX_ETW_DEFAULT    OFF)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(_LTTNG_PROBE QUIET lttng-ust)
        if(_LTTNG_PROBE_FOUND)
            set(_TRIX_LTTNG_DEFAULT ON)
        else()
            set(_TRIX_LTTNG_DEFAULT OFF)
            message(STATUS "trix: liblttng-ust-dev not found — LTTng backend disabled (install liblttng-ust-dev to enable)")
        endif()
    else()
        set(_TRIX_LTTNG_DEFAULT OFF)
        message(STATUS "trix: pkg-config not found — LTTng backend disabled")
    endif()
endif()

option(TRIX_BACKEND_FTRACE "Include ftrace backend (Linux only)" ${_TRIX_FTRACE_DEFAULT})
option(TRIX_BACKEND_PERF   "Include perf/SDT backend (Linux only)" ${_TRIX_PERF_DEFAULT})
option(TRIX_BACKEND_ITT    "Include Intel VTune ITT backend" ${_TRIX_ITT_DEFAULT})
option(TRIX_BACKEND_ETW    "Include ETW backend (Windows only)" ${_TRIX_ETW_DEFAULT})
option(TRIX_BACKEND_LTTNG  "Include LTTng-UST backend (Linux only, requires liblttng-ust-dev)" ${_TRIX_LTTNG_DEFAULT})
option(TRIX_BACKEND_ATRACE "Include atrace/file backend (Linux/Windows)" ON)

# --- Library sources ---
set(TRIX_SOURCES
    src/trix_dispatch.c
    src/backends/nop/backend_nop.c
)

set(TRIX_DEFINES)
set(TRIX_INCLUDE_DIRS)

if(TRIX_BACKEND_FTRACE)
    if(NOT UNIX)
        message(FATAL_ERROR "TRIX_BACKEND_FTRACE is only supported on Linux")
    endif()
    list(APPEND TRIX_SOURCES src/backends/ftrace/backend_ftrace.c)
    list(APPEND TRIX_DEFINES TRIX_BACKEND_FTRACE)
endif()

if(TRIX_BACKEND_PERF)
    if(NOT UNIX)
        message(FATAL_ERROR "TRIX_BACKEND_PERF is only supported on Linux")
    endif()
    list(APPEND TRIX_SOURCES src/backends/perf/backend_perf.c)
    list(APPEND TRIX_DEFINES TRIX_BACKEND_PERF)
endif()

if(TRIX_BACKEND_ITT)
    # ittapi sources are bundled in third_party/ittapi/ (from https://github.com/intel/ittapi).
    # To update: replace third_party/ittapi/ with a fresh clone/export of the desired tag.
    set(ITTAPI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ittapi")
    if(NOT EXISTS "${ITTAPI_DIR}/include/ittnotify.h")
        message(FATAL_ERROR "trix: ittapi not found at ${ITTAPI_DIR}. "
            "Ensure the third_party/ittapi directory is present (git clone includes it).")
    endif()
    list(APPEND TRIX_SOURCES
        src/backends/itt/backend_itt.cpp
        ${ITTAPI_DIR}/src/ittnotify/ittnotify_static.c
    )
    list(APPEND TRIX_DEFINES TRIX_BACKEND_ITT)
    list(APPEND TRIX_INCLUDE_DIRS ${ITTAPI_DIR}/include)
endif()

if(TRIX_BACKEND_ETW)
    if(NOT WIN32)
        message(FATAL_ERROR "TRIX_BACKEND_ETW is only supported on Windows")
    endif()
    list(APPEND TRIX_SOURCES src/backends/etw/backend_etw.cpp)
    list(APPEND TRIX_DEFINES TRIX_BACKEND_ETW)
endif()

if(TRIX_BACKEND_LTTNG)
    if(NOT UNIX)
        message(FATAL_ERROR "TRIX_BACKEND_LTTNG is only supported on Linux")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LTTNG_UST REQUIRED IMPORTED_TARGET lttng-ust)
    # lttng-ust >= 2.14 (Ubuntu 24.04+) uses a renamed API set.
    if(LTTNG_UST_VERSION VERSION_GREATER_EQUAL "2.14")
        list(APPEND TRIX_DEFINES TRIX_LTTNG_UST_NEW_API)
        message(STATUS "trix: lttng-ust ${LTTNG_UST_VERSION} — using new API (>= 2.14)")
    else()
        message(STATUS "trix: lttng-ust ${LTTNG_UST_VERSION} — using old API (< 2.14)")
    endif()
    list(APPEND TRIX_SOURCES src/backends/lttng/backend_lttng.c)
    list(APPEND TRIX_DEFINES TRIX_BACKEND_LTTNG)
endif()

if(TRIX_BACKEND_ATRACE)
    list(APPEND TRIX_SOURCES src/backends/atrace/backend_atrace.c)
    list(APPEND TRIX_DEFINES TRIX_BACKEND_ATRACE)
endif()

# --- Shared library target ---
add_library(trix SHARED ${TRIX_SOURCES})

target_include_directories(trix
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
        ${TRIX_INCLUDE_DIRS}
)

target_compile_definitions(trix PRIVATE ${TRIX_DEFINES} _GNU_SOURCE TRIX_BUILDING_DLL)
set_target_properties(trix PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(TRIX_BACKEND_LTTNG)
    # Link against lttng-ust (handles -llttng-ust and include dirs).
    # The tracepoint header must be found via the source directory.
    target_link_libraries(trix PRIVATE PkgConfig::LTTNG_UST)
    target_include_directories(trix PRIVATE src/backends/lttng)
endif()

# --- Tests ---
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_TRIX_TESTS_DEFAULT ON)
else()
    set(_TRIX_TESTS_DEFAULT OFF)
endif()
option(TRIX_BUILD_TESTS "Build trix tests" ${_TRIX_TESTS_DEFAULT})
if(TRIX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Demo (requires OpenCV) ---
option(TRIX_BUILD_DEMO "Build the trix demo application (requires OpenCV)" OFF)
if(TRIX_BUILD_DEMO)
    add_subdirectory(demo)
endif()

# --- Smoke tester ---
option(TRIX_BUILD_SMOKE "Build the trix smoke tester" ON)
if(TRIX_BUILD_SMOKE)
    add_subdirectory(smoke)
endif()

# --- Install ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Print the effective prefix at install time (picks up --prefix overrides too).
install(CODE "message(STATUS \"trix ${PROJECT_VERSION}: installing to '\${CMAKE_INSTALL_PREFIX}'\")")

install(TARGETS trix
    EXPORT TrixTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/trix/trix.h include/trix/trix_version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/trix
)

install(EXPORT TrixTargets
    FILE TrixTargets.cmake
    NAMESPACE Trix::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trix
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TrixConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TrixConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trix
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TrixConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TrixConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TrixConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trix
)
