set(TEST_COMPILE_FLAGS -DTRIX_ENABLED)

# --- C API test ---
add_executable(test_basic test_basic.c)
target_link_libraries(test_basic PRIVATE trix)
target_compile_definitions(test_basic PRIVATE TRIX_ENABLED)
add_test(NAME basic COMMAND test_basic)

# --- C++ RAII wrapper test ---
add_executable(test_cpp test_cpp.cpp)
target_link_libraries(test_cpp PRIVATE trix)
target_compile_definitions(test_cpp PRIVATE TRIX_ENABLED)
add_test(NAME cpp COMMAND test_cpp)

# --- No-op test (TRIX_ENABLED not defined, no link) ---
add_executable(test_nop test_nop.c)
target_include_directories(test_nop PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME nop COMMAND test_nop)
