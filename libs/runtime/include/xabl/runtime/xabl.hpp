// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
//
// First executable XABL vertical slice.
// This deliberately implements a very small dBASE III PLUS-style subset so that
// language, VM, and DBF compatibility work can be tested end-to-end.

#pragma once

#include <xabl/runtime/profile.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace xabl {

class Program;

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
    CloseTable,
    OpenIndex,
    CloseIndex,
    SetFilter,
    SetDeletedVisibility,
    SetExact,
    SelectArea,
    GoTop,
    GoBottom,
    GoRecord,
    AppendBlank,
    Skip,
    Seek,
    DeleteRecord,
    RecallRecord,
    ReplaceField,
    Print,
    UnaryNot,
    LogicalAnd,
    LogicalOr,
    Add,
    Subtract,
    Multiply,
    Divide,
    Greater,
    Less,
    Equal,
    EqualExact,
    Jump,
    JumpIfFalse,
    SetFound,
    CallEof,
    CallBof,
    CallFound,
    CallRecno,
    CallReccount,
    CallDeleted,
    Halt
};

struct Instruction {
    OpCode opcode{};
    Value operand{};
    std::string text{};
    std::size_t target{};
    std::shared_ptr<const Program> embedded_program{};
};

class Program {
public:
    // The compiler stamps bytecode for the selected legacy dialect. The VM
    // refuses execution under a different dialect.
    CompatibilityDialect dialect{CompatibilityDialect::DBaseIIIPlus};
    std::vector<Instruction> code;
};

class Compiler {
public:
    /// Create a compiler for an implemented profile (III PLUS by default).
    explicit Compiler(CompatibilityProfile profile = {});
    [[nodiscard]] Program compile(std::string_view source) const;

private:
    CompatibilityProfile profile_;
};

class DbfTable {
public:
    explicit DbfTable(std::filesystem::path path);

    [[nodiscard]] bool eof() const noexcept;
    [[nodiscard]] bool bof() const noexcept;
    [[nodiscard]] bool deleted() const;
    [[nodiscard]] std::size_t recno() const noexcept;
    [[nodiscard]] std::size_t reccount() const noexcept;

    void go_top() noexcept;
    void go_bottom() noexcept;
    void append_blank();
    void go_record(std::size_t one_based_record_number);
    void skip(std::ptrdiff_t count = 1) noexcept;
    void set_deleted(bool deleted);

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
    bool before_first_{false};

    void load();
    void flush_record(std::size_t record_index);
    [[nodiscard]] const Field& find_field(const std::string& name) const;
};

class NdxIndex {
public:
    explicit NdxIndex(std::filesystem::path path);

    [[nodiscard]] std::size_t seek(const Value& key) const;
    /// Physical DBF row numbers in ascending NDX order (read-only snapshot).
    [[nodiscard]] std::vector<std::size_t> ordered_records() const;
    [[nodiscard]] const std::string& expression() const noexcept;

private:
    std::filesystem::path path_;
    std::uint32_t root_page_{};
    std::uint16_t key_length_{};
    std::uint16_t key_type_{};
    std::uint32_t key_record_length_{};
    std::string expression_;

    void load_header();
};

class Vm {
public:
    /// Bind an executable runtime dialect and the program output stream.
    explicit Vm(std::ostream& output, CompatibilityProfile profile = {});

    void run(const Program& program, const std::filesystem::path& working_directory);

    [[nodiscard]] const std::unordered_map<std::string, Value>& variables() const noexcept;

private:
    struct WorkArea {
        std::unique_ptr<DbfTable> table;
        std::unique_ptr<NdxIndex> index;
        std::shared_ptr<const Program> filter;
        std::string alias;
        bool found{false};
    };

    std::ostream& output_;
    CompatibilityProfile profile_;
    std::vector<Value> stack_;
    std::unordered_map<std::string, Value> variables_;
    std::unordered_map<int, WorkArea> work_areas_;
    int active_area_{1};
    bool hide_deleted_{false};
    bool exact_{false};

    Value pop();
    [[nodiscard]] Value load_name(const std::string& name) const;
    [[nodiscard]] WorkArea& active_work_area();
    [[nodiscard]] const WorkArea& active_work_area() const;
    [[nodiscard]] const WorkArea& work_area_for_alias(const std::string& alias) const;
    [[nodiscard]] Value evaluate_expression(const Program& program) const;
    [[nodiscard]] bool filter_matches(const WorkArea& area) const;
    [[nodiscard]] bool record_visible(const WorkArea& area) const;
    void position_first_visible(WorkArea& area);
    void position_last_visible(WorkArea& area);
    void skip_visible(WorkArea& area, std::ptrdiff_t count);
};

} // namespace xabl