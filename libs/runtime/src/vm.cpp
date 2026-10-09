// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include "internal.hpp"
#include "comparison.hpp"
#include "text_functions.hpp"
#include "operators.hpp"
#include "numeric_functions.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace xabl {
Vm::Vm(std::ostream& output, CompatibilityProfile profile)
    : output_(output), profile_(profile) {
    profile_.require_implemented();
}

void Vm::run(const Program& program, const std::filesystem::path& working_directory) {
    if (program.dialect != profile_.dialect) {
        throw std::runtime_error("program/runtime compatibility profile mismatch");
    }
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

        case OpCode::CloseTable: {
            WorkArea& area = active_work_area();
            area.table.reset();
            area.index.reset();
            area.filter.reset();
            area.alias.clear();
            area.found = false;
            ++ip;
            break;
        }

        case OpCode::CloseIndex: {
            WorkArea& area = active_work_area();
            area.index.reset();
            area.found = false;
            ++ip;
            break;
        }

        case OpCode::AppendBlank: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("APPEND BLANK with no table open");
            }
            if (area.index) {
                throw std::runtime_error(
                    "APPEND BLANK requires closing NDX index until index writes are supported");
            }
            area.table->append_blank();
            area.found = false;
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

        case OpCode::SetDeletedVisibility:
            hide_deleted_ = instruction.operand.as_logical();
            ++ip;
            break;

        case OpCode::SetExact:
            exact_ = instruction.operand.as_logical();
            ++ip;
            break;

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

        case OpCode::GoBottom: {
            WorkArea& area = active_work_area();
            if (!area.table) {
                throw std::runtime_error("GO BOTTOM with no table open");
            }
            position_last_visible(area);
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
            // Avoid undefined float-to-integer casts. One-past-maximum
            // values round exactly to the bound in a double, so reject
            // those too. The minimum ptrdiff_t remains representable.
            const double exclusive_upper = std::ldexp(
                1.0, std::numeric_limits<std::ptrdiff_t>::digits);
            if (!std::isfinite(requested) ||
                requested >= exclusive_upper ||
                requested < -exclusive_upper) {
                throw std::runtime_error("SKIP count is outside supported range");
            }
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
            if (record_number > area.table->reccount()) {
                throw std::runtime_error("NDX SEEK references an invalid DBF record");
            }
            area.found = record_number != 0;
            // In dBASE III PLUS, an unsuccessful indexed SEEK leaves the
            // record pointer at EOF rather than BOF (RECNO=RECCOUNT+1).
            area.table->go_record(
                record_number == 0 ? area.table->reccount() + 1 : record_number);
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
            // An NDX expression can reference any field. Until NDX writes
            // are supported, never persist a record that could invalidate
            // its index, even when the field seems unrelated to the key.
            if (area.index) {
                throw std::runtime_error(
                    "REPLACE requires closing NDX index until index writes are supported");
            }
            area.table->replace(instruction.text, pop());
            ++ip;
            break;
        }

        case OpCode::CallLen:
        case OpCode::CallUpper:
        case OpCode::CallLower:
        case OpCode::CallTrim:
        case OpCode::CallLTrim:
            stack_.push_back(apply_text_function(instruction.opcode, pop()));
            ++ip;
            break;

        case OpCode::CallLeft:
        case OpCode::CallRight:
        case OpCode::CallSubstr: {
            const bool has_length = instruction.opcode == OpCode::CallSubstr &&
                                    instruction.target == 3;
            const Value length = has_length ? pop() : Value{};
            const Value position = pop();
            const Value characters = pop();
            stack_.push_back(apply_slice_function(
                instruction.opcode, characters, position,
                has_length ? &length : nullptr));
            ++ip;
            break;
        }

        case OpCode::CallAt: {
            const Value haystack = pop();
            const Value needle = pop();
            stack_.push_back(apply_at_function(needle, haystack));
            ++ip;
            break;
        }

        case OpCode::CallAsc:
        case OpCode::CallChr:
            stack_.push_back(apply_character_code_function(
                instruction.opcode, pop()));
            ++ip;
            break;

        case OpCode::CallSpace:
            stack_.push_back(apply_space_function(pop()));
            ++ip;
            break;

        case OpCode::CallReplicate: {
            const Value count = pop();
            const Value text = pop();
            stack_.push_back(apply_replicate_function(text, count));
            ++ip;
            break;
        }

        case OpCode::CallVal:
            stack_.push_back(apply_val_function(pop()));
            ++ip;
            break;

        case OpCode::CallStr: {
            const Value decimals = instruction.target == 3 ? pop() : Value{};
            const Value width = instruction.target >= 2 ? pop() : Value{};
            const Value number = pop();
            stack_.push_back(apply_str_function(
                number, instruction.target >= 2 ? &width : nullptr,
                instruction.target == 3 ? &decimals : nullptr));
            ++ip;
            break;
        }

        case OpCode::CallAbs:
        case OpCode::CallInt:
            stack_.push_back(apply_unary_numeric_function(
                instruction.opcode, pop()));
            ++ip;
            break;

        case OpCode::CallMin:
        case OpCode::CallMax: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(apply_minmax_function(
                instruction.opcode, left, right));
            ++ip;
            break;
        }

        case OpCode::CallStuff: {
            const Value replacement = pop();
            const Value quantity = pop();
            const Value start = pop();
            const Value target = pop();
            stack_.push_back(apply_stuff_function(
                target, start, quantity, replacement));
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

        case OpCode::Add:
        case OpCode::Subtract: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(
                apply_additive_operator(instruction.opcode, lhs, rhs));
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

        case OpCode::Greater:
        case OpCode::Less: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(ordered_values(instruction.opcode, lhs, rhs)));
            ++ip;
            break;
        }

        case OpCode::Equal:
        case OpCode::EqualExact: {
            const Value rhs = pop();
            const Value lhs = pop();
            stack_.push_back(Value(equal_values(
                lhs, rhs, exact_, instruction.opcode == OpCode::EqualExact)));
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

        case OpCode::CallLen:
        case OpCode::CallUpper:
        case OpCode::CallLower:
        case OpCode::CallTrim:
        case OpCode::CallLTrim:
            values.push_back(apply_text_function(instruction.opcode, pop_value()));
            break;

        case OpCode::CallLeft:
        case OpCode::CallRight:
        case OpCode::CallSubstr: {
            const bool has_length = instruction.opcode == OpCode::CallSubstr &&
                                    instruction.target == 3;
            const Value length = has_length ? pop_value() : Value{};
            const Value position = pop_value();
            const Value characters = pop_value();
            values.push_back(apply_slice_function(
                instruction.opcode, characters, position,
                has_length ? &length : nullptr));
            break;
        }

        case OpCode::CallAt: {
            const Value haystack = pop_value();
            const Value needle = pop_value();
            values.push_back(apply_at_function(needle, haystack));
            break;
        }

        case OpCode::CallAsc:
        case OpCode::CallChr:
            values.push_back(apply_character_code_function(
                instruction.opcode, pop_value()));
            break;

        case OpCode::CallSpace:
            values.push_back(apply_space_function(pop_value()));
            break;

        case OpCode::CallReplicate: {
            const Value count = pop_value();
            const Value text = pop_value();
            values.push_back(apply_replicate_function(text, count));
            break;
        }

        case OpCode::CallVal:
            values.push_back(apply_val_function(pop_value()));
            break;

        case OpCode::CallStr: {
            const Value decimals = instruction.target == 3 ? pop_value() : Value{};
            const Value width = instruction.target >= 2 ? pop_value() : Value{};
            const Value number = pop_value();
            values.push_back(apply_str_function(
                number, instruction.target >= 2 ? &width : nullptr,
                instruction.target == 3 ? &decimals : nullptr));
            break;
        }

        case OpCode::CallAbs:
        case OpCode::CallInt:
            values.push_back(apply_unary_numeric_function(
                instruction.opcode, pop_value()));
            break;

        case OpCode::CallMin:
        case OpCode::CallMax: {
            const Value right = pop_value();
            const Value left = pop_value();
            values.push_back(apply_minmax_function(
                instruction.opcode, left, right));
            break;
        }

        case OpCode::CallStuff: {
            const Value replacement = pop_value();
            const Value quantity = pop_value();
            const Value start = pop_value();
            const Value target = pop_value();
            values.push_back(apply_stuff_function(
                target, start, quantity, replacement));
            break;
        }

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

        case OpCode::Add:
        case OpCode::Subtract: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(
                apply_additive_operator(instruction.opcode, lhs, rhs));
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

        case OpCode::Greater:
        case OpCode::Less: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(ordered_values(instruction.opcode, lhs, rhs)));
            break;
        }

        case OpCode::Equal:
        case OpCode::EqualExact: {
            const Value rhs = pop_value();
            const Value lhs = pop_value();
            values.push_back(Value(equal_values(
                lhs, rhs, exact_, instruction.opcode == OpCode::EqualExact)));
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

bool Vm::record_visible(const WorkArea& area) const {
    if (!area.table || area.table->bof() || area.table->eof()) {
        return false;
    }

    if (hide_deleted_ && area.table->deleted()) {
        return false;
    }

    return filter_matches(area);
}

void Vm::position_last_visible(WorkArea& area) {
    if (area.index) {
        const auto order = area.index->ordered_records();
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            if (*it == 0 || *it > area.table->reccount()) {
                throw std::runtime_error("NDX references an invalid physical record");
            }
            area.table->go_record(*it);
            if (record_visible(area)) return;
        }
        area.table->go_record(0); // BOF when no indexed row is visible
        return;
    }
    area.table->go_bottom();
    while (!area.table->eof() && !area.table->bof() &&
           !record_visible(area)) {
        area.table->skip(-1);
    }
}

void Vm::position_first_visible(WorkArea& area) {
    if (area.index) {
        const auto order = area.index->ordered_records();
        for (const auto physical_row : order) {
            if (!physical_row || physical_row > area.table->reccount()) {
                throw std::runtime_error("NDX references an invalid physical record");
            }
            area.table->go_record(physical_row);
            if (record_visible(area)) return;
        }
        area.table->go_record(area.table->reccount() + 1);
        return;
    }

    area.table->go_top();
    while (!area.table->eof() && !record_visible(area)) {
        area.table->skip(1);
    }
}

void Vm::skip_visible(WorkArea& area, std::ptrdiff_t count) {
    if (count == 0) {
        return;
    }

    if (area.index) {
        const auto order = area.index->ordered_records();
        // Check the index before moving the record pointer. A stale NDX
        // must not direct navigation to an invalid physical row.
        for (const auto physical_row : order) {
            if (!physical_row || physical_row > area.table->reccount()) {
                throw std::runtime_error("NDX references an invalid physical record");
            }
        }
        const bool forward = count > 0;
        std::ptrdiff_t position = forward ? -1 :
            static_cast<std::ptrdiff_t>(order.size());
        if (!area.table->bof() && !area.table->eof()) {
            const auto at = std::find(order.begin(), order.end(),
                                      area.table->recno());
            if (at == order.end()) {
                throw std::runtime_error("current physical row missing from active NDX");
            }
            position = std::distance(order.begin(), at);
        } else if (area.table->bof()) {
            position = -1;
        } else {
            position = static_cast<std::ptrdiff_t>(order.size());
        }
        const auto direction = forward ? 1 : -1;
        std::uintmax_t remaining = forward
            ? static_cast<std::uintmax_t>(count)
            : static_cast<std::uintmax_t>(-(count + 1)) + 1;
        while (remaining != 0) {
            position += direction;
            while (position >= 0 &&
                   position < static_cast<std::ptrdiff_t>(order.size())) {
                area.table->go_record(order[static_cast<std::size_t>(position)]);
                if (record_visible(area)) break;
                position += direction;
            }
            if (position < 0) {
                area.table->go_record(0);
                return;
            }
            if (position >= static_cast<std::ptrdiff_t>(order.size())) {
                area.table->go_record(area.table->reccount() + 1);
                return;
            }
            --remaining;
        }
        return;
    }

    if (!area.filter && !hide_deleted_) {
        area.table->skip(count);
        return;
    }

    const std::ptrdiff_t direction = count > 0 ? 1 : -1;
    const std::uintmax_t matches_to_skip = count > 0
        ? static_cast<std::uintmax_t>(count)
        : static_cast<std::uintmax_t>(-(count + 1)) + 1;

    for (std::uintmax_t moved = 0; moved < matches_to_skip; ++moved) {
        area.table->skip(direction);

        while (!area.table->bof() && !area.table->eof() &&
               !record_visible(area)) {
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
