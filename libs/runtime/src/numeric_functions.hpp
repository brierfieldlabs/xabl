// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <xabl/runtime/xabl.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace xabl {

// VAL() reads the leading numeric prefix after ASCII blanks. A character
// argument not beginning with a number returns zero, unlike as_number(),
// which must reject an invalid complete numeric value.
inline Value apply_val_function(const Value& argument) {
    const auto* value = std::get_if<std::string>(&argument.storage());
    if (!value) throw std::runtime_error("VAL requires a character argument");
    const char* start = value->c_str();
    while (*start == ' ') ++start;
    const char* first = start;
    if (*first == '+' || *first == '-') ++first;
    const bool has_digits = (*first >= '0' && *first <= '9') ||
        (*first == '.' && first[1] >= '0' && first[1] <= '9');
    if (!has_digits) return Value(0.0);
    char* end = nullptr;
    const double number = std::strtod(start, &end);
    if (end == start) return Value(0.0);
    if (!std::isfinite(number)) {
        throw std::runtime_error("VAL numeric result exceeds runtime range");
    }
    return Value(number);
}

constexpr std::size_t str_output_limit = 1024 * 1024;
constexpr std::size_t str_decimal_limit = 18;

inline std::size_t str_integer_argument(const Value& value, const char* description,
                                        std::size_t minimum,
                                        std::size_t maximum) {
    const auto* number = std::get_if<double>(&value.storage());
    if (!number || !std::isfinite(*number) ||
        *number < static_cast<double>(minimum) ||
        *number > static_cast<double>(maximum)) {
        throw std::runtime_error(std::string("STR ") + description +
                                 " is outside supported range");
    }
    return static_cast<std::size_t>(std::trunc(*number));
}

inline Value apply_str_function(const Value& number_arg,
                                const Value* width_arg = nullptr,
                                const Value* decimal_arg = nullptr) {
    const auto* number = std::get_if<double>(&number_arg.storage());
    if (!number || !std::isfinite(*number)) {
        throw std::runtime_error("STR requires a finite numeric value");
    }
    const auto width = width_arg
        ? str_integer_argument(*width_arg, "width", 1, str_output_limit)
        : std::size_t{10};
    const auto decimals = decimal_arg
        ? str_integer_argument(*decimal_arg, "decimals", 0, str_decimal_limit)
        : std::size_t{0};
    // Original III PLUS rejects a width incapable of even a single
    // digit, decimal point and requested decimal places. It reserves
    // all-asterisk overflow for actual values too large for a valid
    // formatting specification.
    if (decimals != 0 && width < decimals + 2) {
        throw std::runtime_error("STR width cannot contain requested decimals");
    }

    std::ostringstream buffer;
    buffer.imbue(std::locale::classic());
    buffer << std::fixed << std::setprecision(static_cast<int>(decimals)) << *number;
    const auto digits = buffer.str();
    if (digits.size() > width) {
        return Value(std::string(width, '*'));
    }
    return Value(std::string(width - digits.size(), ' ') + digits);
}

} // namespace xabl
