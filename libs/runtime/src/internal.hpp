// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Private helpers shared by compiler, values, storage, and the VM.
#pragma once
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
namespace xabl {
inline std::string trim(std::string text) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

inline std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

inline bool starts_with_ci(const std::string& text, const std::string& prefix) {
    return upper(text).starts_with(upper(prefix));
}

inline bool equal_ci(const std::string& lhs, const std::string& rhs) {
    return upper(lhs) == upper(rhs);
}

inline std::string unquote(const std::string& token) {
    if (token.size() >= 2 &&
        ((token.front() == '"' && token.back() == '"') ||
         (token.front() == '\'' && token.back() == '\''))) {
        return token.substr(1, token.size() - 2);
    }
    return token;
}

inline std::string basename_without_extension(std::string name) {
    name = trim(std::move(name));
    if (name.empty()) {
        throw std::runtime_error("USE requires a table name");
    }
    return name;
}

inline std::string rtrim_spaces(std::string text) {
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

} // namespace xabl
