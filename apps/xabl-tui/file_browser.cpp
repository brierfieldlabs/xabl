// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include "file_browser.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;
namespace xabl::tui {
namespace {
constexpr std::size_t maximum_entries = 4096;

std::string ascii_upper(std::string text) {
    for (char& value : text) {
        if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 32);
    }
    return text;
}
bool recognised(const fs::path& path) {
    const auto ext = ascii_upper(path.extension().string());
    return ext == ".PRG" || ext == ".XABL";
}
std::string safe_display(std::string label) {
    // Terminal file names may contain escape/control bytes. Never send
    // them to curses as raw display text.
    for (char& character : label) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 32 || byte == 127) character = '?';
    }
    return label;
}
}

FileBrowser::FileBrowser(fs::path starting_directory)
    : directory_(fs::absolute(std::move(starting_directory)).lexically_normal()) {
    refresh();
}
const fs::path& FileBrowser::directory() const noexcept { return directory_; }
const std::vector<FileBrowser::Entry>& FileBrowser::entries() const noexcept {
    return entries_;
}
std::size_t FileBrowser::selected() const noexcept { return selected_; }
void FileBrowser::move_up() noexcept { if (selected_) --selected_; }
void FileBrowser::move_down() noexcept {
    if (selected_ + 1 < entries_.size()) ++selected_;
}
void FileBrowser::select(std::size_t index) noexcept {
    if (!entries_.empty()) selected_ = std::min(index, entries_.size() - 1);
}
void FileBrowser::refresh() {
    std::vector<Entry> next;
    const auto parent = directory_.parent_path();
    if (!parent.empty() && parent != directory_) {
        next.push_back({parent, "[..]  Parent directory", true});
    }
    std::error_code ec;
    fs::directory_iterator iterator(directory_, fs::directory_options::skip_permission_denied, ec);
    if (ec) throw fs::filesystem_error("cannot browse directory", directory_, ec);
    const fs::directory_iterator end;
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) throw fs::filesystem_error("directory listing interrupted", directory_, ec);
        const auto& item = *iterator;
        auto status = item.symlink_status(ec);
        if (ec) { ec.clear(); continue; }
        // Skip symlinks and non-regular files to avoid accidental browsing
        // outside intended directories or opening device files.
        if (fs::is_symlink(status)) continue;
        const bool folder = fs::is_directory(status);
        if (!folder && !(fs::is_regular_file(status) && recognised(item.path()))) continue;
        if (next.size() >= maximum_entries) {
            throw std::runtime_error("too many entries in source directory");
        }
        const auto original = item.path().filename().string();
        next.push_back({item.path(), (folder ? "[DIR] " : "      ") +
                        safe_display(original), folder});
    }
    if (ec) throw fs::filesystem_error("directory listing failed", directory_, ec);
    std::sort(next.begin(), next.end(), [](const Entry& a, const Entry& b) {
        if (a.display_name.starts_with("[..]")) return true;
        if (b.display_name.starts_with("[..]")) return false;
        if (a.directory != b.directory) return a.directory;
        return ascii_upper(a.display_name) < ascii_upper(b.display_name);
    });
    entries_ = std::move(next);
    selected_ = 0;
}
void FileBrowser::parent() {
    const auto next = directory_.parent_path();
    if (!next.empty() && next != directory_) {
        const auto previous = directory_;
        directory_ = next;
        try { refresh(); }
        catch (...) { directory_ = previous; throw; }
    }
}
fs::path FileBrowser::activate() {
    if (entries_.empty()) return {};
    const Entry chosen = entries_.at(selected_);
    if (chosen.directory) {
        const auto previous = directory_;
        directory_ = chosen.path;
        try { refresh(); }
        catch (...) { directory_ = previous; throw; }
        return {};
    }
    // Recheck at selection time, before the source editor reads the path.
    const auto status = fs::symlink_status(chosen.path);
    if (!fs::is_regular_file(status) || fs::is_symlink(status) || !recognised(chosen.path))
        throw std::runtime_error("selected source file is no longer regular");
    return chosen.path;
}
} // namespace xabl::tui
