// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs

#include <xabl/runtime/xabl.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open test source: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void expect_statuses(const std::filesystem::path& table_path) {
    xabl::DbfTable table(table_path);
    table.go_top();

    if (table.field("status").as_string() != "REVIEW") {
        throw std::runtime_error("Alice status was not persisted");
    }

    table.skip();
    if (!table.field("status").as_string().empty()) {
        throw std::runtime_error("Bob status should remain blank");
    }

    table.skip();
    if (table.field("status").as_string() != "REVIEW") {
        throw std::runtime_error("Charlie status was not persisted");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "fixture directory argument missing\n";
        return 2;
    }

    const std::filesystem::path fixtures = argv[1];

    try {
        xabl::Compiler compiler;
        const xabl::Program program =
            compiler.compile(read_text(fixtures / "legacy-customer-review.prg"));

        std::ostringstream output;
        xabl::Vm vm(output);
        vm.run(program, fixtures);

        const std::string expected =
            "Alice\n"
            "Charlie\n";

        if (output.str() != expected) {
            std::cerr << "unexpected VM output\nExpected:\n"
                      << expected << "Actual:\n" << output.str();
            return 1;
        }

        expect_statuses(fixtures / "customers.dbf");

        const xabl::Program seek_program =
            compiler.compile(read_text(fixtures / "legacy-seek.prg"));

        std::ostringstream seek_output;
        xabl::Vm seek_vm(seek_output);
        seek_vm.run(seek_program, fixtures);

        if (seek_output.str() != "Charlie\n") {
            std::cerr << "unexpected SEEK output\nExpected:\nCharlie\nActual:\n"
                      << seek_output.str();
            return 1;
        }

        const xabl::Program total_program =
            compiler.compile(read_text(fixtures / "legacy-total.prg"));

        std::ostringstream total_output;
        xabl::Vm total_vm(total_output);
        total_vm.run(total_program, fixtures);

        if (total_output.str() != "405.5\n") {
            std::cerr << "unexpected arithmetic output\nExpected:\n405.5\nActual:\n"
                      << total_output.str();
            return 1;
        }

        const xabl::Program work_area_program =
            compiler.compile(read_text(fixtures / "legacy-work-areas.prg"));

        std::ostringstream work_area_output;
        xabl::Vm work_area_vm(work_area_output);
        work_area_vm.run(work_area_program, fixtures);

        const std::string expected_work_areas =
            "Bob\n"
            "Alice\n"
            "Bob\n";

        if (work_area_output.str() != expected_work_areas) {
            std::cerr << "unexpected work-area output\nExpected:\n"
                      << expected_work_areas << "Actual:\n" << work_area_output.str();
            return 1;
        }

        const xabl::Program record_status_program =
            compiler.compile(read_text(fixtures / "legacy-record-status.prg"));

        std::ostringstream record_status_output;
        xabl::Vm record_status_vm(record_status_output);
        record_status_vm.run(record_status_program, fixtures);

        const std::string expected_record_status =
            "3\n"
            "1\n"
            "3\n"
            ".T.\n"
            "4\n"
            ".T.\n"
            "0\n"
            ".T.\n"
            ".F.\n";

        if (record_status_output.str() != expected_record_status) {
            std::cerr << "unexpected record-status output\nExpected:\n"
                      << expected_record_status << "Actual:\n"
                      << record_status_output.str();
            return 1;
        }

        std::cout << "XABL smoke test passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "smoke test failed: " << ex.what() << "\n";
        return 1;
    }
}