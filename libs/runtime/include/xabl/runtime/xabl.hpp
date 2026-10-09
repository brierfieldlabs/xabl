// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
//
// First executable XABL vertical slice.
// This deliberately implements a very small dBASE III PLUS-style subset so that
// language, VM, and DBF compatibility work can be tested end-to-end.

#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace xabl {

class Value {
public:
    using Storage = std::variant<std::monostate, bool, double, std::string>;

    Value() = default;
    Value(bool value);
    Value(double value);
    Value(std::string value);

    [[nodiscard]] bool is_empty() const;
    [[nodiscard]] bool as_logical() const;
    [[nodiscard]] double as_number() const;
    [[nodiscard]] std::string as_string() const;
    [[nodiscard]] const Storage& storage() const noexcept;

private:
    Storage storage_{};
};

enum class OpCode {
    PushLiteral,
    LoadName,
    StoreName,
    OpenTable,
    GoTop,
    Skip,
    ReplaceField,
    Print,
    UnaryNot,
    Greater,
    Less,
    Equal,
    Jump,
    JumpIfFalse,
    CallEof,
    Halt
};

struct Instruction {
    OpCode opcode{};
    Value operand{};
    std::string text{};
    std::size_t target{};
};

class Program {
public:
    std::vector<Instruction> code;
};

class Compiler {
public:
    [[nodiscard]] Program compile(std::string_view source) const;
};

class DbfTable {
public:
    explicit DbfTable(std::filesystem::path path);

    [[nodiscard]] bool eof() const noexcept;
    void go_top() noexcept;
    void skip() noexcept;

    [[nodiscard]] Value field(const std::string& name) const;
    void replace(const std::string& name, const Value& value);

private:
    struct Field {
        std::string name;
        char type{};
        std::size_t offset{};
        std::size_t length{};
        std::size_t decimals{};
    };

    std::filesystem::path path_;
    std::vector<Field> fields_;
    std::vector<std::vector<char>> records_;
    std::size_t record_length_{};
    std::size_t current_{};

    void load();
    void flush_record(std::size_t record_index);
    [[nodiscard]] const Field& find_field(const std::string& name) const;
};

class Vm {
public:
    explicit Vm(std::ostream& output);

    void run(const Program& program, const std::filesystem::path& working_directory);

    [[nodiscard]] const std::unordered_map<std::string, Value>& variables() const noexcept;

private:
    std::ostream& output_;
    std::vector<Value> stack_;
    std::unordered_map<std::string, Value> variables_;
    std::unique_ptr<DbfTable> table_;

    Value pop();
    [[nodiscard]] Value load_name(const std::string& name) const;
};

} // namespace xabl