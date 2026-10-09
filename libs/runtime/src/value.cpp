// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include "internal.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace xabl {
Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}

bool Value::is_empty() const {
    if (std::holds_alternative<std::monostate>(storage_)) {
        return true;
    }
    if (const auto* text = std::get_if<std::string>(&storage_)) {
        return text->empty();
    }
    return false;
}

bool Value::as_logical() const {
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value;
    }
    if (const auto* value = std::get_if<double>(&storage_)) {
        return *value != 0.0;
    }
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        return !value->empty();
    }
    return false;
}

double Value::as_number() const {
    if (const auto* value = std::get_if<double>(&storage_)) {
        return *value;
    }
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value ? 1.0 : 0.0;
    }
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        const std::string cleaned = trim(*value);
        if (cleaned.empty()) {
            return 0.0;
        }
        std::size_t consumed = 0;
        const double converted = std::stod(cleaned, &consumed);
        if (consumed != cleaned.size()) {
            throw std::runtime_error("value is not numeric: " + *value);
        }
        return converted;
    }
    return 0.0;
}

std::string Value::as_string() const {
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        return *value;
    }
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value ? ".T." : ".F.";
    }
    if (const auto* value = std::get_if<double>(&storage_)) {
        std::ostringstream out;
        out << std::setprecision(15) << *value;
        return out.str();
    }
    return "";
}

const Value::Storage& Value::storage() const noexcept {
    return storage_;
}

} // namespace xabl
