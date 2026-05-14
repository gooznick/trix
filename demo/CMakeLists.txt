cmake_minimum_required(VERSION 3.16)
project(trix_demo LANGUAGES CXX)

# ── Locate trix ──────────────────────────────────────────────────────────────
# When built as a subdirectory of the main trix tree the 'trix' target already
# exists.  When built standalone (after cmake --install), find the package.
if(TARGET trix)
    set(_trix_target trix)
else()
    find_package(Trix REQUIRED)
    set(_trix_target Trix::trix)
endif()

# ── Locate OpenCV ─────────────────────────────────────────────────────────────
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(trix_demo demo.cpp)

target_link_libraries(trix_demo PRIVATE ${_trix_target} ${OpenCV_LIBS})
target_compile_definitions(trix_demo PRIVATE TRIX_ENABLED)
target_compile_features(trix_demo PRIVATE cxx_std_17)
