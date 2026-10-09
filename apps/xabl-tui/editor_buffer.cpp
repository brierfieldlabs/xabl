// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include "editor_buffer.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace xabl::tui {
namespace {
constexpr std::size_t maximum_source_bytes = 1024 * 1024;
constexpr std::size_t maximum_undo_steps = 40;
}

EditorBuffer::EditorBuffer() = default;

void EditorBuffer::read_text(std::string_view source) {
    if (source.size() > maximum_source_bytes ||
        source.find('\0') != std::string_view::npos) {
        throw std::runtime_error("source is too large or contains binary NUL bytes");
    }
    std::vector<std::string> parsed;
    const auto first_newline = source.find('\n');
    const auto detected = first_newline != std::string_view::npos &&
                          first_newline > 0 && source[first_newline - 1] == '\r'
        ? "\r\n" : "\n";
    std::size_t pos = 0;
    for (;;) {
        const auto next = source.find('\n', pos);
        if (next == std::string_view::npos) {
            parsed.emplace_back(source.substr(pos));
            break;
        }
        std::size_t end = next;
        if (end > pos && source[end - 1] == '\r') --end;
        parsed.emplace_back(source.substr(pos, end - pos));
        pos = next + 1;
    }
    lines_ = std::move(parsed);
    eol_ = detected;
    cursor_ = {};
    dirty_ = false;
    undo_.clear();
}

void EditorBuffer::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open source file: " + path.string());
    // Limit input before allocating enough memory for the whole source.
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) > maximum_source_bytes) {
        throw std::runtime_error("source file exceeds 1 MiB editor limit");
    }
    input.seekg(0);
    std::string source(static_cast<std::size_t>(size), '\0');
    input.read(source.data(), static_cast<std::streamsize>(source.size()));
    if (input.gcount() != static_cast<std::streamsize>(source.size()))
        throw std::runtime_error("source file changed during read");
    read_text(source); // Do not discard the previous document on a read failure.
    path_ = path;
}

void EditorBuffer::save(const std::filesystem::path& path) {
    if (path.empty()) throw std::runtime_error("save requires a filename");
    if (std::filesystem::is_symlink(path)) {
        throw std::runtime_error("save refuses to replace a symbolic link");
    }
    if (std::filesystem::exists(path) &&
        !std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("save target is not a regular source file");
    }
    const auto source = text();
    // Write a sibling file and replace only after a successful close. A
    // failed write must not truncate the last good .PRG file.
    std::filesystem::path candidate;
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 16; ++attempt) {
        candidate = path.string() + ".xabl-tui-" +
                    std::to_string(nonce) + "-" + std::to_string(attempt);
        if (!std::filesystem::exists(candidate)) break;
        if (attempt == 15) throw std::runtime_error("cannot create temporary save");
    }
    try {
        std::ofstream output(candidate, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot stage source file");
        output.write(source.data(), static_cast<std::streamsize>(source.size()));
        output.close();
        if (!output) throw std::runtime_error("failed writing staged source");
        if (std::filesystem::exists(path)) {
            std::filesystem::permissions(candidate,
                                         std::filesystem::status(path).permissions());
        }
        std::filesystem::rename(candidate, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(candidate, ignored);
        throw;
    }
    path_ = path;
    dirty_ = false;
}

void EditorBuffer::clear() {
    lines_ = {""};
    path_.clear();
    cursor_ = {};
    eol_ = "\n";
    dirty_ = false;
    undo_.clear();
}

std::string EditorBuffer::text() const {
    std::string result;
    result.reserve(content_size());
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (i) result += eol_;
        result += lines_[i];
    }
    return result;
}

std::size_t EditorBuffer::content_size() const {
    std::size_t total = eol_.size() * (lines_.size() - 1);
    for (const auto& line : lines_) total += line.size();
    return total;
}

const std::vector<std::string>& EditorBuffer::lines() const noexcept { return lines_; }
EditorBuffer::Cursor EditorBuffer::cursor() const noexcept { return cursor_; }
const std::filesystem::path& EditorBuffer::path() const noexcept { return path_; }
bool EditorBuffer::dirty() const noexcept { return dirty_; }

void EditorBuffer::set_cursor(std::size_t row, std::size_t column) {
    cursor_.row = std::min(row, lines_.size() - 1);
    cursor_.column = std::min(column, lines_[cursor_.row].size());
}

void EditorBuffer::move_left() {
    if (cursor_.column) --cursor_.column;
    else if (cursor_.row) {
        --cursor_.row;
        cursor_.column = lines_[cursor_.row].size();
    }
}

void EditorBuffer::move_right() {
    if (cursor_.column < lines_[cursor_.row].size()) ++cursor_.column;
    else if (cursor_.row + 1 < lines_.size()) {
        ++cursor_.row;
        cursor_.column = 0;
    }
}

void EditorBuffer::move_up() {
    if (cursor_.row) set_cursor(cursor_.row - 1, cursor_.column);
}

void EditorBuffer::move_down() {
    if (cursor_.row + 1 < lines_.size()) set_cursor(cursor_.row + 1, cursor_.column);
}

void EditorBuffer::home() { cursor_.column = 0; }
void EditorBuffer::end() { cursor_.column = lines_[cursor_.row].size(); }

void EditorBuffer::remember() {
    if (undo_.size() == maximum_undo_steps) undo_.erase(undo_.begin());
    undo_.push_back({lines_, cursor_});
}

void EditorBuffer::insert(char character) {
    insert_text(std::string_view(&character, 1));
}

void EditorBuffer::insert_text(std::string_view characters) {
    if (characters.empty()) return;
    if (characters.find('\n') != std::string_view::npos ||
        characters.find('\r') != std::string_view::npos ||
        characters.find('\0') != std::string_view::npos ||
        characters.size() > maximum_source_bytes - content_size()) {
        throw std::runtime_error("invalid or oversized text insertion");
    }
    remember();
    lines_[cursor_.row].insert(cursor_.column, characters);
    cursor_.column += characters.size();
    dirty_ = true;
}

void EditorBuffer::newline() {
    if (content_size() + eol_.size() > maximum_source_bytes) {
        throw std::runtime_error("source exceeds editor limit");
    }
    remember();
    auto& line = lines_[cursor_.row];
    std::string tail = line.substr(cursor_.column);
    line.erase(cursor_.column);
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(cursor_.row + 1),
                  std::move(tail));
    ++cursor_.row;
    cursor_.column = 0;
    dirty_ = true;
}

void EditorBuffer::backspace() {
    if (cursor_.column) {
        remember();
        lines_[cursor_.row].erase(--cursor_.column, 1);
        dirty_ = true;
    } else if (cursor_.row) {
        remember();
        const auto previous = lines_[cursor_.row - 1].size();
        lines_[cursor_.row - 1] += lines_[cursor_.row];
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(cursor_.row));
        --cursor_.row;
        cursor_.column = previous;
        dirty_ = true;
    }
}

void EditorBuffer::erase() {
    if (cursor_.column < lines_[cursor_.row].size()) {
        remember();
        lines_[cursor_.row].erase(cursor_.column, 1);
        dirty_ = true;
    } else if (cursor_.row + 1 < lines_.size()) {
        remember();
        lines_[cursor_.row] += lines_[cursor_.row + 1];
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(cursor_.row + 1));
        dirty_ = true;
    }
}

bool EditorBuffer::undo() {
    if (undo_.empty()) return false;
    auto previous = std::move(undo_.back());
    undo_.pop_back();
    lines_ = std::move(previous.lines);
    cursor_ = previous.cursor;
    dirty_ = true;
    return true;
}

bool EditorBuffer::find_next(std::string_view requested) {
    if (requested.empty()) return false;
    for (std::size_t offset = 0; offset < lines_.size(); ++offset) {
        const auto row = (cursor_.row + offset) % lines_.size();
        const auto start = offset == 0 ? std::min(cursor_.column + 1, lines_[row].size()) : 0;
        const auto found = lines_[row].find(requested, start);
        if (found != std::string::npos) {
            set_cursor(row, found);
            return true;
        }
    }
    // Wrap once within the current line, before the original cursor.
    const auto found = lines_[cursor_.row].find(requested);
    if (found != std::string::npos && found <= cursor_.column) {
        cursor_.column = found;
        return true;
    }
    return false;
}

} // namespace xabl::tui
