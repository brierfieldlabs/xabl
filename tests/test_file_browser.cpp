// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include "file_browser.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
void require(bool correct, const std::string& message) {
    if (!correct) throw std::runtime_error(message);
}
void write(const fs::path& path) { std::ofstream out(path); out << "? 5\n"; }
}
int main() {
    const auto location = fs::temp_directory_path() /
                          ("xabl-browser-" + std::to_string(
                              std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(location / "Assets");
        fs::create_directories(location / "Modules");
        write(location / "test.PRG");
        write(location / "beta.xabl");
        write(location / "README.txt");
        write(location / "Modules" / "sample.prg");
        std::error_code ignored;
        fs::create_symlink(location / "test.PRG", location / "linked.PRG", ignored);
        fs::create_directory_symlink(location / "Modules", location / "linked-folder", ignored);
        xabl::tui::FileBrowser browser(location);
        const auto& entries = browser.entries();
        require(entries.size() == 5, "expected parent, two dirs, two source files");
        require(entries[0].display_name.starts_with("[..]"), "parent first");
        require(entries[1].display_name == "[DIR] Assets" &&
                entries[2].display_name == "[DIR] Modules", "folders sorted first");
        require(entries[3].path.filename() == "beta.xabl" &&
                entries[4].path.filename() == "test.PRG", "source extension filtering");
        browser.select(4);
        require(browser.activate() == location / "test.PRG", "select existing source");
        require(browser.directory() == location, "selection should not change dir");
        browser.select(2);
        require(browser.activate().empty() && browser.directory() == location / "Modules",
                "enter folder");
        browser.move_down();
        require(browser.activate() == location / "Modules" / "sample.prg",
                "select nested source");
        browser.parent();
        require(browser.directory() == location, "go to parent folder");
        browser.select(9999);
        require(browser.selected() == 4, "selection clamps");
        browser.move_down();
        require(browser.selected() == 4, "move at end clamps");
        browser.refresh();
        require(browser.selected() == 0, "refresh resets selection");
        // Terminal escape bytes in names must never flow into curses output.
        write(location / "unsafe-\x1b[31m.PRG");
        browser.refresh();
        bool sanitized = false;
        for (const auto& entry : browser.entries()) {
            if (entry.path.filename().string().starts_with("unsafe")) {
                sanitized = entry.display_name.find('\x1b') == std::string::npos;
            }
        }
        require(sanitized, "control bytes in filenames were not sanitized");
        require(fs::exists(location / "test.PRG"), "browser must not modify files");
        fs::remove_all(location);
        std::cout << "file browser tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::error_code ignored;
        fs::remove_all(location, ignored);
        std::cerr << "browser failure: " << e.what() << '\n';
        return 1;
    }
}
