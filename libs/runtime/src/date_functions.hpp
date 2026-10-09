// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <xabl/runtime/xabl.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace xabl {

// The date domain is AD 1..9999 plus one explicitly blank date. A blank
// date must never be confused with either an empty string or a zero number.
inline bool date_is_blank(const DateValue& date) noexcept {
    return date.year == 0 && date.month == 0 && date.day == 0;
}

inline bool date_is_valid(const DateValue& date) noexcept {
    using namespace std::chrono;
    return date.year >= 1 && date.year <= 9999 &&
           year_month_day{year{date.year}, month{date.month}, day{date.day}}.ok();
}

// DBF III stores D fields as eight ASCII YYYYMMDD bytes or eight blanks.
inline DateValue parse_dbf_date(std::string_view text) {
    if (text == "        ") return {};
    if (text.size() != 8) throw std::runtime_error("invalid DBF date width");
    for (const unsigned char digit : text) {
        if (digit < '0' || digit > '9')
            throw std::runtime_error("invalid DBF date digits");
    }
    const auto number = [&](std::size_t pos, std::size_t count) {
        int result = 0;
        for (std::size_t i = pos; i < pos + count; ++i) {
            result = result * 10 + (text[i] - '0');
        }
        return result;
    };
    const DateValue date{number(0, 4), static_cast<unsigned>(number(4, 2)),
                         static_cast<unsigned>(number(6, 2))};
    if (!date_is_valid(date)) throw std::runtime_error("invalid DBF calendar date");
    return date;
}

inline std::string encode_dbf_date(const DateValue& date) {
    if (date_is_blank(date)) return std::string(8, ' ');
    if (!date_is_valid(date))
        throw std::runtime_error("invalid date value for DBF field");
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << date.year
           << std::setw(2) << date.month << std::setw(2) << date.day;
    return output.str();
}

// The III PLUS subset currently supports the default American MM/DD/YY
// syntax and MM/DD/YYYY. There is no SET DATE/EPOCH state yet. Original
// III PLUS corrected overflow days within valid months, e.g. 02/29/85
// becomes 03/01/85. Malformed input produces a blank date.
inline DateValue parse_ctod(std::string_view text) {
    if (text.size() != 8 && text.size() != 10) return {};
    if (text[2] != '/' || text[5] != '/') return {};
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 2 || i == 5) continue;
        if (text[i] < '0' || text[i] > '9') return {};
    }
    const auto two = [&](std::size_t i) -> unsigned {
        return static_cast<unsigned>((text[i] - '0') * 10 + (text[i + 1] - '0'));
    };
    const unsigned month = two(0);
    const unsigned day = two(3);
    const int year = text.size() == 8
        ? 1900 + static_cast<int>(two(6))
        : (text[6] - '0') * 1000 + (text[7] - '0') * 100 +
          (text[8] - '0') * 10 + (text[9] - '0');
    if (year < 1 || year > 9999 || month < 1 || month > 12 ||
        day < 1 || day > 31) return {};
    using namespace std::chrono;
    const auto first = sys_days{year_month_day{
        std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{1}}};
    const year_month_day corrected{first + days{day - 1}};
    if (int(corrected.year()) > 9999) return {};
    return {int(corrected.year()), unsigned(corrected.month()),
            unsigned(corrected.day())};
}

inline Value apply_date_function(OpCode opcode, const Value& argument) {
    if (opcode == OpCode::CallCtod) {
        const auto* text = std::get_if<std::string>(&argument.storage());
        if (!text) throw std::runtime_error("CTOD requires a character argument");
        return Value(parse_ctod(*text));
    }
    const auto* date = std::get_if<DateValue>(&argument.storage());
    if (!date) throw std::runtime_error("date function requires a date argument");
    if (!date_is_blank(*date) && !date_is_valid(*date))
        throw std::runtime_error("invalid runtime date value");

    switch (opcode) {
    case OpCode::CallDtos:
        return Value(encode_dbf_date(*date));
    case OpCode::CallDtoc:
        return Value(argument.as_string());
    case OpCode::CallYear:
        return Value(static_cast<double>(date->year));
    case OpCode::CallMonth:
        return Value(static_cast<double>(date->month));
    case OpCode::CallDay:
        return Value(static_cast<double>(date->day));
    default:
        throw std::runtime_error("unsupported date function");
    }
}

} // namespace xabl
