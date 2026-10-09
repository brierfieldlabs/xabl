// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace xabl::tui {

/// Terminal-independent editor model, separately testable from curses.
/// Source bytes are preserved through ordinary load/save (including CRLF).
/// The first edit normalises mixed line endings; NUL files are rejected.
class EditorBuffer {
public:
    struct Cursor {
        std::size_t row{};
        std::size_t column{};
    };

    EditorBuffer();
    void load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path);
    void clear();
    [[nodiscard]] std::string text() const;
    [[nodiscard]] const std::vector<std::string>& lines() const noexcept;
    [[nodiscard]] Cursor cursor() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;

    void set_cursor(std::size_t row, std::size_t column);
    void move_left();
    void move_right();
    void move_up();
    void move_down();
    void home();
    void end();
    void insert(char character);
    void insert_text(std::string_view characters);
    void newline();
    void backspace();
    void erase();
    bool undo();
    [[nodiscard]] bool find_next(std::string_view text);

private:
    struct Snapshot {
        std::vector<std::string> lines;
        Cursor cursor;
    };
    void remember();
    void read_text(std::string_view source);
    [[nodiscard]] std::size_t content_size() const;

    std::vector<std::string> lines_{{""}};
    Cursor cursor_{};
    std::filesystem::path path_;
    std::string eol_{"\n"};
    bool dirty_{false};
    std::vector<Snapshot> undo_;
};

} // namespace xabl::tui
