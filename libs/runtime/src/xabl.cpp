// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs

#include <xabl/runtime/xabl.hpp>

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

std::string trim(std::string text) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

bool starts_with_ci(const std::string& text, const std::string& prefix) {
    return upper(text).starts_with(upper(prefix));
}

bool equal_ci(const std::string& lhs, const std::string& rhs) {
    return upper(lhs) == upper(rhs);
}

std::string unquote(const std::string& token) {
    if (token.size() >= 2 &&
        ((token.front() == '"' && token.back() == '"') ||
         (token.front() == '\'' && token.back() == '\''))) {
        return token.substr(1, token.size() - 2);
    }
    return token;
}

struct ExpressionCompiler {
    Program& program;

    void emit(std::string expression) {
        expression = trim(std::move(expression));
        if (expression.empty()) {
            throw std::runtime_error("empty expression");
        }

        while (has_enclosing_parentheses(expression)) {
            expression = trim(expression.substr(1, expression.size() - 2));
            if (expression.empty()) {
                throw std::runtime_error("empty parenthesised expression");
            }
        }

        const std::string folded = upper(expression);

        if (expression.size() > 1 && expression.front() == '-') {
            program.code.push_back({OpCode::PushLiteral, Value(0.0)});
            emit(expression.substr(1));
            program.code.push_back({OpCode::Subtract});
            return;
        }

        if (expression.size() > 1 && expression.front() == '+') {
            emit(expression.substr(1));
            return;
        }

        if (folded.starts_with(".NOT.")) {
            emit(expression.substr(5));
            program.code.push_back({OpCode::UnaryNot});
            return;
        }

        if (folded.starts_with("NOT ")) {
            emit(expression.substr(4));
            program.code.push_back({OpCode::UnaryNot});
            return;
        }

        for (const std::string op : {".OR.", " OR "}) {
            const auto pos = find_operator(folded, op);
            if (pos != std::string::npos) {
                emit(expression.substr(0, pos));
                emit(expression.substr(pos + op.size()));
                program.code.push_back({OpCode::LogicalOr});
                return;
            }
        }

        for (const std::string op : {".AND.", " AND "}) {
            const auto pos = find_operator(folded, op);
            if (pos != std::string::npos) {
                emit(expression.substr(0, pos));
                emit(expression.substr(pos + op.size()));
                program.code.push_back({OpCode::LogicalAnd});
                return;
            }
        }

        for (const std::string op : {">=", "<=", "<>", "!=", "==", ">", "<", "="}) {
            const auto pos = find_operator(expression, op);
            if (pos != std::string::npos) {
                emit(expression.substr(0, pos));
                emit(expression.substr(pos + op.size()));

                if (op == ">") {
                    program.code.push_back({OpCode::Greater});
                } else if (op == "<") {
                    program.code.push_back({OpCode::Less});
                } else if (op == "=" || op == "==") {
                    program.code.push_back({OpCode::Equal});
                } else if (op == "!=" || op == "<>") {
                    program.code.push_back({OpCode::Equal});
                    program.code.push_back({OpCode::UnaryNot});
                } else if (op == ">=") {
                    program.code.push_back({OpCode::Less});
                    program.code.push_back({OpCode::UnaryNot});
                } else if (op == "<=") {
                    program.code.push_back({OpCode::Greater});
                    program.code.push_back({OpCode::UnaryNot});
                }
                return;
            }
        }

        for (const std::string op : {"+", "-"}) {
            const auto pos = find_last_operator(expression, op);
            if (pos != std::string::npos) {
                emit(expression.substr(0, pos));
                emit(expression.substr(pos + op.size()));
                program.code.push_back(
                    {op == "+" ? OpCode::Add : OpCode::Subtract});
                return;
            }
        }

        for (const std::string op : {"*", "/"}) {
            const auto pos = find_last_operator(expression, op);
            if (pos != std::string::npos) {
                emit(expression.substr(0, pos));
                emit(expression.substr(pos + op.size()));
                program.code.push_back(
                    {op == "*" ? OpCode::Multiply : OpCode::Divide});
                return;
            }
        }

        if (folded == "EOF()" || folded == "EOF( )") {
            program.code.push_back({OpCode::CallEof});
            return;
        }

        if (folded == "BOF()" || folded == "BOF( )") {
            program.code.push_back({OpCode::CallBof});
            return;
        }

        if (folded == "FOUND()" || folded == "FOUND( )") {
            program.code.push_back({OpCode::CallFound});
            return;
        }

        if (folded == "RECNO()" || folded == "RECNO( )") {
            program.code.push_back({OpCode::CallRecno});
            return;
        }

        if (folded == "RECCOUNT()" || folded == "RECCOUNT( )") {
            program.code.push_back({OpCode::CallReccount});
            return;
        }

        if (folded == "DELETED()" || folded == "DELETED( )") {
            program.code.push_back({OpCode::CallDeleted});
            return;
        }

        if (folded == ".T." || folded == "TRUE") {
            program.code.push_back({OpCode::PushLiteral, Value(true)});
            return;
        }

        if (folded == ".F." || folded == "FALSE") {
            program.code.push_back({OpCode::PushLiteral, Value(false)});
            return;
        }

        if ((expression.front() == '"' && expression.back() == '"') ||
            (expression.front() == '\'' && expression.back() == '\'')) {
            program.code.push_back({OpCode::PushLiteral, Value(unquote(expression))});
            return;
        }

        char* end = nullptr;
        const double number = std::strtod(expression.c_str(), &end);
        if (end != nullptr && *end == '\0') {
            program.code.push_back({OpCode::PushLiteral, Value(number)});
            return;
        }

        program.code.push_back({OpCode::LoadName, {}, upper(expression)});
    }

private:
    static bool has_enclosing_parentheses(const std::string& expression) {
        if (expression.size() < 2 || expression.front() != '(' ||
            expression.back() != ')') {
            return false;
        }

        bool quoted = false;
        char quote = 0;
        int depth = 0;

        for (std::size_t i = 0; i < expression.size(); ++i) {
            const char c = expression[i];

            if ((c == '"' || c == '\'') && (i == 0 || expression[i - 1] != '\\')) {
                if (!quoted) {
                    quoted = true;
                    quote = c;
                } else if (quote == c) {
                    quoted = false;
                }
                continue;
            }

            if (quoted) {
                continue;
            }

            if (c == '(') {
                ++depth;
            } else if (c == ')') {
                --depth;
                if (depth == 0 && i != expression.size() - 1) {
                    return false;
                }
                if (depth < 0) {
                    return false;
                }
            }
        }

        return depth == 0;
    }

    static std::size_t find_operator(const std::string& expression, const std::string& op) {
        bool quoted = false;
        char quote = 0;
        int depth = 0;

        for (std::size_t i = 0; i + op.size() <= expression.size(); ++i) {
            const char c = expression[i];
            if ((c == '"' || c == '\'') && (i == 0 || expression[i - 1] != '\\')) {
                if (!quoted) {
                    quoted = true;
                    quote = c;
                } else if (quote == c) {
                    quoted = false;
                }
                continue;
            }

            if (quoted) {
                continue;
            }

            if (c == '(') {
                ++depth;
                continue;
            }
            if (c == ')') {
                --depth;
                continue;
            }

            if (depth == 0 && expression.compare(i, op.size(), op) == 0) {
                if (op == ">" && i > 0 && expression[i - 1] == '-') {
                    continue;
                }
                if (op == "-" && i + 1 < expression.size() && expression[i + 1] == '>') {
                    continue;
                }
                return i;
            }
        }

        return std::string::npos;
    }

    static std::size_t find_last_operator(
        const std::string& expression, const std::string& op) {
        bool quoted = false;
        char quote = 0;
        int depth = 0;
        std::size_t found = std::string::npos;

        for (std::size_t i = 0; i + op.size() <= expression.size(); ++i) {
            const char c = expression[i];
            if ((c == '"' || c == '\'') && (i == 0 || expression[i - 1] != '\\')) {
                if (!quoted) {
                    quoted = true;
                    quote = c;
                } else if (quote == c) {
                    quoted = false;
                }
                continue;
            }

            if (quoted) {
                continue;
            }

            if (c == '(') {
                ++depth;
                continue;
            }
            if (c == ')') {
                --depth;
                continue;
            }

            if (depth == 0 && expression.compare(i, op.size(), op) == 0) {
                if (op == ">" && i > 0 && expression[i - 1] == '-') {
                    continue;
                }
                if (op == "-" && i + 1 < expression.size() && expression[i + 1] == '>') {
                    continue;
                }
                found = i;
            }
        }

        return found;
    }
};

struct Block {
    enum class Kind { If, DoWhile };
    Kind kind;
    std::size_t jump_index;
    std::size_t loop_start;
};

std::string basename_without_extension(std::string name) {
    name = trim(std::move(name));
    if (name.empty()) {
        throw std::runtime_error("USE requires a table name");
    }
    return name;
}

std::string rtrim_spaces(std::string text) {
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
    return text;
}

} // namespace

Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}

bool Value::is_empty() const {
    if (std::holds_alternative<std::monostate>(storage_)) {
        return true;
    }
    if (const auto* text = std::get_if<std::string>(&storage_)) {
        return text->empty();
    }
    return false;
}

bool Value::as_logical() const {
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value;
    }
    if (const auto* value = std::get_if<double>(&storage_)) {
        return *value != 0.0;
    }
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        return !value->empty();
    }
    return false;
}

double Value::as_number() const {
    if (const auto* value = std::get_if<double>(&storage_)) {
        return *value;
    }
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value ? 1.0 : 0.0;
    }
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        const std::string cleaned = trim(*value);
        if (cleaned.empty()) {
            return 0.0;
        }
        std::size_t consumed = 0;
        const double converted = std::stod(cleaned, &consumed);
        if (consumed != cleaned.size()) {
            throw std::runtime_error("value is not numeric: " + *value);
        }
        return converted;
    }
    return 0.0;
}

std::string Value::as_string() const {
    if (const auto* value = std::get_if<std::string>(&storage_)) {
        return *value;
    }
    if (const auto* value = std::get_if<bool>(&storage_)) {
        return *value ? ".T." : ".F.";
    }
    if (const auto* value = std::get_if<double>(&storage_)) {
        std::ostringstream out;
        out << std::setprecision(15) << *value;
        return out.str();
    }
    return "";
}

const Value::Storage& Value::storage() const noexcept {
    return storage_;
}

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

DbfTable::DbfTable(std::filesystem::path path) : path_(std::move(path)) {
    load();
}

bool DbfTable::eof() const noexcept {
    return !before_first_ && current_ >= records_.size();
}

bool DbfTable::bof() const noexcept {
    return before_first_;
}

bool DbfTable::deleted() const {
    if (before_first_ || eof()) {
        return false;
    }
    return !records_[current_].empty() && records_[current_][0] == '*';
}

std::size_t DbfTable::recno() const noexcept {
    if (before_first_) {
        return 0;
    }
    if (eof()) {
        return records_.size() + 1;
    }
    return current_ + 1;
}

std::size_t DbfTable::reccount() const noexcept {
    return records_.size();
}

void DbfTable::go_top() noexcept {
    before_first_ = false;
    current_ = 0;
}

void DbfTable::skip(std::ptrdiff_t count) noexcept {
    if (count == 0) {
        return;
    }

    const std::ptrdiff_t current_position = before_first_
        ? -1
        : static_cast<std::ptrdiff_t>(current_);
    const std::ptrdiff_t target = current_position + count;

    if (target < 0) {
        before_first_ = true;
        current_ = 0;
        return;
    }

    before_first_ = false;
    const auto unsigned_target = static_cast<std::size_t>(target);
    current_ = std::min(unsigned_target, records_.size());
}

Value DbfTable::field(const std::string& name) const {
    if (eof()) {
        return {};
    }

    const Field& field_info = find_field(name);
    const auto& record = records_[current_];
    const std::string raw(record.begin() + static_cast<std::ptrdiff_t>(field_info.offset),
                          record.begin() + static_cast<std::ptrdiff_t>(field_info.offset + field_info.length));

    if (field_info.type == 'N' || field_info.type == 'F') {
        const std::string cleaned = trim(raw);
        return Value(cleaned.empty() ? 0.0 : std::stod(cleaned));
    }

    if (field_info.type == 'L') {
        const char c = raw.empty() ? 'F' : static_cast<char>(std::toupper(raw.front()));
        return Value(c == 'T' || c == 'Y');
    }

    return Value(rtrim_spaces(raw));
}

void DbfTable::go_record(std::size_t one_based_record_number) {
    if (one_based_record_number == 0) {
        before_first_ = true;
        current_ = 0;
        return;
    }
    before_first_ = false;
    if (one_based_record_number > records_.size()) {
        current_ = records_.size();
        return;
    }

    current_ = one_based_record_number - 1;
}

void DbfTable::set_deleted(bool deleted_state) {
    if (before_first_ || eof()) {
        throw std::runtime_error("DELETE/RECALL attempted outside a record");
    }

    records_[current_][0] = deleted_state ? '*' : ' ';
    flush_record(current_);
}

void DbfTable::replace(const std::string& name, const Value& value) {
    if (eof()) {
        throw std::runtime_error("REPLACE attempted at EOF");
    }

    const Field& field_info = find_field(name);
    std::string encoded;

    if (field_info.type == 'N' || field_info.type == 'F') {
        std::ostringstream out;
        out << std::fixed << std::setprecision(static_cast<int>(field_info.decimals))
            << value.as_number();
        encoded = out.str();
        if (encoded.size() > field_info.length) {
            throw std::runtime_error("numeric value too wide for DBF field " + field_info.name);
        }
        encoded.insert(encoded.begin(), field_info.length - encoded.size(), ' ');
    } else if (field_info.type == 'L') {
        encoded = value.as_logical() ? "T" : "F";
        encoded.resize(field_info.length, ' ');
    } else {
        encoded = value.as_string();
        if (encoded.size() > field_info.length) {
            encoded.resize(field_info.length);
        } else {
            encoded.resize(field_info.length, ' ');
        }
    }

    auto& record = records_[current_];
    std::copy(encoded.begin(), encoded.end(),
              record.begin() + static_cast<std::ptrdiff_t>(field_info.offset));
    flush_record(current_);
}

void DbfTable::load() {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open DBF table: " + path_.string());
    }

    unsigned char header[32]{};
    input.read(reinterpret_cast<char*>(header), sizeof(header));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(header))) {
        throw std::runtime_error("invalid DBF header: " + path_.string());
    }

    if (header[0] != 0x03 && header[0] != 0x83) {
        throw std::runtime_error("unsupported DBF version byte");
    }

    const std::uint32_t record_count =
        static_cast<std::uint32_t>(header[4]) |
        (static_cast<std::uint32_t>(header[5]) << 8) |
        (static_cast<std::uint32_t>(header[6]) << 16) |
        (static_cast<std::uint32_t>(header[7]) << 24);

    const std::uint16_t header_length =
        static_cast<std::uint16_t>(header[8]) |
        (static_cast<std::uint16_t>(header[9]) << 8);
    record_length_ =
        static_cast<std::uint16_t>(header[10]) |
        (static_cast<std::uint16_t>(header[11]) << 8);

    std::size_t offset = 1;
    while (true) {
        unsigned char descriptor[32]{};
        input.read(reinterpret_cast<char*>(descriptor), sizeof(descriptor));
        if (!input) {
            throw std::runtime_error("truncated DBF field descriptors");
        }

        if (descriptor[0] == 0x0D) {
            input.seekg(-31, std::ios::cur);
            break;
        }

        std::string field_name(reinterpret_cast<char*>(descriptor), 11);
        const auto nul = field_name.find('\0');
        if (nul != std::string::npos) {
            field_name.resize(nul);
        }
        field_name = upper(trim(field_name));

        const std::size_t length = descriptor[16];        fields_.push_back(
            {field_name, static_cast<char>(descriptor[11]), offset, length, descriptor[17]});
        offset += length;
    }

    input.seekg(header_length, std::ios::beg);
    records_.reserve(record_count);

    for (std::uint32_t i = 0; i < record_count; ++i) {
        std::vector<char> record(record_length_);
        input.read(record.data(), static_cast<std::streamsize>(record.size()));
        if (!input) {
            throw std::runtime_error("truncated DBF record data");
        }

        // Preserve physical records exactly, including the deletion marker.
        // dBASE record numbers and NDX record pointers refer to physical rows.
        records_.push_back(std::move(record));
    }
}

void DbfTable::flush_record(std::size_t record_index) {
    std::fstream file(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot update DBF table: " + path_.string());
    }

    unsigned char header[32]{};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    const std::uint16_t header_length =
        static_cast<std::uint16_t>(header[8]) |
        (static_cast<std::uint16_t>(header[9]) << 8);

    const std::streamoff position =
        static_cast<std::streamoff>(header_length) +
        static_cast<std::streamoff>(record_index * record_length_);

    file.seekp(position, std::ios::beg);
    file.write(records_[record_index].data(),
               static_cast<std::streamsize>(records_[record_index].size()));
    if (!file) {
        throw std::runtime_error("failed writing DBF record");
    }
}

const DbfTable::Field& DbfTable::find_field(const std::string& name) const {
    const std::string folded = upper(name);
    const auto it = std::find_if(fields_.begin(), fields_.end(), [&](const Field& field) {
        return field.name == folded;
    });

    if (it == fields_.end()) {
        throw std::runtime_error("unknown DBF field: " + name);
    }

    return *it;
}

NdxIndex::NdxIndex(std::filesystem::path path) : path_(std::move(path)) {
    load_header();
}

void NdxIndex::load_header() {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open NDX index: " + path_.string());
    }

    std::array<unsigned char, 512> header{};
    input.read(reinterpret_cast<char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    if (input.gcount() != static_cast<std::streamsize>(header.size())) {
        throw std::runtime_error("invalid NDX header: " + path_.string());
    }

    const auto read_u16 = [&](std::size_t offset) {
        return static_cast<std::uint16_t>(header[offset]) |               (static_cast<std::uint16_t>(header[offset + 1]) << 8);
    };
    const auto read_u32 = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(header[offset]) |
               (static_cast<std::uint32_t>(header[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(header[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(header[offset + 3]) << 24);
    };

    root_page_ = read_u32(0);
    key_length_ = read_u16(12);
    key_type_ = read_u16(16);
    key_record_length_ = read_u32(18);

    if (root_page_ == 0 || key_length_ == 0 ||
        key_record_length_ < static_cast<std::uint32_t>(8U + key_length_)) {
        throw std::runtime_error("unsupported or corrupt NDX header");
    }

    const char* expression = reinterpret_cast<const char*>(header.data() + 24);
    const std::size_t available = header.size() - 24;
    const std::size_t length = strnlen(expression, available);
    expression_ = upper(trim(std::string(expression, length)));
}

const std::string& NdxIndex::expression() const noexcept {
    return expression_;
}

std::size_t NdxIndex::seek(const Value& key) const {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open NDX index: " + path_.string());
    }

    auto read_u32 = [](const unsigned char* bytes) {
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    };

    const auto compare_key = [&](const unsigned char* bytes) {
        if (key_type_ == 1) {
            double stored = 0.0;
            std::memcpy(&stored, bytes, sizeof(double));
            const double wanted = key.as_number();
            return stored < wanted ? -1 : (stored > wanted ? 1 : 0);
        }

        const std::string stored =
            rtrim_spaces(std::string(reinterpret_cast<const char*>(bytes), key_length_));
        const std::string wanted = key.as_string();
        if (stored < wanted) {
            return -1;
        }
        if (stored > wanted) {
            return 1;
        }
        return 0;
    };

    std::uint32_t page_number = root_page_;

    while (page_number != 0) {
        std::array<unsigned char, 512> page{};
        input.seekg(static_cast<std::streamoff>(page_number) * 512, std::ios::beg);
        input.read(reinterpret_cast<char*>(page.data()),
                   static_cast<std::streamsize>(page.size()));
        if (input.gcount() != static_cast<std::streamsize>(page.size())) {
            throw std::runtime_error("truncated NDX page");
        }

        const std::uint32_t count = read_u32(page.data());
        std::uint32_t next_page = 0;

        for (std::uint32_t i = 0; i < count; ++i) {
            const std::size_t entry_offset =
                4 + static_cast<std::size_t>(i) * key_record_length_;
            if (entry_offset + key_record_length_ > page.size()) {
                throw std::runtime_error("corrupt NDX key entry");
            }

            const unsigned char* entry = page.data() + entry_offset;
            const std::uint32_t lower_page = read_u32(entry);
            const std::uint32_t record_number = read_u32(entry + 4);
            const int comparison = compare_key(entry + 8);

            if (comparison >= 0) {
                if (lower_page != 0) {
                    next_page = lower_page;
                    break;
                }

                if (comparison == 0) {
                    return record_number;
                }

                return 0;
            }
        }

        if (next_page != 0) {
            page_number = next_page;
            continue;
        }
        const std::size_t tail_offset =
            4 + static_cast<std::size_t>(count) * key_record_length_;
        if (tail_offset + 4 <= page.size()) {
            page_number = read_u32(page.data() + tail_offset);
            if (page_number != 0) {
                continue;
            }
        }

        return 0;
    }

    return 0;
}

Vm::Vm(std::ostream& output) : output_(output) {}

void Vm::run(const Program& program, const std::filesystem::path& working_directory) {
    stack_.clear();
    std::size_t ip = 0;

    while (ip < program.code.size()) {
        const Instruction& instruction = program.code[ip];

        switch (instruction.opcode) {
        case OpCode::PushLiteral:
            stack_.push_back(instruction.operand);
            ++ip;
            break;

        case OpCode::LoadName:
            stack_.push_back(load_name(instruction.text));
            ++ip;
            break;

        case OpCode::StoreName:
            variables_[upper(instruction.text)] = pop();
            ++ip;
            break;

        case OpCode::SelectArea: {
            const std::string selector = trim(instruction.text);
            char* end = nullptr;
            const long numeric = std::strtol(selector.c_str(), &end, 10);

            if (end != nullptr && *end == '\0') {
                if (numeric <= 0) {
                    throw std::runtime_error("SELECT requires a positive work area");
                }
                active_area_ = static_cast<int>(numeric);
                work_areas_.try_emplace(active_area_);
            } else {
                const std::string wanted = upper(selector);
                bool matched = false;
                for (const auto& [number, area] : work_areas_) {
                    if (!area.alias.empty() && upper(area.alias) == wanted) {
                        active_area_ = number;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    throw std::runtime_error("unknown work-area alias: " + selector);
                }
            }

            ++ip;
            break;
        }

        case OpCode::OpenTable: {
            std::filesystem::path path = instruction.text;
            if (!path.has_extension()) {
                path += ".dbf";
            }
            if (path.is_relative()) {
                path = working_directory / path;
            }

            WorkArea& area = active_work_area();
            area.table = std::make_unique<DbfTable>(path);
            area.index.reset();
            area.filter.reset();
            area.found = false;

            const std::string requested_alias = instruction.operand.as_string();
            area.alias = requested_alias.empty()
                ? upper(path.stem().string())
                : upper(requested_alias);

            ++ip;
            break;
        }

        case OpCode::OpenIndex: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("SET INDEX TO with no table open");
            }

            std::filesystem::path path = instruction.text;
            if (!path.has_extension()) {
                path += ".ndx";
            }
            if (path.is_relative()) {
                path = working_directory / path;
            }

            area.index = std::make_unique<NdxIndex>(path);
            area.found = false;
            ++ip;
            break;
        }

        case OpCode::SetFilter: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("SET FILTER TO with no table open");
            }
            area.filter = instruction.embedded_program;
            ++ip;
            break;
        }

        case OpCode::GoTop: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("GO TOP with no table open");
            }
            position_first_visible(area);
            area.found = false;
            ++ip;
            break;
        }

        case OpCode::GoRecord: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("GO with no table open");
            }

            const double requested = pop().as_number();
            if (requested < 0.0) {
                throw std::runtime_error("GO requires a non-negative record number");
            }

            area.table->go_record(static_cast<std::size_t>(requested));
            area.found = false;
            ++ip;
            break;
        }

        case OpCode::Skip: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("SKIP with no table open");
            }

            const double requested = pop().as_number();
            skip_visible(area, static_cast<std::ptrdiff_t>(requested));
            area.found = false;
            ++ip;
            break;
        }

        case OpCode::Seek: {
            WorkArea& area = active_work_area();
            if (!area.table || !area.index) {
                throw std::runtime_error("SEEK requires an open table and active index");
            }

            const std::size_t record_number = area.index->seek(pop());
            area.found = record_number != 0;
            area.table->go_record(record_number);
            ++ip;
            break;
        }

        case OpCode::DeleteRecord: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("DELETE with no table open");
            }
            area.table->set_deleted(true);
            ++ip;
            break;
        }

        case OpCode::RecallRecord: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("RECALL with no table open");
            }
            area.table->set_deleted(false);
            ++ip;
            break;
        }

        case OpCode::ReplaceField: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("REPLACE with no table open");
            }
            area.table->replace(instruction.text, pop());
            ++ip;
            break;
        }

        case OpCode::Print:
            output_ << pop().as_string() << '\n';
            ++ip;
            break;

        case OpCode::UnaryNot:
            stack_.push_back(Value(!pop().as_logical()));
            ++ip;
            break;

        case OpCode::LogicalAnd: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_logical() && rhs.as_logical()));
            ++ip;
            break;
        }

        case OpCode::LogicalOr: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_logical() || rhs.as_logical()));
            ++ip;
            break;
        }

        case OpCode::Add: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_number() + rhs.as_number()));
            ++ip;
            break;
        }

        case OpCode::Subtract: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_number() - rhs.as_number()));
            ++ip;
            break;
        }

        case OpCode::Multiply: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_number() * rhs.as_number()));
            ++ip;
            break;
        }

        case OpCode::Divide: {
            const Value rhs = pop();
            const Value lhs = pop();
            if (rhs.as_number() == 0.0) {
                throw std::runtime_error("division by zero");
            }
            stack_.push_back(Value(lhs.as_number() / rhs.as_number()));
            ++ip;
            break;
        }

        case OpCode::Greater: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_number() > rhs.as_number()));
            ++ip;
            break;
        }

        case OpCode::Less: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(lhs.as_number() < rhs.as_number()));
            ++ip;
            break;
        }

        case OpCode::Equal: {
            const Value rhs = pop();
            const Value lhs = pop();

            if (std::holds_alternative<std::string>(lhs.storage()) ||
                std::holds_alternative<std::string>(rhs.storage())) {
                stack_.push_back(Value(lhs.as_string() == rhs.as_string()));
            } else {
                stack_.push_back(Value(
                    std::fabs(lhs.as_number() - rhs.as_number()) < 1e-12));
            }
            ++ip;
            break;
        }

        case OpCode::Jump:
            ip = instruction.target;
            break;

        case OpCode::JumpIfFalse:
            ip = pop().as_logical() ? ip + 1 : instruction.target;
            break;

        case OpCode::SetFound:
            active_work_area().found = pop().as_logical();
            ++ip;
            break;

        case OpCode::CallEof: {
            const WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("EOF() with no table open");
            }
            stack_.push_back(Value(area.table->eof()));
            ++ip;
            break;
        }

        case OpCode::CallBof: {
            const WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("BOF() with no table open");
            }
            stack_.push_back(Value(area.table->bof()));
            ++ip;
            break;
        }

        case OpCode::CallFound:
            stack_.push_back(Value(active_work_area().found));
            ++ip;
            break;

        case OpCode::CallRecno: {
            const WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("RECNO() with no table open");
            }
            stack_.push_back(Value(static_cast<double>(area.table->recno())));
            ++ip;
            break;
        }

        case OpCode::CallReccount: {
            const WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("RECCOUNT() with no table open");
            }
            stack_.push_back(Value(static_cast<double>(area.table->reccount())));
            ++ip;
            break;
        }

        case OpCode::CallDeleted: {
            const WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("DELETED() with no table open");
            }
            stack_.push_back(Value(area.table->deleted()));
            ++ip;
            break;
        }

        case OpCode::Halt:
            return;
        }
    }
}

const std::unordered_map<std::string, Value>& Vm::variables() const noexcept {
    return variables_;
}

Value Vm::pop() {
    if (stack_.empty()) {
        throw std::runtime_error("VM stack underflow");
    }

    Value value = std::move(stack_.back());
    stack_.pop_back();
    return value;
}

Vm::WorkArea& Vm::active_work_area() {
    return work_areas_[active_area_];
}

const Vm::WorkArea& Vm::active_work_area() const {
    const auto it = work_areas_.find(active_area_);
    if (it == work_areas_.end()) {
        throw std::runtime_error(
            "active work area " + std::to_string(active_area_) + " is not initialised");
    }
    return it->second;
}

const Vm::WorkArea& Vm::work_area_for_alias(const std::string& alias) const {
    const std::string wanted = upper(alias);
    for (const auto& [number, area] : work_areas_) {
        (void)number;
        if (!area.alias.empty() && upper(area.alias) == wanted) {
            return area;
        }
    }

    throw std::runtime_error("unknown work-area alias: " + alias);
}


Value Vm::evaluate_expression(const Program& program) const {
    std::vector<Value> values;

    const auto pop_value = [&]() {
        if (values.empty()) {
            throw std::runtime_error("expression stack underflow");
        }
        Value value = std::move(values.back());
        values.pop_back();
        return value;
    };

    for (std::size_t ip = 0; ip < program.code.size(); ++ip) {
        const Instruction& instruction = program.code[ip];

        switch (instruction.opcode) {
        case OpCode::PushLiteral:
            values.push_back(instruction.operand);
            break;

        case OpCode::LoadName:
            values.push_back(load_name(instruction.text));
            break;

        case OpCode::UnaryNot:
            values.push_back(Value(!pop_value().as_logical()));
            break;

        case OpCode::LogicalAnd: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_logical() && rhs.as_logical()));
            break;
        }

        case OpCode::LogicalOr: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_logical() || rhs.as_logical()));
            break;
        }

        case OpCode::Add: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_number() + rhs.as_number()));
            break;
        }

        case OpCode::Subtract: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_number() - rhs.as_number()));
            break;
        }

        case OpCode::Multiply: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_number() * rhs.as_number()));
            break;
        }

        case OpCode::Divide: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            if (rhs.as_number() == 0.0) {
                throw std::runtime_error("division by zero in filter expression");
            }
            values.push_back(Value(lhs.as_number() / rhs.as_number()));
            break;
        }

        case OpCode::Greater: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_number() > rhs.as_number()));
            break;
        }

        case OpCode::Less: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(lhs.as_number() < rhs.as_number()));
            break;
        }

        case OpCode::Equal: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            if (std::holds_alternative<std::string>(lhs.storage()) ||
                std::holds_alternative<std::string>(rhs.storage())) {
                values.push_back(Value(lhs.as_string() == rhs.as_string()));
            } else {
                values.push_back(Value(
                    std::fabs(lhs.as_number() - rhs.as_number()) < 1e-12));
            }
            break;
        }

        case OpCode::CallEof: {
            const WorkArea& area = active_work_area();
            values.push_back(Value(area.table ? area.table->eof() : true));
            break;
        }

        case OpCode::CallBof: {
            const WorkArea& area = active_work_area();
            values.push_back(Value(area.table ? area.table->bof() : true));
            break;
        }

        case OpCode::CallFound:
            values.push_back(Value(active_work_area().found));
            break;

        case OpCode::CallRecno: {
            const WorkArea& area = active_work_area();
            values.push_back(Value(
                area.table ? static_cast<double>(area.table->recno()) : 0.0));
            break;
        }

        case OpCode::CallReccount: {
            const WorkArea& area = active_work_area();
            values.push_back(Value(
                area.table ? static_cast<double>(area.table->reccount()) : 0.0));
            break;
        }

        case OpCode::CallDeleted: {
            const WorkArea& area = active_work_area();
            values.push_back(Value(area.table ? area.table->deleted() : false));
            break;
        }

        case OpCode::Halt:
            if (values.empty()) {
                return {};
            }
            return values.back();

        default:
            throw std::runtime_error("unsupported opcode in filter expression");
        }
    }

    return values.empty() ? Value{} : values.back();
}

bool Vm::filter_matches(const WorkArea& area) const {
    if (!area.filter) {
        return true;
    }

    if (!area.table || area.table->bof() || area.table->eof()) {
        return false;
    }

    return evaluate_expression(*area.filter).as_logical();
}

void Vm::position_first_visible(WorkArea& area) {
    area.table->go_top();

    if (!area.filter) {
        return;
    }

    while (!area.table->eof() && !filter_matches(area)) {
        area.table->skip(1);
    }
}

void Vm::skip_visible(WorkArea& area, std::ptrdiff_t count) {
    if (count == 0) {
        return;
    }

    if (!area.filter) {
        area.table->skip(count);
        return;
    }

    const std::ptrdiff_t direction = count > 0 ? 1 : -1;
    const std::size_t matches_to_skip =
        static_cast<std::size_t>(count > 0 ? count : -count);

    for (std::size_t moved = 0; moved < matches_to_skip; ++moved) {
        area.table->skip(direction);

        while (!area.table->bof() && !area.table->eof() &&
               !filter_matches(area)) {
            area.table->skip(direction);
        }

        if (area.table->bof() || area.table->eof()) {
            return;
        }
    }
}

Value Vm::load_name(const std::string& name) const {
    const std::string folded = upper(name);

    const auto variable = variables_.find(folded);
    if (variable != variables_.end()) {
        return variable->second;
    }

    const auto alias_separator = folded.find("->");
    if (alias_separator != std::string::npos) {
        const std::string alias = trim(folded.substr(0, alias_separator));
        const std::string field = trim(folded.substr(alias_separator + 2));
        const WorkArea& area = work_area_for_alias(alias);
        if (!area.table) {
            throw std::runtime_error("alias has no table open: " + alias);
        }
        return area.table->field(field);
    }

    const WorkArea& area = active_work_area();
    if (area.table) {
        return area.table->field(folded);
    }

    throw std::runtime_error("unknown name: " + name);
}

} // namespace xabl