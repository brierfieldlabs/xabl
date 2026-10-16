// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

void verify(std::string_view script, std::string_view expected,
            const std::filesystem::path& directory) {
    xabl::Compiler compiler;
    std::ostringstream output;
    xabl::Vm vm(output);
    vm.run(compiler.compile(script), directory);
    if (output.str() != expected) {
        throw std::runtime_error("unexpected control-flow output; got: " +
                                 output.str());
    }
}

void reject(std::string_view script) {
    try {
        (void)xabl::Compiler{}.compile(script);
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("invalid control-flow program was accepted: " +
                             std::string(script));
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        verify(R"(
IF .F.
    ? "wrong"
ELSE
    ? "yes"
ENDIF
IF .T.
    ? "first"
ELSE
    ? "wrong"
ENDIF
)", "yes\nfirst\n", ".");

        verify(R"(
STORE 0 TO n
DO WHILE n < 5
    n = n + 1
    IF n = 2
        LOOP
    ELSE
        IF n = 4
            EXIT
        ELSE
            ? n
        ENDIF
    ENDIF
ENDDO
? n
)", "1\n3\n4\n", ".");

        verify(R"(
STORE 0 TO outer
DO WHILE outer < 3
    outer = outer + 1
    STORE 0 TO inner
    DO WHILE inner < 5
        inner = inner + 1
        IF inner = 2
            EXIT
        ENDIF
    ENDDO
    ? outer * 10 + inner
ENDDO
)", "12\n22\n32\n", ".");

        // The first true CASE wins, and later CASE/OTHERWISE bodies
        // are skipped. Nested blocks must keep their own jump targets.
        verify(R"(
STORE 0 TO n
DO CASE
    CASE n < 0
        ? "wrong"
    CASE n = 0
        ? "zero"
    CASE .T.
        ? "wrong"
    OTHERWISE
        ? "wrong"
ENDCASE
DO CASE
    CASE .F.
        ? "wrong"
    CASE .F.
        ? "wrong"
ENDCASE
DO CASE
    CASE .F.
        ? "wrong"
    OTHERWISE
        DO CASE
            CASE .T.
                IF .T.
                    ? "nested"
                ENDIF
            OTHERWISE
                ? "wrong"
        ENDCASE
ENDCASE
)", "zero\nnested\n", ".");

        verify(R"(
STORE 0 TO n
DO WHILE n < 3
    n = n + 1
    DO CASE
        CASE n = 1
            ? "one"
        CASE n = 2
            ? "two"
        OTHERWISE
            ? "three"
    ENDCASE
ENDDO
)", "one\ntwo\nthree\n", ".");

        verify(R"(
USE customers
GO TOP
DO WHILE NOT EOF()
    IF BALANCE < 100
        SKIP
        LOOP
    ENDIF
    ? TRIM(NAME)
    IF NAME = "Charlie"
        EXIT
    ENDIF
    SKIP
ENDDO
)", "Alice\nCharlie\n", argv[1]);

        for (const std::string_view bad : {
                 "ELSE", "ENDIF", "LOOP", "EXIT",
                 "DO WHILE .T.\nELSE\nENDDO",
                 "IF .T.\nELSE\nELSE\nENDIF",
                 "IF .T.\nELSE\nENDDO",
                 "DO WHILE .T.\nIF .T.\nEXIT\nENDIF",
                 "IF .T.\nLOOP\nENDIF",
                 "CASE .T.", "OTHERWISE", "ENDCASE",
                 "DO CASE\nENDCASE",
                 "DO CASE\nOTHERWISE\nENDCASE",
                 "DO CASE\nCASE\nENDCASE",
                 "DO CASE\nCASE .T.\nOTHERWISE\nOTHERWISE\nENDCASE",
                 "DO CASE\nCASE .T.\nOTHERWISE\nCASE .F.\nENDCASE",
                 "DO CASE\nCASE .T.\nENDIF",
                 "DO CASE\nCASE .T.\nDO WHILE .F.\nENDCASE"}) {
            reject(bad);
        }

        std::cout << "legacy control-flow tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
