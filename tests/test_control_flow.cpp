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
                 "IF .T.\nLOOP\nENDIF"}) {
            reject(bad);
        }

        std::cout << "legacy control-flow tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
