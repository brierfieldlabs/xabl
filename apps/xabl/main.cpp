// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs

#include <xabl/runtime/xabl.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view usage =
    "Usage: xabl [--dialect dbase-iii-plus] [--check] "
    "<program.prg|program.xabl>\n";

void report_compile_error(const std::filesystem::path& path,
                          std::string_view message) {
    // A GCC-style location lets VS Code's existing $gcc matcher display
    // compiler errors without a second parser in the editor extension.
    if (message.starts_with("line ")) {
        const auto separator = message.find(':', 5);
        const auto number = message.substr(5, separator - 5);
        if (separator != std::string_view::npos && !number.empty() &&
            std::all_of(number.begin(), number.end(), [](unsigned char c) {
                return c >= '0' && c <= '9';
            })) {
            auto detail = message.substr(separator + 1);
            if (detail.starts_with(" ")) detail.remove_prefix(1);
            std::cerr << path.string() << ':' << number
                      << ":1: error: " << detail << '\n';
            return;
        }
    }
    std::cerr << "xabl: " << message << '\n';
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << usage
                  << "Without --check, successful compilation executes the program.\n"
                  << "--check compiles only; no program statements execute.\n"
                  << "Only dbase-iii-plus is currently implemented.\n";
        return 0;
    }

    std::string_view dialect_name = "dbase-iii-plus";
    std::filesystem::path program_path;
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--check") {
            check_only = true;
        } else if (arg == "--dialect" && i + 1 < argc) {
            dialect_name = argv[++i];
        } else if (arg.starts_with('-') || !program_path.empty()) {
            std::cerr << usage;
            return 2;
        } else {
            program_path = argv[i];
        }
    }
    if (program_path.empty()) {
        std::cerr << usage;
        return 2;
    }

    std::ifstream input(program_path);
    if (!input) {
        std::cerr << "xabl: cannot open source file: " << program_path << '\n';
        return 3;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        const auto profile = xabl::CompatibilityProfile::parse(dialect_name);
        xabl::Compiler compiler(profile);
        const xabl::Program program = compiler.compile(buffer.str());
        if (check_only) return 0;

        xabl::Vm vm(std::cout, profile);
        vm.run(program, program_path.parent_path());
        return 0;
    } catch (const std::exception& ex) {
        if (check_only) report_compile_error(program_path, ex.what());
        else std::cerr << "xabl: " << ex.what() << '\n';
        return 1;
    }
}
