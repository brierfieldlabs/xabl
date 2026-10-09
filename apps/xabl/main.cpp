// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs

#include <xabl/runtime/xabl.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: xabl [--dialect dbase-iii-plus] <program.prg|program.xabl>\n"
                  << "Only dbase-iii-plus is currently implemented.\n";
        return 0;
    }
    if (argc != 2 && !(argc == 4 &&
                       std::string_view(argv[1]) == "--dialect")) {
        std::cerr << "Usage: xabl [--dialect dbase-iii-plus] "
                     "<program.prg|program.xabl>\n";
        return 2;
    }

    const std::string_view dialect_name =
        argc == 4 ? argv[2] : "dbase-iii-plus";
    const std::filesystem::path program_path = argc == 4 ? argv[3] : argv[1];
    std::ifstream input(program_path);
    if (!input) {
        std::cerr << "xabl: cannot open source file: " << program_path << "\n";
        return 3;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        const auto profile = xabl::CompatibilityProfile::parse(dialect_name);
        xabl::Compiler compiler(profile);
        const xabl::Program program = compiler.compile(buffer.str());

        xabl::Vm vm(std::cout, profile);
        vm.run(program, program_path.parent_path());
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "xabl: " << ex.what() << "\n";
        return 1;
    }
}