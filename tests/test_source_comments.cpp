// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

std::string read_fixture(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot read PRG fixture");
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        xabl::Compiler compiler;
        const auto directory = std::filesystem::path(argv[1]);
        std::ostringstream output;
        xabl::Vm vm(output);
        vm.run(compiler.compile(read_fixture(directory / "legacy-comments.prg")),
               directory);
        const std::string expected =
            "100\ntwo&&three\nDon't && quit\n5\n\nOK\nA&&B\n"
            "A TO B\nREADY TO GO\nA' TO 'B\nCharlie\n";
        if (output.str() != expected) {
            throw std::runtime_error("comment fixture output mismatch: " +
                                     output.str());
        }

        // The lexer must preserve a quoted && even when doubled-delimiter
        // escaping occurs on both single- and double-quoted strings.
        std::ostringstream quotes;
        xabl::Vm quote_vm(quotes);
        quote_vm.run(compiler.compile(
            "? 'a''&&''b' && comment\n"
            "? \"a\"\"&&\"\"b\" && comment\n"
            "NOTE another full-line comment\n"
            "?\n"), ".");
        if (quotes.str() != "a'&&'b\na\"&&\"b\n\n") {
            throw std::runtime_error("quoted comment handling mismatch");
        }

        try {
            (void)compiler.compile(
                "NOTE skipped header\n"
                "?42 && valid line\n"
                "? (2 + && invalid expression\n");
            throw std::runtime_error("invalid expression unexpectedly accepted");
        } catch (const std::runtime_error& ex) {
            if (std::string(ex.what()).find("line 3:") == std::string::npos) {
                throw;
            }
        }

        std::cout << "dBASE source-comment tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "source-comment test failed: " << error.what() << '\n';
        return 1;
    }
}
