// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include "internal.hpp"
#include "expression.hpp"
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
namespace {
struct Block {
    enum class Kind { If, DoWhile };
    Kind kind;
    std::size_t jump_index;
    std::size_t loop_start;
};

} // namespace

Program Compiler::compile(std::string_view source) const {
    Program program;
    ExpressionCompiler expression_compiler{program};
    std::vector<Block> blocks;

    std::istringstream input{std::string(source)};
    std::string raw_line;
    std::size_t line_number = 0;
    std::string last_locate_expression;

    const auto emit_locate = [&](const std::string& condition, bool continue_search) {
        if (continue_search) {
            program.code.push_back({OpCode::PushLiteral, Value(1.0)});
            program.code.push_back({OpCode::Skip});
        } else {
            program.code.push_back({OpCode::GoTop});
        }

        const std::size_t loop_start = program.code.size();

        program.code.push_back({OpCode::CallEof});
        const std::size_t not_eof_jump = program.code.size();
        program.code.push_back({OpCode::JumpIfFalse});

        program.code.push_back({OpCode::PushLiteral, Value(false)});
        program.code.push_back({OpCode::SetFound});
        const std::size_t eof_jump = program.code.size();
        program.code.push_back({OpCode::Jump});

        const std::size_t test_position = program.code.size();
        program.code[not_eof_jump].target = test_position;

        expression_compiler.emit(condition);
        const std::size_t miss_jump = program.code.size();
        program.code.push_back({OpCode::JumpIfFalse});

        program.code.push_back({OpCode::PushLiteral, Value(true)});
        program.code.push_back({OpCode::SetFound});
        const std::size_t found_jump = program.code.size();
        program.code.push_back({OpCode::Jump});

        const std::size_t miss_position = program.code.size();
        program.code[miss_jump].target = miss_position;

        program.code.push_back({OpCode::PushLiteral, Value(1.0)});
        program.code.push_back({OpCode::Skip});
        program.code.push_back({OpCode::Jump, {}, {}, loop_start});

        const std::size_t end = program.code.size();
        program.code[eof_jump].target = end;
        program.code[found_jump].target = end;
    };

    while (std::getline(input, raw_line)) {
        ++line_number;
        std::string line = trim(raw_line);

        if (line.empty() || line.front() == '*') {
            continue;
        }

        try {
            if (starts_with_ci(line, "USE ")) {
                const std::string remainder = trim(line.substr(4));
                const std::string folded = upper(remainder);
                const std::string marker = " ALIAS ";
                const auto alias_pos = folded.find(marker);

                std::string table_name = remainder;
                std::string alias;
                if (alias_pos != std::string::npos) {
                    table_name = trim(remainder.substr(0, alias_pos));
                    alias = trim(remainder.substr(alias_pos + marker.size()));
                }

                program.code.push_back(
                    {OpCode::OpenTable, Value(alias), basename_without_extension(table_name)});
                continue;
            }

            if (starts_with_ci(line, "SELECT ")) {
                program.code.push_back(
                    {OpCode::SelectArea, {}, trim(line.substr(7))});
                continue;
            }

            if (starts_with_ci(line, "SET INDEX TO ")) {
                program.code.push_back(
                    {OpCode::OpenIndex, {}, basename_without_extension(line.substr(13))});
                continue;
            }

            if (starts_with_ci(line, "SET FILTER TO")) {
                const std::string condition = trim(line.substr(13));
                Instruction instruction{OpCode::SetFilter};

                if (!condition.empty()) {
                    auto filter_program = std::make_shared<Program>();
                    ExpressionCompiler filter_compiler{*filter_program};
                    filter_compiler.emit(condition);
                    filter_program->code.push_back({OpCode::Halt});
                    instruction.embedded_program = std::move(filter_program);
                }

                program.code.push_back(std::move(instruction));
                continue;
            }

            if (equal_ci(line, "SET DELETED ON")) {
                program.code.push_back(
                    {OpCode::SetDeletedVisibility, Value(true)});
                continue;
            }

            if (equal_ci(line, "SET DELETED OFF")) {
                program.code.push_back(
                    {OpCode::SetDeletedVisibility, Value(false)});
                continue;
            }

            if (starts_with_ci(line, "SEEK ")) {
                expression_compiler.emit(line.substr(5));
                program.code.push_back({OpCode::Seek});
                continue;
            }

            if (starts_with_ci(line, "LOCATE FOR ")) {
                last_locate_expression = trim(line.substr(11));
                if (last_locate_expression.empty()) {
                    throw std::runtime_error("LOCATE FOR requires an expression");
                }
                emit_locate(last_locate_expression, false);
                continue;
            }

            if (equal_ci(line, "CONTINUE")) {
                if (last_locate_expression.empty()) {
                    throw std::runtime_error("CONTINUE without a preceding LOCATE FOR");
                }
                emit_locate(last_locate_expression, true);
                continue;
            }

            if (equal_ci(line, "GO TOP")) {
                program.code.push_back({OpCode::GoTop});
                continue;
            }

            if (starts_with_ci(line, "GO ") && !equal_ci(line, "GO TOP")) {
                expression_compiler.emit(line.substr(3));
                program.code.push_back({OpCode::GoRecord});
                continue;
            }

            if (equal_ci(line, "SKIP")) {
                program.code.push_back({OpCode::PushLiteral, Value(1.0)});
                program.code.push_back({OpCode::Skip});
                continue;
            }

            if (starts_with_ci(line, "SKIP ")) {
                expression_compiler.emit(line.substr(5));
                program.code.push_back({OpCode::Skip});
                continue;
            }

            if (equal_ci(line, "DELETE")) {
                program.code.push_back({OpCode::DeleteRecord});
                continue;
            }

            if (equal_ci(line, "RECALL")) {
                program.code.push_back({OpCode::RecallRecord});
                continue;
            }

            if (starts_with_ci(line, "? ")) {
                expression_compiler.emit(line.substr(2));
                program.code.push_back({OpCode::Print});
                continue;
            }

            if (starts_with_ci(line, "STORE ")) {
                const std::string remainder = line.substr(6);
                const std::string marker = " TO ";
                const std::string folded = upper(remainder);
                const auto to_pos = folded.find(marker);
                if (to_pos == std::string::npos) {
                    throw std::runtime_error("STORE requires TO");
                }

                expression_compiler.emit(remainder.substr(0, to_pos));
                const std::string variable =
                    trim(remainder.substr(to_pos + marker.size()));
                if (variable.empty()) {
                    throw std::runtime_error("STORE requires a variable name");
                }
                program.code.push_back({OpCode::StoreName, {}, upper(variable)});
                continue;
            }

            if (starts_with_ci(line, "REPLACE ")) {
                const std::string remainder = line.substr(8);
                const std::string marker = " WITH ";
                const std::string folded = upper(remainder);
                const auto with_pos = folded.find(marker);
                if (with_pos == std::string::npos) {
                    throw std::runtime_error("REPLACE requires WITH");
                }

                const std::string field = trim(remainder.substr(0, with_pos));
                const std::string expression = remainder.substr(with_pos + marker.size());
                expression_compiler.emit(expression);
                program.code.push_back({OpCode::ReplaceField, {}, upper(field)});
                continue;
            }

            if (starts_with_ci(line, "IF ")) {
                expression_compiler.emit(line.substr(3));
                const std::size_t jump_index = program.code.size();
                program.code.push_back({OpCode::JumpIfFalse});
                blocks.push_back({Block::Kind::If, jump_index, 0});
                continue;
            }

            if (equal_ci(line, "ENDIF")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::If) {
                    throw std::runtime_error("ENDIF without matching IF");
                }
                const Block block = blocks.back();
                blocks.pop_back();
                program.code[block.jump_index].target = program.code.size();
                continue;
            }

            if (starts_with_ci(line, "DO WHILE ")) {
                const std::size_t loop_start = program.code.size();
                expression_compiler.emit(line.substr(9));
                const std::size_t jump_index = program.code.size();
                program.code.push_back({OpCode::JumpIfFalse});
                blocks.push_back({Block::Kind::DoWhile, jump_index, loop_start});
                continue;
            }

            if (equal_ci(line, "ENDDO")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::DoWhile) {
                    throw std::runtime_error("ENDDO without matching DO WHILE");
                }
                const Block block = blocks.back();
                blocks.pop_back();
                program.code.push_back({OpCode::Jump, {}, {}, block.loop_start});
                program.code[block.jump_index].target = program.code.size();
                continue;
            }

            const auto assignment = line.find('=');
            if (assignment != std::string::npos) {
                const std::string variable = trim(line.substr(0, assignment));
                const std::string expression = line.substr(assignment + 1);
                if (variable.empty() || expression.empty()) {
                    throw std::runtime_error("invalid assignment");
                }
                expression_compiler.emit(expression);
                program.code.push_back({OpCode::StoreName, {}, upper(variable)});
                continue;
            }

            throw std::runtime_error("unsupported statement: " + line);
        } catch (const std::exception& ex) {
            throw std::runtime_error(
                "line " + std::to_string(line_number) + ": " + ex.what());
        }
    }

    if (!blocks.empty()) {
        throw std::runtime_error("unterminated control-flow block");
    }

    program.code.push_back({OpCode::Halt});
    return program;
}

} // namespace xabl
