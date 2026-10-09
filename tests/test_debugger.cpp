// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/debugger.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void check(bool condition, std::string_view explanation) {
    if (!condition) throw std::runtime_error(std::string(explanation));
}

void test_mapping_and_breakpoints() {
    const auto program = xabl::Compiler{}.compile(
        "  * comment\r\n"
        "\r\n"
        "   STORE 1 + 2 TO n  && trailing comment\r\n"
        "  IF n = 3\r\n"
        "    ? n * 2\r\n"
        "  ELSE\r\n"
        "    ? 0\r\n"
        "  ENDIF\r\n"
        "  DO WHILE n < 4\r\n"
        "     n = n + 1\r\n"
        "  ENDDO\r\n");

    check(!program.code.empty(), "compiler emitted no instructions");
    check(program.code.back().opcode == xabl::OpCode::Halt,
          "missing halt instruction");
    check(program.code.back().source == xabl::SourceLocation{},
          "synthetic halt must have no source location");

    const auto lines = std::vector<std::size_t>{3, 4, 5, 6, 7, 9, 10, 11};
    const xabl::BreakpointMap map(program);
    check(map.executable_lines() == lines, "source line set mismatch");
    check(!map.instruction_at(1) && !map.instruction_at(2) &&
              !map.instruction_at(8) && !map.instruction_at(12),
          "non-executable lines should have no bytecode");
    check(!map.instruction_at(0), "unknown source line 0 is not executable");

    for (std::size_t line : lines) {
        const auto start = map.instruction_at(line);
        check(start.has_value(), "executable line missing bytecode");
        check(program.code[*start].source.line == line,
              "resolved instruction points at wrong source line");
        if (*start != 0) {
            check(program.code[*start - 1].source.line != line,
                  "breakpoint should be on first instruction of a statement");
        }
    }
    check(program.code[*map.instruction_at(3)].source.column == 4,
          "source column must reflect original indentation");
    check(program.code[*map.instruction_at(5)].source.column == 5,
          "indented source column incorrect");
    check(program.code[*map.instruction_at(6)].opcode == xabl::OpCode::Jump,
          "ELSE should be mapped to its emitted jump");
    check(program.code[*map.instruction_at(11)].opcode == xabl::OpCode::Jump,
          "ENDDO should be mapped to its back-edge");
    check(program.code[*map.instruction_at(9)].source.line == 9,
          "loop condition entry source line incorrect");

    auto active = xabl::BreakpointMap(program);
    check(active.enabled().empty(), "breakpoints must begin disabled");
    check(active.enable(5), "enabling first breakpoint failed");
    check(!active.enable(5), "duplicate enable should be idempotent");
    check(active.enable(3), "enabling second breakpoint failed");
    check(active.contains(3) && active.contains(5), "enabled state lost");
    check(active.enabled().size() == 2 && active.enabled()[0].line == 3 &&
              active.enabled()[1].line == 5,
          "enabled breakpoints should be sorted");
    check(active.should_pause(*map.instruction_at(3)),
          "first instruction should trigger pause");
    check(active.should_pause(*map.instruction_at(5)),
          "second breakpoint should trigger pause");
    check(!active.should_pause(*map.instruction_at(5) + 1),
          "expression-internal instructions must not cause repeated pauses");
    check(!active.should_pause(program.code.size() - 1),
          "HALT must not cause a breakpoint");
    check(active.disable(3) && !active.disable(3),
          "disabling an absent breakpoint must be idempotent");
    check(!active.should_pause(*map.instruction_at(3)),
          "disabled breakpoint must not pause");
    for (auto invalid : {0u, 1u, 8u, 99u}) {
        try {
            (void)active.enable(invalid);
            throw std::runtime_error("non-executable line accepted as breakpoint");
        } catch (const std::invalid_argument&) {
            // Expected: unsupported line remains disabled.
        }
    }
    check(active.enabled().size() == 1 && active.enabled()[0].line == 5,
          "invalid requests must not mutate breakpoint state");
}

void test_embedded_program_and_unsourced_bytecode() {
    const auto program = xabl::Compiler{}.compile(
        "SET FILTER TO n > 1\n? 7\n");
    check(program.code[0].opcode == xabl::OpCode::SetFilter,
          "filter entry instruction missing");
    const auto embedded = program.code[0].embedded_program;
    check(embedded && embedded->code.size() > 1,
          "SET FILTER must carry an embedded expression program");
    for (std::size_t i = 0; i + 1 < embedded->code.size(); ++i) {
        check(embedded->code[i].source.line == 1,
              "embedded filter instructions must retain source origin");
    }
    check(embedded->code.back().source.line == 0,
          "embedded HALT should have no source origin");
    const xabl::BreakpointMap map(program);
    check(map.instruction_at(1) == 0, "filter must be breakable as one statement");
    check(map.instruction_at(2).has_value(), "following line should be breakable");

    xabl::Program hand_built;
    hand_built.code.push_back({xabl::OpCode::PushLiteral});
    hand_built.code.push_back({xabl::OpCode::Halt});
    xabl::BreakpointMap unknown(hand_built);
    check(unknown.executable_lines().empty(), "unsourced bytecode is not breakable");
}

} // namespace

int main() {
    try {
        test_mapping_and_breakpoints();
        test_embedded_program_and_unsourced_bytecode();
        std::cout << "source mapping and breakpoint tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
