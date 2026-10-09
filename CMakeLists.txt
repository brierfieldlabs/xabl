cmake_minimum_required(VERSION 3.25)

project(xabl VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(xabl_core
    libs/runtime/src/compiler.cpp
    libs/runtime/src/expression.cpp
    libs/runtime/src/value.cpp
    libs/runtime/src/dbf.cpp
    libs/runtime/src/ndx.cpp
    libs/runtime/src/vm.cpp
)

target_include_directories(xabl_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/runtime/include
)

target_compile_options(xabl_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

add_executable(xabl apps/xabl/main.cpp)
target_link_libraries(xabl PRIVATE xabl_core)

enable_testing()

add_executable(xabl_smoke_tests tests/test_smoke.cpp)
target_link_libraries(xabl_smoke_tests PRIVATE xabl_core)

add_test(
    NAME xabl_fixture_generate
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/generate_customers_dbf.py
)

add_test(
    NAME xabl_smoke
    COMMAND xabl_smoke_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
)

set_tests_properties(xabl_smoke PROPERTIES DEPENDS xabl_fixture_generate)

add_executable(xabl_expression_tests tests/test_expressions.cpp)
target_link_libraries(xabl_expression_tests PRIVATE xabl_core)
add_test(NAME xabl_expressions COMMAND xabl_expression_tests)

add_executable(xabl_append_tests tests/test_append.cpp)
target_link_libraries(xabl_append_tests PRIVATE xabl_core)
add_test(
    NAME xabl_append
    COMMAND xabl_append_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
)
set_tests_properties(xabl_append PROPERTIES DEPENDS xabl_fixture_generate)
