// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include "internal.hpp"
#include <xabl/runtime/xabl.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <cctype>
#include <stdexcept>
#include <string>
#include <variant>

namespace xabl {

// Single-argument character function semantics shared between VM evaluation
// and embedded SET FILTER bytecode. Keep the operations byte-oriented for
// the current legacy DBF codepage subset; Unicode requires a later profile.
inline Value apply_text_function(OpCode opcode, const Value& argument) {
    const auto* value = std::get_if<std::string>(&argument.storage());
    if (!value) {
        throw std::runtime_error("character function requires a string argument");
    }
    std::string result = *value;
    switch (opcode) {
    case OpCode::CallLen:
        return Value(static_cast<double>(result.size()));
    case OpCode::CallUpper:
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(c >= 'a' && c <= 'z' ?
                                                    c - ('a' - 'A') : c);
                       });
        return Value(std::move(result));
    case OpCode::CallLower:
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(c >= 'A' && c <= 'Z' ?
                                                    c + ('a' - 'A') : c);
                       });
        return Value(std::move(result));
    case OpCode::CallTrim:
        return Value(rtrim_spaces(std::move(result)));
    case OpCode::CallLTrim:
        result.erase(result.begin(), std::find_if(result.begin(), result.end(),
            [](char c) { return c != ' '; }));
        return Value(std::move(result));
    default:
        throw std::runtime_error("unsupported character function opcode");
    }
}

// Byte-oriented legacy string extraction. Numeric positions are truncated
// toward zero, but are range-checked before converting to an index.
inline Value apply_slice_function(OpCode opcode, const Value& string_arg,
                                  const Value& position_arg,
                                  const Value* count_arg = nullptr) {
    const auto* characters = std::get_if<std::string>(&string_arg.storage());
    const auto* position = std::get_if<double>(&position_arg.storage());
    if (!characters || !position || !std::isfinite(*position)) {
        throw std::runtime_error("string slice requires string and numeric arguments");
    }
    const auto count_to_size = [](double value, std::size_t maximum) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("string slice count must be finite");
        }
        if (value <= 0) return std::size_t{0};
        if (value >= static_cast<double>(maximum)) return maximum;
        return static_cast<std::size_t>(std::trunc(value));
    };
    const auto& text = *characters;
    if (opcode == OpCode::CallLeft) {
        if (count_arg) throw std::runtime_error("LEFT takes two arguments");
        return Value(text.substr(0, count_to_size(*position, text.size())));
    }
    if (opcode == OpCode::CallRight) {
        if (count_arg) throw std::runtime_error("RIGHT takes two arguments");
        const auto count = count_to_size(*position, text.size());
        return Value(text.substr(text.size() - count));
    }
    if (opcode == OpCode::CallSubstr) {
        if (*position < 1) {
            throw std::runtime_error("SUBSTR position must be one-based and positive");
        }
        if (*position > static_cast<double>(text.size())) return Value(std::string{});
        const auto start = static_cast<std::size_t>(std::trunc(*position)) - 1;
        std::size_t count = text.size() - start;
        if (count_arg) {
            const auto* numeric = std::get_if<double>(&count_arg->storage());
            if (!numeric) throw std::runtime_error("SUBSTR length must be numeric");
            count = count_to_size(*numeric, count);
        }
        return Value(text.substr(start, count));
    }
    throw std::runtime_error("unsupported string slice opcode");
}

// AT(needle, haystack) returns the one-based position of the first
// case-sensitive byte sequence, or zero when no occurrence exists.
inline Value apply_at_function(const Value& needle, const Value& haystack) {
    const auto* wanted = std::get_if<std::string>(&needle.storage());
    const auto* target = std::get_if<std::string>(&haystack.storage());
    if (!wanted || !target) {
        throw std::runtime_error("AT requires two character arguments");
    }
    if (wanted->empty() || target->empty()) return Value(0.0);
    const auto offset = target->find(*wanted);
    return Value(offset == std::string::npos
                     ? 0.0
                     : static_cast<double>(offset + 1));
}

} // namespace xabl
