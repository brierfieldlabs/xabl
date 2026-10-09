// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Headless editor-model regressions, independent from curses/terminal size.
#include "editor_buffer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using xabl::tui::EditorBuffer;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}
std::string read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
}
int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = fs::temp_directory_path() /
                           ("xabl-text-editor-" + std::to_string(suffix));
    try {
        fs::create_directories(directory);
        EditorBuffer editor;
        check(editor.text().empty() && !editor.dirty(), "fresh buffer");
        editor.insert_text("USE CUSTOMERS");
        editor.newline();
        editor.insert_text("? NAME");
        check(editor.text() == "USE CUSTOMERS\n? NAME", "multiline insertion");
        check(editor.dirty() && editor.cursor().row == 1 &&
              editor.cursor().column == 6, "cursor position after edits");
        editor.home();
        editor.backspace();
        check(editor.text() == "USE CUSTOMERS? NAME", "join via backspace");
        check(editor.undo(), "undo split join");
        check(editor.text() == "USE CUSTOMERS\n? NAME", "undo restores line");
        editor.set_cursor(0, 99);
        editor.erase();
        check(editor.text() == "USE CUSTOMERS? NAME", "delete joins lines");
        editor.undo();
        editor.set_cursor(0, 0);
        editor.backspace();
        check(editor.text() == "USE CUSTOMERS\n? NAME",
              "backspace at beginning is harmless");
        editor.set_cursor(0, 99);
        check(editor.cursor().column == 13, "clamp large cursor column");
        editor.move_right();
        check(editor.cursor().row == 1 && editor.cursor().column == 0,
              "move through newline");
        check(editor.find_next("NAME") && editor.cursor().column == 2,
              "search across source");
        check(editor.find_next("USE") && editor.cursor().row == 0,
              "search wraps around");
        check(!editor.find_next("MISSING"), "missing search term");
        check(editor.go_to_line(2) && editor.cursor().row == 1 &&
              editor.cursor().column == 0, "go to second source line");
        const auto unchanged = editor.cursor();
        check(!editor.go_to_line(0) && !editor.go_to_line(3) &&
              editor.cursor() == unchanged, "invalid line must not move cursor");
        check(editor.go_to_line(1) && editor.cursor().row == 0,
              "go to first source line");
        using xabl::tui::diagnostic_source_line;
        check(diagnostic_source_line("line 2: unsupported statement").value() == 2,
              "parse compiler line number");
        check(!diagnostic_source_line("line 0: invalid") &&
              !diagnostic_source_line("error: line 2") &&
              !diagnostic_source_line("line 2x: error") &&
              !diagnostic_source_line("line 9999999999999999999999: overflow"),
              "reject invalid diagnostic locations");

        const auto file = directory / "demo.prg";
        editor.save(file);
        check(!editor.dirty() && read(file) == editor.text(),
              "saved source matches model");
        write(directory / "windows.prg", "USE CUSTOMERS\r\n? NAME\r\n");
        editor.load(directory / "windows.prg");
        check(editor.text() == "USE CUSTOMERS\r\n? NAME\r\n",
              "CRLF source roundtrip");
        editor.set_cursor(1, 6);
        editor.insert_text(" + '!' ");
        editor.save(directory / "windows-copy.prg");
        check(read(directory / "windows-copy.prg") ==
              "USE CUSTOMERS\r\n? NAME + '!' \r\n", "CRLF remains after edits");
        write(directory / "binary.prg", std::string("A\0B", 3));
        const auto before = editor.text();
        bool refused = false;
        try { editor.load(directory / "binary.prg"); }
        catch (const std::runtime_error&) { refused = true; }
        check(refused && editor.text() == before,
              "binary load refused without losing current source");
        refused = false;
        try { editor.load(directory / "missing.prg"); }
        catch (const std::runtime_error&) { refused = true; }
        check(refused && editor.text() == before, "failed open keeps document");

        // Save failures must not damage a previously saved source file.
        editor.insert_text("CHANGED ");
        const auto safe_copy = directory / "safe.prg";
        write(safe_copy, "PRIOR CONTENT\n");
        editor.save(safe_copy);
        check(read(safe_copy) == editor.text() &&
                  read(safe_copy) != "PRIOR CONTENT\n",
              "save should replace the old source after staging");
        editor.insert_text("again ");
        const auto safe_bytes = read(safe_copy);
        bool invalid_save_refused = false;
        try { editor.save(directory); }
        catch (const std::exception&) { invalid_save_refused = true; }
        check(invalid_save_refused && read(safe_copy) == safe_bytes && editor.dirty(),
              "failed replacement must preserve saved file and dirty state");

        editor.clear();
        check(editor.text().empty() && editor.path().empty() &&
              !editor.dirty(), "new document");
        editor.insert_text("ABC");
        editor.set_cursor(0, 1);
        editor.insert('X');
        check(editor.text() == "AXBC", "inline insertion");
        editor.backspace();
        check(editor.text() == "ABC", "backspace erases exact character");
        editor.end();
        editor.move_left();
        editor.move_up();
        editor.move_down();
        check(editor.cursor().column == 2, "cursor movement");
        fs::remove_all(directory);
        std::cout << "headless text editor tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "text editor test failed: " << e.what() << "\n";
        std::error_code ignored;
        fs::remove_all(directory, ignored);
        return 1;
    }
}
