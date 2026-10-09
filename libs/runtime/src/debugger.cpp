// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/debugger.hpp>
#include <stdexcept>

namespace xabl {

BreakpointMap::BreakpointMap(const Program& program) {
    for (std::size_t i = 0; i < program.code.size(); ++i) {
        const Instruction& instruction = program.code[i];
        if (instruction.opcode != OpCode::Halt && instruction.source.line != 0) {
            // emplace keeps the first instruction on the source line.
            source_to_instruction_.emplace(instruction.source.line, i);
        }
    }
}

std::optional<std::size_t> BreakpointMap::instruction_at(
    std::size_t line) const noexcept {
    const auto found = source_to_instruction_.find(line);
    if (found == source_to_instruction_.end()) return std::nullopt;
    return found->second;
}

std::vector<std::size_t> BreakpointMap::executable_lines() const {
    std::vector<std::size_t> lines;
    lines.reserve(source_to_instruction_.size());
    for (const auto& [line, offset] : source_to_instruction_) {
        (void)offset;
        lines.push_back(line);
    }
    return lines;
}

bool BreakpointMap::enable(std::size_t line) {
    const auto found = source_to_instruction_.find(line);
    if (found == source_to_instruction_.end()) {
        throw std::invalid_argument("no executable instruction at source line " +
                                    std::to_string(line));
    }
    if (!active_lines_.insert(line).second) return false;
    active_offsets_.insert(found->second);
    return true;
}

bool BreakpointMap::disable(std::size_t line) noexcept {
    if (active_lines_.erase(line) == 0) return false;
    active_offsets_.erase(source_to_instruction_.find(line)->second);
    return true;
}

bool BreakpointMap::contains(std::size_t line) const noexcept {
    return active_lines_.contains(line);
}

bool BreakpointMap::should_pause(std::size_t instruction_offset) const noexcept {
    return active_offsets_.contains(instruction_offset);
}

std::vector<Breakpoint> BreakpointMap::enabled() const {
    std::vector<Breakpoint> lines;
    lines.reserve(active_lines_.size());
    for (const auto line : active_lines_) {
        lines.push_back({line, source_to_instruction_.at(line)});
    }
    return lines;
}

} // namespace xabl
