// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <cstdint>
#include <string_view>

namespace xabl {

/// Explicit legacy-language dialect identifiers reserved by the roadmap.
/// These names do not imply the corresponding compatibility pack is ready.
/// At present only DBaseIIIPlus is executable.
enum class CompatibilityDialect : std::uint8_t {
    DBaseIIIPlus,
    DBaseIV,
    Clipper,
    FoxPro,
    VisualFoxPro,
    VisualDBase,
    DBasePlus,
    Harbour,
    XHarbour,
    Modern
};

/// Value object recording the chosen language dialect for a compiler/VM.
/// Constructing a profile is permitted for all planned dialects, but clients
/// MUST call require_implemented() before executing a selected dialect.
struct CompatibilityProfile {
    CompatibilityDialect dialect{CompatibilityDialect::DBaseIIIPlus};

    /// Parse canonical names such as "dbase-iii-plus"; reject unknown names.
    [[nodiscard]] static CompatibilityProfile parse(std::string_view name);
    /// Canonical stable CLI identifier (or "invalid" for an unknown enum).
    [[nodiscard]] std::string_view name() const noexcept;
    /// True only when this dialect has a tested executable implementation.
    [[nodiscard]] bool implemented() const noexcept;
    /// Fail closed rather than silently substituting dBASE III PLUS.
    void require_implemented() const;
};

} // namespace xabl
