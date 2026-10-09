// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include "syntax_highlight.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using xabl::tui::SyntaxKind;
using xabl::tui::SyntaxSpan;
namespace {
void check(std::string_view source, std::vector<SyntaxSpan> expected) {
    auto actual = xabl::tui::highlight_line(source);
    if (actual != expected) throw std::runtime_error("highlight mismatch: " + std::string(source));
}
}
int main() {
    try {
        check("  * a comment", {{2, 11, SyntaxKind::Comment}});
        check("NOTE example", {{0, 12, SyntaxKind::Comment}});
        check("NOTED = 1", {{8, 1, SyntaxKind::Number}});
        check("* 100", {{0, 5, SyntaxKind::Comment}});
        check("? '&& not a comment' && true", {{2, 18, SyntaxKind::String}, {21, 7, SyntaxKind::Comment}});
        check("? 'He said ''Hi'''", {{2, 16, SyntaxKind::String}});
        check("if .T. AND age >= 21", {{0, 2, SyntaxKind::Keyword}, {3, 3, SyntaxKind::Logical},
                                        {7, 3, SyntaxKind::Keyword}, {18, 2, SyntaxKind::Number}});
        check("? DTOS(CTOD('02/29/84'))", {{2, 4, SyntaxKind::Function},
              {7, 4, SyntaxKind::Function}, {12, 10, SyntaxKind::String}});
        check("? 3.14e-2 + .5", {{2, 7, SyntaxKind::Number}, {12, 2, SyntaxKind::Number}});
        check("? 5 * 2", {{2, 1, SyntaxKind::Number}, {6, 1, SyntaxKind::Number}});
        check("   ", {});
        check("? 'unfinished", {{2, 11, SyntaxKind::String}});
        std::cout << "syntax-highlighting tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
