// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include "syntax_highlight.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace xabl::tui {
namespace {
bool alpha(unsigned char c) { return (c >= 'a' && c <= 'z') ||
                                    (c >= 'A' && c <= 'Z') || c == '_'; }
bool digit(unsigned char c) { return c >= '0' && c <= '9'; }
bool ident(unsigned char c) { return alpha(c) || digit(c); }
std::string upper(std::string_view source) {
    std::string result(source);
    for (auto& c : result) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    return result;
}

constexpr std::array<std::string_view, 40> keywords{
    "AND", "APPEND", "BLANK", "BOF", "BY", "CLOSE", "CONTINUE", "DELETE",
    "DELETED", "DO", "ELSE", "ENDDO", "ENDIF", "EOF", "EXACT", "FALSE",
    "FILTER", "FOR", "FOUND", "GO", "GOTO", "IF", "INDEX", "LOCATE",
    "NOT", "OFF", "ON", "OR", "RECALL", "REPLACE", "SELECT", "SEEK",
    "SET", "SKIP", "TO", "TOP", "TRUE", "USE", "WHILE", "WITH"};
constexpr std::array<std::string_view, 33> functions{
    "ABS", "ASC", "AT", "BOF", "CHR", "CTOD", "DAY", "DELETED", "DTOC",
    "DTOS", "EOF", "FOUND", "INT", "LEFT", "LEN", "LOWER", "LTRIM",
    "MAX", "MIN", "MONTH", "RECNO", "RECCOUNT", "REPLICATE", "RIGHT",
    "RTRIM", "SPACE", "STR", "STUFF", "SUBSTR", "TRIM", "UPPER", "VAL", "YEAR"};

bool has_keyword(std::string_view word) {
    return std::find(keywords.begin(), keywords.end(), word) != keywords.end();
}
bool has_function(std::string_view word) {
    return std::find(functions.begin(), functions.end(), word) != functions.end();
}
}

std::vector<SyntaxSpan> highlight_line(std::string_view source) {
    std::vector<SyntaxSpan> spans;
    auto add = [&](std::size_t start, std::size_t end, SyntaxKind kind) {
        if (end > start) spans.push_back({start, end - start, kind});
    };
    const auto size = source.size();
    std::size_t first = source.find_first_not_of(" \t");
    if (first == std::string_view::npos) return spans;
    // A leading * is a comment command only when it is the first
    // nonblank character, not when it is the multiplication operator.
    if (source[first] == '*' ||
        (size - first >= 2 && source.substr(first, 2) == "&&") ||
        (size - first >= 4 && upper(source.substr(first, 4)) == "NOTE" &&
         (first + 4 == size || !ident(source[first + 4])))) {
        add(first, size, SyntaxKind::Comment);
        return spans;
    }
    for (std::size_t i = 0; i < size;) {
        const unsigned char c = static_cast<unsigned char>(source[i]);
        if (c == '&' && i + 1 < size && source[i + 1] == '&') {
            add(i, size, SyntaxKind::Comment);
            break;
        }
        if (c == '\'' || c == '"') {
            const char delimiter = static_cast<char>(c);
            const auto begin = i++;
            while (i < size) {
                if (source[i++] == delimiter) {
                    if (i < size && source[i] == delimiter) {
                        ++i; // doubled quote escaped inside literal
                    } else {
                        break;
                    }
                }
            }
            add(begin, i, SyntaxKind::String);
            continue;
        }
        if (c == '.' && i + 2 < size &&
            (source[i + 1] == 't' || source[i + 1] == 'T' ||
             source[i + 1] == 'f' || source[i + 1] == 'F') && source[i + 2] == '.') {
            add(i, i + 3, SyntaxKind::Logical);
            i += 3;
            continue;
        }
        if (alpha(c)) {
            const auto begin = i++;
            while (i < size && ident(static_cast<unsigned char>(source[i]))) ++i;
            const std::string word = upper(source.substr(begin, i - begin));
            if (has_function(word) && i < size && source[i] == '(') {
                add(begin, i, SyntaxKind::Function);
            } else if (has_keyword(word)) {
                add(begin, i, SyntaxKind::Keyword);
            }
            continue;
        }
        if (digit(c) || (c == '.' && i + 1 < size &&
                         digit(static_cast<unsigned char>(source[i + 1])))) {
            const auto begin = i;
            while (i < size && digit(static_cast<unsigned char>(source[i]))) ++i;
            if (i < size && source[i] == '.') {
                ++i;
                while (i < size && digit(static_cast<unsigned char>(source[i]))) ++i;
            }
            if (i < size && (source[i] == 'e' || source[i] == 'E')) {
                auto cursor = i + 1;
                if (cursor < size && (source[cursor] == '+' || source[cursor] == '-')) ++cursor;
                if (cursor < size && digit(static_cast<unsigned char>(source[cursor]))) {
                    i = cursor + 1;
                    while (i < size && digit(static_cast<unsigned char>(source[i]))) ++i;
                }
            }
            add(begin, i, SyntaxKind::Number);
            continue;
        }
        ++i;
    }
    return spans;
}
} // namespace xabl::tui
