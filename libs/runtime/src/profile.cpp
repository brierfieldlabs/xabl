// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/profile.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace xabl {
namespace {
constexpr std::array<std::string_view, 10> names = {
    "dbase-iii-plus", "dbase-iv", "clipper", "foxpro",
    "visual-foxpro", "visual-dbase", "dbase-plus", "harbour",
    "xharbour", "modern"
};
} // namespace

CompatibilityProfile CompatibilityProfile::parse(std::string_view name) {
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (name == names[index]) {
            return {static_cast<CompatibilityDialect>(index)};
        }
    }
    throw std::invalid_argument("unknown compatibility profile: " +
                                std::string(name));
}

std::string_view CompatibilityProfile::name() const noexcept {
    const auto index = static_cast<std::size_t>(dialect);
    return index < names.size() ? names[index] : "invalid";
}

bool CompatibilityProfile::implemented() const noexcept {
    return dialect == CompatibilityDialect::DBaseIIIPlus;
}

void CompatibilityProfile::require_implemented() const {
    if (!implemented()) {
        throw std::runtime_error("compatibility profile not implemented: " +
                                 std::string(name()));
    }
}
} // namespace xabl
