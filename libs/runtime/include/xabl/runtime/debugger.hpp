// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <xabl/runtime/xabl.hpp>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace xabl {

/// A source breakpoint resolved against one immutable compiled Program.
struct Breakpoint {
    std::size_t line{};                ///< One-based original source line.
    std::size_t instruction_offset{};  ///< Zero-based index into Program::code.
};

/// Read-only breakpoint resolution, with no VM execution or file accesses.
///
/// Source lines without emitted bytecode are not valid breakpoints. Each
/// breakpoint resolves to the FIRST instruction belonging to that statement,
/// so expression evaluation cannot cause multiple stops on the same line.
/// Rebuild this object whenever source is recompiled; offsets are not stable
/// across program versions. Runtime pause/step controls are not yet provided.
class BreakpointMap {
public:
    explicit BreakpointMap(const Program& program);

    /// Return the first executable bytecode offset on a source line.
    [[nodiscard]] std::optional<std::size_t> instruction_at(
        std::size_t line) const noexcept;

    /// Return all directly executable source lines in ascending order.
    [[nodiscard]] std::vector<std::size_t> executable_lines() const;

    /// Enable an executable line. Throws std::invalid_argument otherwise.
    /// Returns false if it was already enabled.
    bool enable(std::size_t line);

    /// Disable a line. Returns false if no breakpoint was set on it.
    bool disable(std::size_t line) noexcept;

    [[nodiscard]] bool contains(std::size_t line) const noexcept;

    /// True only for the entry instruction of an enabled source statement.
    /// For a future VM debug callback, not used to execute anything here.
    [[nodiscard]] bool should_pause(std::size_t instruction_offset) const noexcept;

    /// Enabled breakpoints sorted by source line.
    [[nodiscard]] std::vector<Breakpoint> enabled() const;

private:
    std::map<std::size_t, std::size_t> source_to_instruction_;
    std::set<std::size_t> active_lines_;
    std::set<std::size_t> active_offsets_;
};

} // namespace xabl
