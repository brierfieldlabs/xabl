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

        // The classic ?? form emits no newline and keeps ordinary
        // expression evaluation; ?? by itself emits no text.
        std::ostringstream inline_output;
        xabl::Vm inline_vm(inline_output);
        inline_vm.run(compiler.compile(
            "?? 'HEL'\n"
            "?? 'LO'\n"
            "??\n"
            "?\n"
            "? 'NEXT'\n"
            "?? 2 + 3\n"), ".");
        if (inline_output.str() != "HELLO\nNEXT\n5") {
            throw std::runtime_error("?? inline print output mismatch: " +
                                     inline_output.str());
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

        // STORE and simple assignment may only target lexer-style
        // memory-variable identifiers, not arbitrary left-hand text.
        std::ostringstream assigned;
        xabl::Vm assignment_vm(assigned);
        assignment_vm.run(compiler.compile(
            "STORE 7 TO _counter2\n"
            "result_3 = _counter2 + 1\n"
            "STORE result_3 + 2 TO rate, towing, _hours\n"
            "? result_3\n"
            "? rate\n"
            "? towing\n"
            "? _hours\n"), ".");
        if (assigned.str() != "8\n10\n10\n10\n") {
            throw std::runtime_error("valid memory-variable assignment failed");
        }
        for (std::string_view malformed : {
                 "2total = 5", "a + b = 5", "a.b = 5",
                 "STORE 1 TO a,,b", "STORE 1 TO a,", "STORE 2 TO a b",
                 "STORE 3 TO table->field"
             }) {
            bool rejected = false;
            try { (void)compiler.compile(malformed); }
            catch (const std::runtime_error& ex) {
                rejected = std::string(ex.what()).find("line 1:") !=
                           std::string::npos;
            }
            if (!rejected) {
                throw std::runtime_error("invalid variable target accepted: " +
                                         std::string(malformed));
            }
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
                 "? 'unterminated;\n? 8", // quote shields semicolon
                 "STORE 1 TO n; STORE 2 TO m",
                 "? 'one;two'; ? 'three'",
                 "? 1; ? 2;"  // both same-line and terminal separators
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
            (void)compiler.compile("   STORE 1 TO n; ? n\n");
            throw std::runtime_error("unsupported same-line command separator accepted");
        } catch (const std::runtime_error& ex) {
            if (std::string(ex.what()).find("line 1: same-line semicolon") ==
                std::string::npos) throw;
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
