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

// dBASE III PLUS source comments are stripped before command recognition.
// A doubled matching quote is part of a string, never a comment delimiter.
std::string without_inline_comment(std::string_view source) {
    char quote = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const char c = source[i];
        if (quote != 0) {
            if (c == quote) {
                if (i + 1 < source.size() && source[i + 1] == quote) {
                    ++i; // doubled delimiters escape the quote
                } else {
                    quote = 0;
                }
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '&' && i + 1 < source.size() && source[i + 1] == '&') {
            return std::string(source.substr(0, i));
        }
    }
    return std::string(source);
}

// A continued statement is one logical source line, even when split across
// physical PRG lines. Keep its original start position for debugger mapping.
struct LogicalSourceLine {
    std::string text;
    std::size_t line{0};
    std::size_t column{0};
};

// Find semicolons outside both quoted-string styles, respecting doubled
// quote escapes. Only terminal continuation is implemented for now; the
// first earlier separator must be rejected instead of silently misparsed.
std::size_t first_unquoted_semicolon(std::string_view source) {
    char quote = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const char c = source[i];
        if (quote != 0) {
            if (c == quote) {
                if (i + 1 < source.size() && source[i + 1] == quote) {
                    ++i;
                } else {
                    quote = 0;
                }
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == ';') {
            return i;
        }
    }
    return std::string_view::npos;
}

bool read_statement(std::istream& input, std::size_t& physical_line,
                    LogicalSourceLine& result) {
    result = {};
    std::string raw;
    bool continued = false;
    while (std::getline(input, raw)) {
        ++physical_line;
        std::string part = without_inline_comment(raw);
        const std::string stripped = trim(part);
        // An ignored full-line comment is not part of a continuation.
        if (stripped.empty() || stripped.front() == '*' ||
            equal_ci(stripped, "NOTE") || starts_with_ci(stripped, "NOTE ")) {
            continue;
        }
        if (!continued) {
            result.line = physical_line;
            const auto first = std::find_if_not(
                raw.begin(), raw.end(), [](unsigned char c) {
                    return std::isspace(c) != 0;
                });
            result.column = static_cast<std::size_t>(
                std::distance(raw.begin(), first)) + 1;
        }

        const auto last = part.find_last_not_of(" \t\r\n");
        const bool more = last != std::string::npos &&
                          part[last] == ';' &&
                          first_unquoted_semicolon(part.substr(0, last + 1)) == last;
        if (more) part.erase(last, 1);
        result.text += part;
        if (!more) {
            result.text = trim(result.text);
            if (first_unquoted_semicolon(result.text) != std::string::npos) {
                throw std::runtime_error("line " + std::to_string(result.line) +
                                         ": same-line semicolon statement separators "
                                         "are not yet supported");
            }
            return true;
        }
        continued = true;
    }
    if (continued) {
        throw std::runtime_error("line " + std::to_string(result.line) +
                                 ": unfinished semicolon continuation");
    }
    return false;
}

// STORE and bare assignments write memory variables, not arbitrary source
// fragments. Match the simple names accepted by the expression lexer.
// Historical length/codepage limits belong in a later dialect-specific pass.
bool valid_variable_name(std::string_view name) {
    if (name.empty()) return false;
    const auto letter = [](unsigned char c) {
        return std::isalpha(c) != 0 || c == '_';
    };
    const auto rest = [&](unsigned char c) {
        return letter(c) || std::isdigit(c) != 0;
    };
    return letter(static_cast<unsigned char>(name.front())) &&
           std::all_of(name.begin() + 1, name.end(), rest);
}

// Locate command separators such as " TO " without mistaking matching
// words inside strings or nested parenthesised function expressions.
std::size_t find_command_marker(std::string_view source,
                                std::string_view marker) {
    char quote = 0;
    std::size_t nesting = 0;
    for (std::size_t i = 0; i + marker.size() <= source.size(); ++i) {
        const char c = source[i];
        if (quote != 0) {
            if (c == quote) {
                if (i + 1 < source.size() && source[i + 1] == quote) {
                    ++i;
                } else {
                    quote = 0;
                }
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '(') { ++nesting; continue; }
        if (c == ')') {
            if (nesting != 0) --nesting;
            continue;
        }
        if (nesting == 0 &&
            upper(std::string(source.substr(i, marker.size()))) == marker) {
            return i;
        }
    }
    return std::string::npos;
}

// Stamp all instructions appended by one source statement, including those
// emitted by the expression compiler. A scope guard is necessary because each
// recognised dBASE command exits the parser's line loop with `continue`.
struct StatementSourceScope {
    Program& program;
    std::size_t first;
    SourceLocation source;

    ~StatementSourceScope() {
        for (std::size_t i = first; i < program.code.size(); ++i) {
            program.code[i].source = source;
        }
    }
};

struct Block {
    enum class Kind { If, DoWhile, DoCase };
    Kind kind;
    std::size_t jump_index;
    std::size_t loop_start;
    bool has_else{false};
    std::vector<std::size_t> exit_jumps{};
    bool has_case{false};
};

} // namespace

Compiler::Compiler(CompatibilityProfile profile) : profile_(profile) {
    profile_.require_implemented();
}

Program Compiler::compile(std::string_view source) const {
    Program program;
    program.dialect = profile_.dialect;
    ExpressionCompiler expression_compiler{program};
    std::vector<Block> blocks;

    std::istringstream input{std::string(source)};
    LogicalSourceLine source_line;
    std::size_t physical_line = 0;
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

    while (read_statement(input, physical_line, source_line)) {
        const std::string& line = source_line.text;
        try {
            StatementSourceScope statement{program, program.code.size(),
                                           {source_line.line, source_line.column}};
            if (equal_ci(line, "USE")) {
                program.code.push_back({OpCode::CloseTable});
                continue;
            }

            if (equal_ci(line, "SET INDEX TO")) {
                program.code.push_back({OpCode::CloseIndex});
                continue;
            }

            if (equal_ci(line, "APPEND BLANK")) {
                program.code.push_back({OpCode::AppendBlank});
                continue;
            }

            if (equal_ci(line, "GO BOTTOM") || equal_ci(line, "GOTO BOTTOM")) {
                program.code.push_back({OpCode::GoBottom});
                continue;
            }

            if (starts_with_ci(line, "USE ")) {
                const std::string remainder = trim(line.substr(4));
                const std::string marker = " ALIAS ";
                const auto alias_pos = find_command_marker(remainder, marker);

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
                    filter_program->dialect = profile_.dialect;
                    ExpressionCompiler filter_compiler{*filter_program};
                    filter_compiler.emit(condition);
                    for (auto& emitted : filter_program->code) {
                        emitted.source = {source_line.line, source_line.column};
                    }
                    filter_program->code.push_back({OpCode::Halt});
                    instruction.embedded_program = std::move(filter_program);
                }

                program.code.push_back(std::move(instruction));
                continue;
            }

            if (equal_ci(line, "SET EXACT ON")) {
                program.code.push_back({OpCode::SetExact, Value(true)});
                continue;
            }

            if (equal_ci(line, "SET EXACT OFF")) {
                program.code.push_back({OpCode::SetExact, Value(false)});
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

            if (equal_ci(line, "GO TOP") || equal_ci(line, "GOTO TOP")) {
                program.code.push_back({OpCode::GoTop});
                continue;
            }

            if (starts_with_ci(line, "GO ") || starts_with_ci(line, "GOTO ")) {
                expression_compiler.emit(
                    line.substr(starts_with_ci(line, "GOTO ") ? 5 : 3));
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

            if (line.starts_with("??")) {
                // DOS-era ?? writes on the current line, unlike ?.
                // This subset accepts one expression, or an empty ??.
                const std::string expression = trim(line.substr(2));
                if (expression.empty()) {
                    program.code.push_back(
                        {OpCode::PushLiteral, Value(std::string{})});
                } else {
                    expression_compiler.emit(expression);
                }
                program.code.push_back({OpCode::PrintInline});
                continue;
            }

            if (line.front() == '?') {
                if (line == "?") {
                    // A bare ? emits a blank output line.
                    program.code.push_back(
                        {OpCode::PushLiteral, Value(std::string{})});
                } else {
                    expression_compiler.emit(trim(line.substr(1)));
                }
                program.code.push_back({OpCode::Print});
                continue;
            }

            if (starts_with_ci(line, "STORE ")) {
                const std::string remainder = line.substr(6);
                const std::string marker = " TO ";
                const auto to_pos = find_command_marker(remainder, marker);
                if (to_pos == std::string::npos) {
                    throw std::runtime_error("STORE requires TO");
                }

                expression_compiler.emit(remainder.substr(0, to_pos));
                const std::string variable =
                    trim(remainder.substr(to_pos + marker.size()));
                if (!valid_variable_name(variable)) {
                    throw std::runtime_error("STORE requires one valid variable name");
                }
                program.code.push_back({OpCode::StoreName, {}, upper(variable)});
                continue;
            }

            if (starts_with_ci(line, "REPLACE ")) {
                const std::string remainder = line.substr(8);
                const std::string marker = " WITH ";
                const auto with_pos = find_command_marker(remainder, marker);
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

            if (equal_ci(line, "ELSE")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::If ||
                    blocks.back().has_else) {
                    throw std::runtime_error("ELSE without unmatched IF");
                }
                const std::size_t end_jump = program.code.size();
                program.code.push_back({OpCode::Jump});
                program.code[blocks.back().jump_index].target = program.code.size();
                blocks.back().jump_index = end_jump;
                blocks.back().has_else = true;
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

            if (equal_ci(line, "DO CASE")) {
                blocks.push_back({Block::Kind::DoCase, 0, 0});
                continue;
            }

            if (equal_ci(line, "CASE") || starts_with_ci(line, "CASE ")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::DoCase ||
                    blocks.back().has_else) {
                    throw std::runtime_error("CASE without open DO CASE");
                }
                const std::string condition = trim(line.substr(4));
                if (condition.empty()) {
                    throw std::runtime_error("CASE requires a condition");
                }
                Block& block = blocks.back();
                if (block.has_case) {
                    // A successful prior branch skips all later branches;
                    // its false condition jumps past this unconditional jump.
                    block.exit_jumps.push_back(program.code.size());
                    program.code.push_back({OpCode::Jump});
                    program.code[block.jump_index].target = program.code.size();
                }
                expression_compiler.emit(condition);
                block.jump_index = program.code.size();
                program.code.push_back({OpCode::JumpIfFalse});
                block.has_case = true;
                continue;
            }

            if (equal_ci(line, "OTHERWISE")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::DoCase ||
                    !blocks.back().has_case || blocks.back().has_else) {
                    throw std::runtime_error("OTHERWISE without unmatched CASE");
                }
                Block& block = blocks.back();
                block.exit_jumps.push_back(program.code.size());
                program.code.push_back({OpCode::Jump});
                program.code[block.jump_index].target = program.code.size();
                block.has_else = true;
                continue;
            }

            if (equal_ci(line, "ENDCASE")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::DoCase ||
                    !blocks.back().has_case) {
                    throw std::runtime_error("ENDCASE without matching CASE");
                }
                const Block block = blocks.back();
                blocks.pop_back();
                const std::size_t end = program.code.size();
                if (!block.has_else) {
                    program.code[block.jump_index].target = end;
                }
                for (const auto exit_jump : block.exit_jumps) {
                    program.code[exit_jump].target = end;
                }
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

            if (equal_ci(line, "LOOP") || equal_ci(line, "EXIT")) {
                const auto loop = std::find_if(
                    blocks.rbegin(), blocks.rend(), [](const Block& block) {
                        return block.kind == Block::Kind::DoWhile;
                    });
                if (loop == blocks.rend()) {
                    throw std::runtime_error(line + " outside DO WHILE block");
                }
                if (equal_ci(line, "LOOP")) {
                    // Re-evaluate the loop condition on each continue.
                    program.code.push_back(
                        {OpCode::Jump, {}, {}, loop->loop_start});
                } else {
                    loop->exit_jumps.push_back(program.code.size());
                    program.code.push_back({OpCode::Jump});
                }
                continue;
            }

            if (equal_ci(line, "ENDDO")) {
                if (blocks.empty() || blocks.back().kind != Block::Kind::DoWhile) {
                    throw std::runtime_error("ENDDO without matching DO WHILE");
                }
                const Block block = blocks.back();
                blocks.pop_back();
                program.code.push_back({OpCode::Jump, {}, {}, block.loop_start});
                const std::size_t end = program.code.size();
                program.code[block.jump_index].target = end;
                for (const std::size_t exit_jump : block.exit_jumps) {
                    program.code[exit_jump].target = end;
                }
                continue;
            }

            const auto assignment = line.find('=');
            if (assignment != std::string::npos) {
                const std::string variable = trim(line.substr(0, assignment));
                const std::string expression = line.substr(assignment + 1);
                if (!valid_variable_name(variable) || trim(expression).empty()) {
                    throw std::runtime_error("invalid assignment target or expression");
                }
                expression_compiler.emit(expression);
                program.code.push_back({OpCode::StoreName, {}, upper(variable)});
                continue;
            }

            throw std::runtime_error("unsupported statement: " + line);
        } catch (const std::exception& ex) {
            throw std::runtime_error(
                "line " + std::to_string(source_line.line) + ": " + ex.what());
        }
    }

    if (!blocks.empty()) {
        throw std::runtime_error("unterminated control-flow block");
    }

    program.code.push_back({OpCode::Halt});
    return program;
}

} // namespace xabl
