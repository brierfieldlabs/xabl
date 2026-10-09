// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

int main() {
    const struct Case { const char* source; const char* expected; } cases[] = {
        {"? 2 + 3 * 4", "14\n"},
        {"? (2 + 3) * 4", "20\n"},
        {"? 20 - 5 - 3", "12\n"},
        {"? 24 / 3 / 2", "4\n"},
        {"? .T. .OR. .F. .AND. .F.", ".T.\n"},
        {"? (.T. .OR. .F.) .AND. .F.", ".F.\n"},
        {"? \"A+B\"", "A+B\n"},
        {"? 'A AND B'", "A AND B\n"},
        {"? -(2 + 3)", "-5\n"}
    };
    try {
        xabl::Compiler compiler;
        for (const auto& test : cases) {
            std::ostringstream output;
            xabl::Vm vm(output);
            vm.run(compiler.compile(test.source), ".");
            if (output.str() != test.expected) {
                std::cerr << "Expression: " << test.source << "\nExpected: " << test.expected
                          << "Actual: " << output.str();
                return 1;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
