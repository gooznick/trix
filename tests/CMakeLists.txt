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

# --- Version test ---
add_executable(test_version test_version.c)
target_link_libraries(test_version PRIVATE trix)
target_compile_definitions(test_version PRIVATE TRIX_ENABLED)
add_test(NAME version COMMAND test_version)

add_executable(test_nop test_nop.c)
target_include_directories(test_nop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
add_test(NAME nop COMMAND test_nop)
