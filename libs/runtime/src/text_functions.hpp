// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include "internal.hpp"
#include <xabl/runtime/xabl.hpp>

#include <algorithm>
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

} // namespace xabl
