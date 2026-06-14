cmake_minimum_required(VERSION 3.16)
project(trix_smoke LANGUAGES CXX)

# ── Locate trix ──────────────────────────────────────────────────────────────
# When built as a subdirectory of the main trix tree the 'trix' target already
# exists.  When built standalone (after cmake --install), find the package.
if(TARGET trix)
    set(_trix_target trix)
else()
    find_package(Trix REQUIRED)
    set(_trix_target Trix::trix)
endif()

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(trix_smoke smoke.cpp)

target_link_libraries(trix_smoke PRIVATE ${_trix_target})
target_compile_definitions(trix_smoke PRIVATE TRIX_ENABLED)
target_compile_features(trix_smoke PRIVATE cxx_std_11)

# Threads (std::thread requires -pthread on Linux)
find_package(Threads REQUIRED)
target_link_libraries(trix_smoke PRIVATE Threads::Threads)

# Always emit debug info so the smoke run is traceable
target_compile_options(trix_smoke PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/Zi,-g>
)
target_link_options(trix_smoke PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/DEBUG>
)
