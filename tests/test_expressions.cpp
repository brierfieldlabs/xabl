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
        {"? -(2 + 3)", "-5\n"},
        {"? -2 + 3", "1\n"},
        {"? 2 * -3 + 9", "3\n"},
        {"? 8 - 3 + 1", "6\n"},
        {"? 8 / 2 * 2", "8\n"},
        {"? -(2 + 3) * -2", "10\n"},
        {"? .NOT. .F.", ".T.\n"},
        {"? NOT 2 < 3", ".F.\n"},
        {"? NOT (2 < 3)", ".F.\n"},
        {"? 2 > 3 .AND. 1 < 2 .OR. 6 >= 6", ".T.\n"},
        {"? 2 <> 3", ".T.\n"},
        {"? 2 != 2", ".F.\n"},
        {"? 2 == 2", ".T.\n"},
        {"? 2 <= 2", ".T.\n"},
        {"? 1.5e2 + .5", "150.5\n"},
        {"? +(+2)", "2\n"},
        {"? \"A+B\" = 'A+B'", ".T.\n"},
        {"? 'A AND B'", "A AND B\n"},
        {"? 'DON''T PANIC'", "DON'T PANIC\n"},
        {"? \"OR .NOT. AND ->\"", "OR .NOT. AND ->\n"},
        {"? 'JACOBSON' = 'JACOBS'", ".T.\n"},
        {"? 'JACOBS' = 'JACOBSON'", ".F.\n"},
        {"? 'JACOBSON' <> 'JACOBS'", ".F.\n"},
        {"? 'JACOBSON' == 'JACOBS'", ".F.\n"},
        {"? 'ABC' == 'ABC '", ".F.\n"},
        {"? 'ABC' = ''", ".T.\n"},
        {"? '' = 'ABC'", ".F.\n"},
        {"SET EXACT ON\n? 'JACOBSON' = 'JACOBS'", ".F.\n"},
        {"SET EXACT ON\n? 'ABC ' = 'ABC'", ".T.\n"},
        {"SET EXACT ON\n? 'ABC' = 'ABC '", ".T.\n"},
        {"SET EXACT ON\n? 'ABC' == 'ABC '", ".F.\n"},
        {"SET EXACT ON\n? 'ABC' <> 'ABC '", ".F.\n"},
        {"SET EXACT ON\nSET EXACT OFF\n? 'ABC' = 'AB'", ".T.\n"}
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

        // Invalid syntax must fail during compilation, not become a bogus
        // variable lookup at runtime or silently ignore trailing tokens.
        for (const char* invalid : {
                 "? (2 + 3", "? 2 +", "? \"unterminated",
                 "? 1 2", "? UNKNOWN(1)", "? 1 / / 2",
                 "? EOF(1)", "? .BAD.", "? 5 ==", "? ()"}) {
            try {
                (void)compiler.compile(invalid);
                std::cerr << "Expression unexpectedly compiled: " << invalid << '\n';
                return 1;
            } catch (const std::runtime_error&) {
                // Expected diagnostics, including source line number.
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
