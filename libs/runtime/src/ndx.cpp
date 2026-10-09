// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include "internal.hpp"
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

} // namespace xabl
