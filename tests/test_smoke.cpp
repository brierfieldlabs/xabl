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

        std::cout << "XABL smoke test passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "smoke test failed: " << ex.what() << "\n";
        return 1;
    }
}