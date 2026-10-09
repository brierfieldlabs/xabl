// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include "internal.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <unordered_set>
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
        key_record_length_ < static_cast<std::uint32_t>(8U + key_length_) ||
        key_record_length_ > 504 ||
        (key_type_ != 0 && key_type_ != 1) ||
        (key_type_ == 1 && key_length_ < sizeof(double))) {
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

    const auto bytes = std::filesystem::file_size(path_);
    if (bytes < 1024 || bytes % 512 != 0) {
        throw std::runtime_error("invalid NDX physical size");
    }
    const auto page_count = bytes / 512;
    std::unordered_set<std::uint32_t> visited;
    std::uint32_t page_number = root_page_;

    while (page_number != 0) {
        if (page_number >= page_count || !visited.insert(page_number).second) {
            throw std::runtime_error("invalid or cyclic NDX SEEK page reference");
        }
        std::array<unsigned char, 512> page{};
        input.seekg(static_cast<std::streamoff>(page_number) * 512, std::ios::beg);
        input.read(reinterpret_cast<char*>(page.data()),
                   static_cast<std::streamsize>(page.size()));
        if (input.gcount() != static_cast<std::streamsize>(page.size())) {
            throw std::runtime_error("truncated NDX page");
        }

        const std::uint32_t count = read_u32(page.data());
        if (count > (page.size() - 8) / key_record_length_) {
            throw std::runtime_error("invalid NDX SEEK key count");
        }
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

std::vector<std::size_t> NdxIndex::ordered_records() const {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open NDX index: " + path_.string());
    }
    const auto bytes = std::filesystem::file_size(path_);
    if (bytes < 1024 || bytes % 512 != 0) {
        throw std::runtime_error("invalid NDX physical size");
    }
    const auto page_count = bytes / 512;
    std::unordered_set<std::uint32_t> visited;
    std::unordered_set<std::uint32_t> referenced_rows;
    std::vector<std::size_t> records;

    const auto read_u32 = [](const unsigned char* bytes) {
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    };

    // NDX nodes use an in-order B-tree arrangement. The lower-page
    // pointer precedes each key, with the rightmost pointer after the
    // final key. Treat repeated page references as corruption.
    std::function<void(std::uint32_t, std::size_t)> traverse =
        [&](std::uint32_t page_number, std::size_t depth) {
        if (!page_number) return;
        // Bound recursion even when a corrupt file has thousands of distinct
        // pages arranged into an absurdly deep chain.
        if (depth > 256) {
            throw std::runtime_error("NDX B-tree exceeds safe traversal depth");
        }
        if (page_number >= page_count || !visited.insert(page_number).second) {
            throw std::runtime_error("invalid or cyclic NDX page reference");
        }
        std::array<unsigned char, 512> page{};
        input.clear();
        input.seekg(static_cast<std::streamoff>(page_number) * 512);
        input.read(reinterpret_cast<char*>(page.data()),
                   static_cast<std::streamsize>(page.size()));
        if (!input) {
            throw std::runtime_error("truncated NDX page");
        }
        const auto count = read_u32(page.data());
        if (key_record_length_ == 0 ||
            count > (page.size() - 8) / key_record_length_) {
            throw std::runtime_error("corrupt NDX key count");
        }

        for (std::uint32_t i = 0; i < count; ++i) {
            const auto offset = 4 + static_cast<std::size_t>(i) *
                                       key_record_length_;
            const auto lower_page = read_u32(page.data() + offset);
            const auto physical_row = read_u32(page.data() + offset + 4);
            traverse(lower_page, depth + 1);
            if (physical_row) {
                if (!referenced_rows.insert(physical_row).second) {
                    throw std::runtime_error("duplicate NDX physical record pointer");
                }
                records.push_back(physical_row);
            }
        }
        const auto tail_offset = 4 + static_cast<std::size_t>(count) *
                                        key_record_length_;
        traverse(read_u32(page.data() + tail_offset), depth + 1);
    };
    traverse(root_page_, 0);
    return records;
}

} // namespace xabl
