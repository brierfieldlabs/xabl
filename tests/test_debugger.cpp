// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/debugger.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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


void test_resumable_vm_execution() {
    const auto program = xabl::Compiler{}.compile(
        "STORE 0 TO n\n"
        "n = n + 1\n"
        "? n\n"
        "n = n + 1\n"
        "? n\n");
    xabl::BreakpointMap breaks(program);
    check(breaks.enable(2) && breaks.enable(4),
          "unable to set test breakpoints");

    std::ostringstream output;
    xabl::Vm vm(output);
    vm.begin(program, ".");
    check(vm.debugging(), "begin did not activate debug session");
    check(vm.debug_location() && vm.debug_location()->line == 1,
          "begin did not locate first statement");
    check(output.str().empty() && vm.variables().empty(),
          "begin must not execute source");

    check(vm.resume(breaks) == xabl::DebugStop::Breakpoint,
          "expected first breakpoint");
    check(vm.debug_location() && vm.debug_location()->line == 2,
          "first breakpoint must halt before line 2");
    check(vm.variables().at("N").as_number() == 0.0 && output.str().empty(),
          "first breakpoint executed the paused statement");

    check(vm.resume(breaks) == xabl::DebugStop::Breakpoint,
          "continuing should bypass the current breakpoint once");
    check(vm.debug_location() && vm.debug_location()->line == 4,
          "second breakpoint must stop at line 4");
    check(vm.variables().at("N").as_number() == 1.0 && output.str() == "1\n",
          "continue repeated or skipped instructions");

    check(vm.step() == xabl::DebugStop::Step,
          "step did not stop at next statement");
    check(vm.debug_location() && vm.debug_location()->line == 5,
          "step must stop before line 5");
    check(vm.variables().at("N").as_number() == 2.0 && output.str() == "1\n",
          "step executed too many statements");

    check(vm.step() == xabl::DebugStop::Finished,
          "last statement and HALT should finish session");
    check(!vm.debugging() && !vm.debug_location() && output.str() == "1\n2\n",
          "finished debug session has wrong output or location");
    try {
        (void)vm.step();
        throw std::runtime_error("stepping completed program was accepted");
    } catch (const std::logic_error&) {
        // A completed debug session cannot silently restart.
    }
}

void test_loop_breakpoints_are_revisited() {
    const auto program = xabl::Compiler{}.compile(
        "STORE 0 TO n\n"
        "DO WHILE n < 3\n"
        "n = n + 1\n"
        "ENDDO\n"
        "? n\n");
    xabl::BreakpointMap breaks(program);
    breaks.enable(3);
    std::ostringstream output;
    xabl::Vm vm(output);
    vm.begin(program, ".");
    for (int n = 0; n != 3; ++n) {
        check(vm.resume(breaks) == xabl::DebugStop::Breakpoint,
              "expected breakpoint on each loop iteration");
        check(vm.debug_location() && vm.debug_location()->line == 3,
              "loop breakpoint stopped on wrong statement");
        check(vm.variables().at("N").as_number() == static_cast<double>(n),
              "loop breakpoint advanced too far");
    }
    check(vm.resume(breaks) == xabl::DebugStop::Finished,
          "loop did not finish after final iteration");
    check(output.str() == "3\n", "loop output changed under debugger");

    // Step across a loop back-edge and stop on the same statement again.
    std::ostringstream second_output;
    xabl::Vm stepped(second_output);
    stepped.begin(program, ".");
    check(stepped.step() == xabl::DebugStop::Step &&
              stepped.debug_location()->line == 2,
          "step did not reach loop condition");
    check(stepped.step() == xabl::DebugStop::Step &&
              stepped.debug_location()->line == 3,
          "step did not reach loop body");
    check(stepped.step() == xabl::DebugStop::Step &&
              stepped.debug_location()->line == 4,
          "step did not reach loop back-edge");
    check(stepped.step() == xabl::DebugStop::Step &&
              stepped.debug_location()->line == 2,
          "step did not return to repeated loop condition");
}

void test_debug_validation_and_error_safety() {
    const auto program = xabl::Compiler{}.compile("? 42\n");
    const auto other = xabl::Compiler{}.compile("? 99\n");
    xabl::BreakpointMap wrong(other);
    xabl::BreakpointMap right(program);
    std::ostringstream output;
    xabl::Vm vm(output);
    vm.begin(program, ".");
    try {
        (void)vm.resume(wrong);
        throw std::runtime_error("foreign breakpoint map accepted");
    } catch (const std::invalid_argument&) {
        check(vm.debugging() && output.str().empty(),
              "foreign map changed execution state");
    }
    check(vm.resume(right) == xabl::DebugStop::Finished,
          "valid map could not resume after rejection");
    check(output.str() == "42\n", "valid program executed incorrectly");

    // No filesystem access or program execution before an explicit step.
    const auto missing_dbf = xabl::Compiler{}.compile(
        "USE xabl_debugger_deliberately_missing_dbf\n? 1\n");
    std::ostringstream dbf_output;
    xabl::Vm dbf_vm(dbf_output);
    dbf_vm.begin(missing_dbf, "/nonexistent/xabl-debugger-directory");
    check(dbf_vm.debugging() && dbf_output.str().empty(),
          "begin accessed a database or emitted output");
    bool failed = false;
    try {
        (void)dbf_vm.step();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    check(failed && !dbf_vm.debugging(),
          "failed instruction should abort resumable execution");

    // Ordinary run() remains compatible with temporary Program arguments.
    std::ostringstream plain_output;
    xabl::Vm plain(plain_output);
    plain.run(xabl::Compiler{}.compile("? 7\n"), ".");
    check(plain_output.str() == "7\n" && !plain.debugging(),
          "legacy run semantics changed");
}


std::string file_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open debugger DBF fixture");
    std::ostringstream content;
    content << stream.rdbuf();
    return content.str();
}

void test_explicit_database_write(const std::filesystem::path& fixtures) {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path directory = fs::temp_directory_path() /
        ("xabl-debugger-" + std::to_string(stamp));
    struct Cleanup {
        fs::path directory;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(directory, ignored);
        }
    } cleanup{directory};
    fs::create_directories(directory);
    fs::copy_file(fixtures / "customers.dbf", directory / "debug.dbf");
    const std::string original = file_bytes(directory / "debug.dbf");

    const auto program = xabl::Compiler{}.compile(
        "USE debug\nAPPEND BLANK\n? RECCOUNT()\nUSE\n");
    xabl::BreakpointMap breakpoints(program);
    breakpoints.enable(2);
    std::ostringstream output;
    xabl::Vm vm(output);

    vm.begin(program, directory);
    check(file_bytes(directory / "debug.dbf") == original,
          "begin mutated DBF before execution");
    check(vm.resume(breakpoints) == xabl::DebugStop::Breakpoint,
          "expected breakpoint before APPEND BLANK");
    check(file_bytes(directory / "debug.dbf") == original,
          "APPEND BLANK ran before the debugger resumed");

    vm.cancel_debugging();
    check(!vm.debugging() && !vm.debug_location(),
          "cancel did not discard paused execution");
    check(file_bytes(directory / "debug.dbf") == original,
          "cancel performed an unauthorised DBF write");

    // Explicitly stepping over APPEND permits exactly one intended write.
    vm.begin(program, directory);
    check(vm.resume(breakpoints) == xabl::DebugStop::Breakpoint,
          "new session did not break before APPEND");
    check(vm.step() == xabl::DebugStop::Step,
          "single step did not execute APPEND");
    check(vm.debug_location() && vm.debug_location()->line == 3,
          "step continued beyond DBF write statement");
    check(xabl::DbfTable(directory / "debug.dbf").reccount() == 4,
          "explicitly stepped APPEND did not persist");
    check(output.str().empty(), "stepping APPEND unexpectedly printed");
    check(vm.resume(breakpoints) == xabl::DebugStop::Finished,
          "could not continue after DBF write");
    check(output.str() == "4\n", "post-append output changed");
    check(file_bytes(fixtures / "customers.dbf") == original,
          "disposable test unexpectedly changed original DBF fixture");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        test_mapping_and_breakpoints();
        test_embedded_program_and_unsourced_bytecode();
        test_resumable_vm_execution();
        test_loop_breakpoints_are_revisited();
        test_debug_validation_and_error_safety();
        test_explicit_database_write(argv[1]);
        std::cout << "source mapping and breakpoint tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
