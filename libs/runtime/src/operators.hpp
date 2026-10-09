// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include "internal.hpp"
#include <xabl/runtime/xabl.hpp>

#include <stdexcept>
#include <string>
#include <variant>

namespace xabl {

// Existing numeric arithmetic and classic xBase character concatenation
// share the + and - operators. Both VM execution paths call this helper.
inline Value apply_additive_operator(OpCode opcode, const Value& left,
                                     const Value& right) {
    const auto* lhs = std::get_if<std::string>(&left.storage());
    const auto* rhs = std::get_if<std::string>(&right.storage());
    if (lhs || rhs) {
        if (!lhs || !rhs) {
            throw std::runtime_error(
                "mixed character/numeric operands require explicit conversion");
        }
        if (opcode == OpCode::Add) {
            return Value(*lhs + *rhs);
        }
        if (opcode == OpCode::Subtract) {
            // Traditional xBase '-' concatenates but shifts the trailing
            // blank padding of the left operand behind the right operand.
            const std::string trimmed = rtrim_spaces(*lhs);
            return Value(trimmed + *rhs +
                         std::string(lhs->size() - trimmed.size(), ' '));
        }
        throw std::runtime_error("invalid character additive opcode");
    }
    if (opcode == OpCode::Add) {
        return Value(left.as_number() + right.as_number());
    }
    if (opcode == OpCode::Subtract) {
        return Value(left.as_number() - right.as_number());
    }
    throw std::runtime_error("unsupported additive opcode");
}

} // namespace xabl
