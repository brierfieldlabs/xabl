// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace xabl::tui {

/// Presentation-only lexical colouring; never used to validate source.
/// Offsets are byte positions, matching the editor's ASCII-first cursor.
enum class SyntaxKind { Plain, Keyword, Function, Number, String, Comment, Logical };
struct SyntaxSpan {
    std::size_t start{};
    std::size_t length{};
    SyntaxKind kind{SyntaxKind::Plain};
    bool operator==(const SyntaxSpan&) const = default;
};

/// Classify one logical source line without interpreting DOS codepages.
/// The lexer respects doubled string quotes and hides comments in strings.
[[nodiscard]] std::vector<SyntaxSpan> highlight_line(std::string_view line);

} // namespace xabl::tui
