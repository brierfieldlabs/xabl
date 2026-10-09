// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include "internal.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <limits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace xabl {
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

void DbfTable::go_bottom() noexcept {
    before_first_ = false;
    current_ = records_.empty() ? 0 : records_.size() - 1;
}

// Append one physical DBF record, retaining legacy record numbers. DBF III
// conventionally terminates its data with an optional 0x1A EOF byte.
// Refuse unknown trailing data rather than corrupting proprietary extensions.
// This implementation is deliberately single-writer; locking and indexed
// write maintenance are separate work.
void DbfTable::append_blank() {
    if (records_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("DBF record count overflow");
    }
    std::fstream file(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot append to DBF table: " + path_.string());
    }
    unsigned char header[32]{};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!file) throw std::runtime_error("truncated DBF header before append");
    const std::size_t header_length =
        static_cast<std::size_t>(header[8]) |
        (static_cast<std::size_t>(header[9]) << 8);
    const std::uint32_t existing_count =
        static_cast<std::uint32_t>(header[4]) |
        (static_cast<std::uint32_t>(header[5]) << 8) |
        (static_cast<std::uint32_t>(header[6]) << 16) |
        (static_cast<std::uint32_t>(header[7]) << 24);
    if (existing_count != records_.size() || record_length_ == 0) {
        throw std::runtime_error("DBF changed on disk before append");
    }

    const auto position = static_cast<std::streamoff>(header_length) +
                          static_cast<std::streamoff>(records_.size()) *
                              static_cast<std::streamoff>(record_length_);
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size < position || size > position + 1) {
        throw std::runtime_error("unexpected DBF trailer; append refused");
    }
    if (size == position + 1) {
        file.seekg(position);
        char marker{};
        file.get(marker);
        if (!file || static_cast<unsigned char>(marker) != 0x1A) {
            throw std::runtime_error("unknown DBF trailing byte; append refused");
        }
    }

    std::vector<char> record(record_length_, ' ');
    for (const Field& field_info : fields_) {
        if (field_info.offset >= record_length_ ||
            field_info.length > record_length_ - field_info.offset) {
            throw std::runtime_error("invalid DBF field layout; append refused");
        }
        if (field_info.type == 'L') {
            record[field_info.offset] = '?'; // uninitialised logical value
        }
    }

    file.clear();
    file.seekp(position);
    file.write(record.data(), static_cast<std::streamsize>(record.size()));
    file.put(static_cast<char>(0x1A));
    file.flush();
    if (!file) throw std::runtime_error("failed writing appended DBF record");

    const auto now = std::chrono::floor<std::chrono::days>(
        std::chrono::system_clock::now());
    const auto date = std::chrono::year_month_day(now);
    const auto year = static_cast<int>(date.year()) - 1900;
    const auto count = static_cast<std::uint32_t>(records_.size() + 1);
    header[1] = static_cast<unsigned char>(year);
    header[2] = static_cast<unsigned char>(static_cast<unsigned>(date.month()));
    header[3] = static_cast<unsigned char>(static_cast<unsigned>(date.day()));
    for (int i = 0; i < 4; ++i) {
        header[4 + i] = static_cast<unsigned char>((count >> (8 * i)) & 0xFF);
    }
    file.seekp(1);
    file.write(reinterpret_cast<const char*>(header + 1), 7);
    file.flush();
    if (!file) throw std::runtime_error("failed updating DBF record count");

    records_.push_back(std::move(record));
    before_first_ = false;
    current_ = records_.size() - 1;
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
        return Value(c == 'T' || c == 'Y');    }

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

    auto& record = records_[current_];
    const std::vector<char> original = record;
    record[0] = deleted_state ? '*' : ' ';
    try {
        flush_record(current_, original);
    } catch (...) {
        record = original;
        throw;
    }
}

void DbfTable::replace(const std::string& name, const Value& value) {
    if (before_first_ || eof()) {
        throw std::runtime_error("REPLACE attempted outside a record");
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
    const std::vector<char> original = record;
    std::copy(encoded.begin(), encoded.end(),
              record.begin() + static_cast<std::ptrdiff_t>(field_info.offset));
    try {
        flush_record(current_, original);
    } catch (...) {
        record = original;
        throw;
    }
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
    header_length_ = header_length;

    // Check physical size before reserving record_count: a corrupt 32-bit
    // count must not cause massive allocations or out-of-bounds reads.
    if (header_length < 33 || record_length_ == 0) {
        throw std::runtime_error("invalid DBF header or record length");
    }
    const auto physical_size = std::filesystem::file_size(path_);
    if (physical_size < header_length ||
        static_cast<std::uintmax_t>(record_count) >
            (physical_size - header_length) / record_length_) {
        throw std::runtime_error("DBF record count exceeds physical file size");
    }

    std::size_t offset = 1;
    while (true) {
        const auto position = input.tellg();
        if (position < 0 || static_cast<std::uintmax_t>(position) >= header_length) {
            throw std::runtime_error("missing DBF field descriptor terminator");
        }
        unsigned char descriptor[32]{};
        // A valid zero-record DBF may have only the 0x0D descriptor
        // terminator plus an optional 0x1A at this point. Read one byte
        // before deciding whether the remaining 31 bytes exist.
        input.read(reinterpret_cast<char*>(descriptor), 1);
        if (!input) {
            throw std::runtime_error("truncated DBF field descriptors");
        }
        if (descriptor[0] == 0x0D) {
            break;
        }
        if (static_cast<std::uintmax_t>(position) + sizeof(descriptor) >
            header_length) {
            throw std::runtime_error("truncated DBF field descriptor");
        }
        input.read(reinterpret_cast<char*>(descriptor + 1), 31);
        if (!input) {
            throw std::runtime_error("truncated DBF field descriptors");
        }

        std::string field_name(reinterpret_cast<char*>(descriptor), 11);
        const auto nul = field_name.find('\0');
        if (nul != std::string::npos) {
            field_name.resize(nul);
        }
        field_name = upper(trim(field_name));

        const std::size_t length = descriptor[16];
        if (field_name.empty() || length == 0 ||
            offset > record_length_ || length > record_length_ - offset) {
            throw std::runtime_error("invalid DBF field descriptor layout");
        }
        fields_.push_back({field_name, static_cast<char>(descriptor[11]),
                           offset, length, descriptor[17]});
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

void DbfTable::flush_record(std::size_t record_index,
                            const std::vector<char>& original_record) {
    if (record_index >= records_.size() ||
        original_record.size() != record_length_ ||
        records_[record_index].size() != record_length_) {
        throw std::runtime_error("invalid DBF record update");
    }
    std::fstream file(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot update DBF table: " + path_.string());
    }

    unsigned char header[32]{};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!file) {
        throw std::runtime_error("truncated DBF header before update");
    }
    const auto read_u16 = [&](std::size_t offset) -> std::size_t {
        return static_cast<std::size_t>(header[offset]) |
               (static_cast<std::size_t>(header[offset + 1]) << 8);
    };
    const auto disk_count =
        static_cast<std::uint32_t>(header[4]) |
        (static_cast<std::uint32_t>(header[5]) << 8) |
        (static_cast<std::uint32_t>(header[6]) << 16) |
        (static_cast<std::uint32_t>(header[7]) << 24);
    if ((header[0] != 0x03 && header[0] != 0x83) ||
        read_u16(8) != header_length_ ||
        read_u16(10) != record_length_ ||
        disk_count != records_.size()) {
        throw std::runtime_error("DBF header changed on disk; update refused");
    }
    const auto records_end = static_cast<std::uintmax_t>(header_length_) +
                             static_cast<std::uintmax_t>(records_.size()) *
                                 record_length_;
    file.seekg(0, std::ios::end);
    const auto file_end = file.tellg();
    if (file_end < 0 ||
        (static_cast<std::uintmax_t>(file_end) != records_end &&
         static_cast<std::uintmax_t>(file_end) != records_end + 1)) {
        throw std::runtime_error("DBF length/trailer changed; update refused");
    }
    if (static_cast<std::uintmax_t>(file_end) == records_end + 1) {
        file.seekg(static_cast<std::streamoff>(records_end));
        char marker{};
        file.get(marker);
        if (!file || static_cast<unsigned char>(marker) != 0x1A) {
            throw std::runtime_error("unknown DBF trailer; update refused");
        }
    }

    const auto position = static_cast<std::streamoff>(header_length_) +
                          static_cast<std::streamoff>(record_index) *
                              static_cast<std::streamoff>(record_length_);
    file.clear();
    file.seekg(position);
    std::vector<char> existing(record_length_);
    file.read(existing.data(), static_cast<std::streamsize>(existing.size()));
    if (!file || existing != original_record) {
        throw std::runtime_error("DBF record changed on disk; update refused");
    }
    // Optimistic preimage validation prevents accidental sequential stale
    // writes, but is not a substitute for cross-process locking: another
    // writer could intervene between the check and the write.
    file.clear();
    file.seekp(position);
    file.write(records_[record_index].data(),
               static_cast<std::streamsize>(records_[record_index].size()));
    file.flush();
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

} // namespace xabl
