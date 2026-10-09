// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include "internal.hpp"
#include <xabl/runtime/xabl.hpp>

#include <cmath>
#include <string>
#include <variant>

namespace xabl {

// Implements the current dBASE III PLUS-compatible string equality subset.
// SET EXACT OFF compares only the right operand's length. SET EXACT ON
// compares both strings ignoring trailing spaces. '==' ignores SET EXACT
// and compares the complete string bytes (other dialects may differ).
inline bool equal_values(const Value& lhs, const Value& rhs,
                         bool exact, bool strictly_equal) {
    if (std::holds_alternative<std::string>(lhs.storage()) ||
        std::holds_alternative<std::string>(rhs.storage())) {
        const std::string left = lhs.as_string();
        const std::string right = rhs.as_string();
        if (strictly_equal) return left == right;
        if (exact) return rtrim_spaces(left) == rtrim_spaces(right);
        return left.size() >= right.size() &&
               left.compare(0, right.size(), right) == 0;
    }
    return std::fabs(lhs.as_number() - rhs.as_number()) < 1e-12;
}

} // namespace xabl
