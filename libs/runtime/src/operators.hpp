// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include "internal.hpp"
#include "date_functions.hpp"
#include <xabl/runtime/xabl.hpp>

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <variant>

namespace xabl {

// Existing numeric arithmetic and classic xBase character concatenation
// share the + and - operators. Both VM execution paths call this helper.
inline Value apply_additive_operator(OpCode opcode, const Value& left,
                                     const Value& right) {
    const auto* left_date = std::get_if<DateValue>(&left.storage());
    const auto* right_date = std::get_if<DateValue>(&right.storage());
    if (left_date || right_date) {
        if (left_date && right_date && opcode == OpCode::Subtract) {
            if (!date_is_valid(*left_date) || !date_is_valid(*right_date))
                throw std::runtime_error("date subtraction requires populated valid dates");
            using namespace std::chrono;
            const auto left_day = sys_days{year_month_day{
                year{left_date->year}, month{left_date->month}, day{left_date->day}}};
            const auto right_day = sys_days{year_month_day{
                year{right_date->year}, month{right_date->month}, day{right_date->day}}};
            return Value(static_cast<double>((left_day - right_day).count()));
        }
        // Original III PLUS allows date +/- integral days. Keep numeric
        // offsets explicitly typed; arbitrary fractions and 1e100 must not
        // be cast to the chrono duration representation.
        if (!left_date || right_date ||
            (opcode != OpCode::Add && opcode != OpCode::Subtract)) {
            throw std::runtime_error("unsupported date arithmetic operands");
        }
        if (!date_is_valid(*left_date))
            throw std::runtime_error("date arithmetic requires a populated valid date");
        const auto* amount = std::get_if<double>(&right.storage());
        if (!amount || !std::isfinite(*amount) || std::trunc(*amount) != *amount ||
            std::fabs(*amount) > 3660000.0) {
            throw std::runtime_error("date arithmetic requires an in-range whole-day offset");
        }
        using namespace std::chrono;
        const auto first = sys_days{year_month_day{
            year{left_date->year}, month{left_date->month}, day{left_date->day}}};
        const auto signed_days = opcode == OpCode::Subtract ? -*amount : *amount;
        const year_month_day result{first + days{static_cast<days::rep>(signed_days)}};
        const DateValue transformed{int(result.year()), unsigned(result.month()),
                                    unsigned(result.day())};
        if (!date_is_valid(transformed))
            throw std::runtime_error("date arithmetic result outside supported calendar");
        return Value(transformed);
    }

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
