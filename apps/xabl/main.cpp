// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs

#include <xabl/runtime/xabl.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: xabl <program.prg|program.xabl>\n";
        return 2;
    }

    const std::filesystem::path program_path = argv[1];
    std::ifstream input(program_path);
    if (!input) {
        std::cerr << "xabl: cannot open source file: " << program_path << "\n";
        return 3;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    try {
        xabl::Compiler compiler;
        const xabl::Program program = compiler.compile(buffer.str());

        xabl::Vm vm(std::cout);
        vm.run(program, program_path.parent_path());
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "xabl: " << ex.what() << "\n";
        return 1;
    }
}