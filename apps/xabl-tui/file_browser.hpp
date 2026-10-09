// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace xabl::tui {

/// Directory browser independent of ncurses. Lists directories and PRG/XABL
/// files; never creates/deletes/moves files and does not execute source.
class FileBrowser {
public:
    struct Entry {
        std::filesystem::path path;
        std::string display_name;
        bool directory{};
    };
    explicit FileBrowser(std::filesystem::path starting_directory);
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;
    [[nodiscard]] const std::vector<Entry>& entries() const noexcept;
    [[nodiscard]] std::size_t selected() const noexcept;
    void move_up() noexcept;
    void move_down() noexcept;
    void select(std::size_t index) noexcept;
    /// Enter directory or return a source path (never reads source bytes).
    [[nodiscard]] std::filesystem::path activate();
    void parent();
    void refresh();

private:
    std::filesystem::path directory_;
    std::vector<Entry> entries_;
    std::size_t selected_{};
};

} // namespace xabl::tui
