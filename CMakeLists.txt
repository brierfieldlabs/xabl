cmake_minimum_required(VERSION 3.25)

project(xabl VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_library(xabl_core
    libs/runtime/src/compiler.cpp
    libs/runtime/src/expression.cpp
    libs/runtime/src/profile.cpp
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

add_executable(xabl_control_flow_tests tests/test_control_flow.cpp)
target_link_libraries(xabl_control_flow_tests PRIVATE xabl_core)
add_test(
    NAME xabl_control_flow
    COMMAND xabl_control_flow_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
)
set_tests_properties(xabl_control_flow PROPERTIES DEPENDS xabl_fixture_generate)

add_executable(xabl_profile_tests tests/test_profiles.cpp)
target_link_libraries(xabl_profile_tests PRIVATE xabl_core)
add_test(NAME xabl_profiles COMMAND xabl_profile_tests)

add_executable(xabl_ndx_navigation_tests tests/test_ndx_navigation.cpp)
target_link_libraries(xabl_ndx_navigation_tests PRIVATE xabl_core)
add_test(
    NAME xabl_ndx_navigation
    COMMAND xabl_ndx_navigation_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
)
set_tests_properties(xabl_ndx_navigation PROPERTIES DEPENDS xabl_fixture_generate)

add_executable(xabl_source_comments_tests tests/test_source_comments.cpp)
target_link_libraries(xabl_source_comments_tests PRIVATE xabl_core)
add_test(
    NAME xabl_source_comments
    COMMAND xabl_source_comments_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
)
set_tests_properties(xabl_source_comments PROPERTIES DEPENDS xabl_fixture_generate)


add_executable(xabl_date_tests tests/test_dates.cpp)
target_link_libraries(xabl_date_tests PRIVATE xabl_core)
add_test(
    NAME xabl_dates
    COMMAND xabl_date_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
)
set_tests_properties(xabl_dates PROPERTIES DEPENDS xabl_fixture_generate)


# The text-mode editor is independently testable, even if curses is absent.
add_library(xabl_text_editor
    apps/xabl-tui/editor_buffer.cpp
    apps/xabl-tui/syntax_highlight.cpp
    apps/xabl-tui/file_browser.cpp)
target_include_directories(xabl_text_editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/xabl-tui)
add_executable(xabl_text_editor_tests tests/test_text_editor.cpp)
target_link_libraries(xabl_text_editor_tests PRIVATE xabl_text_editor)
add_test(NAME xabl_text_editor COMMAND xabl_text_editor_tests)
add_executable(xabl_syntax_tests tests/test_syntax_highlight.cpp)
target_link_libraries(xabl_syntax_tests PRIVATE xabl_text_editor)
add_test(NAME xabl_syntax_highlighting COMMAND xabl_syntax_tests)
add_executable(xabl_file_browser_tests tests/test_file_browser.cpp)
target_link_libraries(xabl_file_browser_tests PRIVATE xabl_text_editor)
add_test(NAME xabl_file_browser COMMAND xabl_file_browser_tests)

# ncurses is optional: other binaries and the future Qt IDE do not depend
# on a terminal frontend. Windows can add a PDCurses-backed frontend later.
find_package(Curses QUIET)
if(CURSES_FOUND)
    add_executable(xabl-tui apps/xabl-tui/main.cpp)
    target_include_directories(xabl-tui PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(xabl-tui PRIVATE xabl_core xabl_text_editor
        ${CURSES_LIBRARIES})
    if(UNIX)
        add_test(NAME xabl_tui_pty
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_text_studio_pty.py
                $<TARGET_FILE:xabl-tui>)
    endif()
endif()
