// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include "internal.hpp"
#include <xabl/runtime/xabl.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <variant>

namespace xabl {

// Implements the current dBASE III PLUS-compatible string equality subset.
// SET EXACT OFF compares only the right operand's length. SET EXACT ON
// compares both strings ignoring trailing spaces. '==' ignores SET EXACT
// and compares the complete string bytes (other dialects may differ).
inline bool equal_values(const Value& lhs, const Value& rhs,
                         bool exact, bool strictly_equal) {
    const auto* left_date = std::get_if<DateValue>(&lhs.storage());
    const auto* right_date = std::get_if<DateValue>(&rhs.storage());
    if (left_date || right_date) {
        if (!left_date || !right_date)
            throw std::runtime_error("date comparison type mismatch");
        if (left_date->year == 0 || right_date->year == 0)
            throw std::runtime_error("blank date comparison needs explicit handling");
        return *left_date == *right_date;
    }
    const auto* lhs_text = std::get_if<std::string>(&lhs.storage());
    const auto* rhs_text = std::get_if<std::string>(&rhs.storage());
    if (lhs_text || rhs_text) {
        if (!lhs_text || !rhs_text) {
            throw std::runtime_error("character/numeric comparison type mismatch");
        }
        const std::string& left = *lhs_text;
        const std::string& right = *rhs_text;
        if (strictly_equal) return left == right;
        if (exact) return rtrim_spaces(left) == rtrim_spaces(right);
        return left.size() >= right.size() &&
               left.compare(0, right.size(), right) == 0;
    }
    return std::fabs(lhs.as_number() - rhs.as_number()) < 1e-12;
}

// Character comparisons use the byte ordering of the stored legacy
// encoding for now. Do not reinterpret decimal-looking strings as numbers.
// This is intentionally ASCII/bytewise rather than locale-collated.
inline bool ordered_values(OpCode opcode, const Value& lhs, const Value& rhs) {
    const auto* left_date = std::get_if<DateValue>(&lhs.storage());
    const auto* right_date = std::get_if<DateValue>(&rhs.storage());
    if (left_date || right_date) {
        if (!left_date || !right_date)
            throw std::runtime_error("date comparison type mismatch");
        if (left_date->year == 0 || right_date->year == 0)
            throw std::runtime_error("blank date comparison needs explicit handling");
        const auto lhs_key = left_date->year * 10000 + left_date->month * 100 + left_date->day;
        const auto rhs_key = right_date->year * 10000 + right_date->month * 100 + right_date->day;
        if (opcode == OpCode::Less) return lhs_key < rhs_key;
        if (opcode == OpCode::Greater) return lhs_key > rhs_key;
        throw std::runtime_error("unsupported date relational opcode");
    }
    const auto* lhs_text = std::get_if<std::string>(&lhs.storage());
    const auto* rhs_text = std::get_if<std::string>(&rhs.storage());
    if (lhs_text || rhs_text) {
        if (!lhs_text || !rhs_text) {
            throw std::runtime_error("character/numeric comparison type mismatch");
        }
        const auto byte_less = [](unsigned char a, unsigned char b) {
            return a < b;
        };
        if (opcode == OpCode::Less) {
            return std::lexicographical_compare(
                lhs_text->begin(), lhs_text->end(),
                rhs_text->begin(), rhs_text->end(), byte_less);
        }
        if (opcode == OpCode::Greater) {
            return std::lexicographical_compare(
                rhs_text->begin(), rhs_text->end(),
                lhs_text->begin(), lhs_text->end(), byte_less);
        }
    } else {
        if (opcode == OpCode::Less) return lhs.as_number() < rhs.as_number();
        if (opcode == OpCode::Greater) return lhs.as_number() > rhs.as_number();
    }
    throw std::runtime_error("unsupported relational opcode");
}

// The classic $ operator asks whether the first character expression is
// contained anywhere inside the second. An empty needle never matches.
// It is bytewise and independent of SET EXACT.
inline bool contained_in(const Value& needle, const Value& haystack) {
    const auto* requested = std::get_if<std::string>(&needle.storage());
    const auto* searched = std::get_if<std::string>(&haystack.storage());
    if (!requested || !searched) {
        throw std::runtime_error("$ substring operator requires character operands");
    }
    return !requested->empty() && searched->find(*requested) != std::string::npos;
}

} // namespace xabl
