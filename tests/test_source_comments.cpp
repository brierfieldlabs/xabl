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

        // Semicolon continuation is resolved after && comments, and
        // ordinary quoted semicolons are never continuation markers.
        std::ostringstream continued;
        xabl::Vm multiline_vm(continued);
        multiline_vm.run(compiler.compile(
            "STORE 1 + ; && ignore the rest of this physical line\r\n"
            "    2 + ;\r\n"
            "    3 TO total\r\n"
            "? total\r\n"
            "STORE 'a;b' TO literal\r\n"
            "? literal\r\n"
            "? 'one' + ; && trailing comment\r\n"
            " * skipped full-line comment\r\n"
            "  'two'\r\n"
            "IF total = ;\r\n"
            "   6\r\n"
            "? 'yes'\r\n"
            "ENDIF\r\n"
            "DO CASE\r\n"
            " CASE total = ;\r\n"
            "   6\r\n"
            "  ? 'match'\r\n"
            " OTHERWISE\r\n"
            "  ? 'incorrect'\r\n"
            "ENDCASE\r\n"), ".");
        if (continued.str() != "6\na;b\nonetwo\nyes\nmatch\n") {
            throw std::runtime_error("line continuation output mismatch: " +
                                     continued.str());
        }

        for (std::string_view bad : {
                 "? 1 + ;",             // EOF after trailing continuation
                 "? 1 + ;\n&& comment",// no next executable source
                 "  ? 1 + ;\n  BADNAME(3)",
                 "? 'unterminated;\n? 8"  // quote shields semicolon
             }) {
            bool failed = false;
            try { (void)compiler.compile(bad); }
            catch (const std::runtime_error&) { failed = true; }
            if (!failed) {
                throw std::runtime_error("bad continued source compiled: " +
                                         std::string(bad));
            }
        }
        try {
            (void)compiler.compile(
                "? 5 + ; && comment\n"
                "    unknown(4)\n");
            throw std::runtime_error("bad continuation unexpectedly accepted");
        } catch (const std::runtime_error& ex) {
            if (std::string(ex.what()).find("line 1:") == std::string::npos)
                throw;
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
