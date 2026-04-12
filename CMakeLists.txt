cmake_minimum_required(VERSION 3.16)
project(trix VERSION 1.0.0 LANGUAGES C CXX)

# --- Platform-appropriate backend defaults ---
if(WIN32)
    set(_TRIX_FTRACE_DEFAULT OFF)
    set(_TRIX_PERF_DEFAULT   OFF)
    set(_TRIX_ITT_DEFAULT    ON)
    set(_TRIX_ETW_DEFAULT    ON)
else()
    set(_TRIX_FTRACE_DEFAULT ON)
    set(_TRIX_PERF_DEFAULT   ON)
    set(_TRIX_ITT_DEFAULT    ON)
    set(_TRIX_ETW_DEFAULT    OFF)
endif()

option(TRIX_BACKEND_FTRACE "Include ftrace backend (Linux only)" ${_TRIX_FTRACE_DEFAULT})
option(TRIX_BACKEND_PERF   "Include perf/SDT backend (Linux only)" ${_TRIX_PERF_DEFAULT})
option(TRIX_BACKEND_ITT    "Include Intel VTune ITT backend" ${_TRIX_ITT_DEFAULT})
option(TRIX_BACKEND_ETW    "Include ETW backend (Windows only)" ${_TRIX_ETW_DEFAULT})

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
    set(ITTAPI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ittapi")
    if(NOT EXISTS "${ITTAPI_DIR}/include/ittnotify.h")
        message(STATUS "trix: ittapi not found — fetching from GitHub...")
        include(FetchContent)
        FetchContent_Declare(ittapi
            GIT_REPOSITORY https://github.com/intel/ittapi.git
            GIT_TAG        master
            GIT_SHALLOW    TRUE
            SOURCE_DIR     "${ITTAPI_DIR}"
        )
        FetchContent_Populate(ittapi)
    endif()
    list(APPEND TRIX_SOURCES
        src/backends/itt/backend_itt.c
        ${ITTAPI_DIR}/src/ittnotify/ittnotify_static.c
    )
    list(APPEND TRIX_DEFINES TRIX_BACKEND_ITT)
    list(APPEND TRIX_INCLUDE_DIRS ${ITTAPI_DIR}/include)
endif()

if(TRIX_BACKEND_ETW)
    if(NOT WIN32)
        message(FATAL_ERROR "TRIX_BACKEND_ETW is only supported on Windows")
    endif()
    list(APPEND TRIX_SOURCES src/backends/etw/backend_etw.c)
    list(APPEND TRIX_DEFINES TRIX_BACKEND_ETW)
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
set_target_properties(trix PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(TRIX_BACKEND_ETW)
    # TraceLoggingProvider.h requires C++ in some SDK versions
    set_source_files_properties(src/backends/etw/backend_etw.c
        PROPERTIES LANGUAGE CXX)
endif()

# --- Tests ---
enable_testing()
add_subdirectory(tests)

# --- Install ---
include(GNUInstallDirs)

install(TARGETS trix
    EXPORT TrixTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES include/trix/trix.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/trix
)

install(EXPORT TrixTargets
    FILE TrixTargets.cmake
    NAMESPACE Trix::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/trix
)
