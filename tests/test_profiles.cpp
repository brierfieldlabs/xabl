// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#include <xabl/runtime/xabl.hpp>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

int main() {
    try {
        const auto default_profile = xabl::CompatibilityProfile{};
        if (default_profile.name() != "dbase-iii-plus" ||
            !default_profile.implemented()) {
            throw std::runtime_error("unexpected default dialect");
        }
        xabl::Compiler compiler(default_profile);
        xabl::Program program = compiler.compile("? 3 * 4");
        if (program.dialect != xabl::CompatibilityDialect::DBaseIIIPlus) {
            throw std::runtime_error("program dialect stamp incorrect");
        }

        std::ostringstream output;
        xabl::Vm vm(output, default_profile);
        vm.run(program, ".");
        if (output.str() != "12\n") {
            throw std::runtime_error("profiled execution incorrect");
        }

        for (std::string_view id : {
                 "dbase-iv", "clipper", "foxpro", "visual-foxpro",
                 "visual-dbase", "dbase-plus", "harbour", "xharbour",
                 "modern"}) {
            const auto profile = xabl::CompatibilityProfile::parse(id);
            if (profile.name() != id || profile.implemented()) {
                throw std::runtime_error("unsupported dialect was misclassified");
            }
            try {
                xabl::Compiler unsupported(profile);
                throw std::logic_error("unsupported compiler was accepted");
            } catch (const std::runtime_error&) {}
            try {
                xabl::Vm unsupported(output, profile);
                throw std::logic_error("unsupported VM was accepted");
            } catch (const std::runtime_error&) {}
        }

        try {
            (void)xabl::CompatibilityProfile::parse("something-unknown");
            throw std::logic_error("invalid profile was accepted");
        } catch (const std::invalid_argument&) {}

        program.dialect = xabl::CompatibilityDialect::DBaseIV;
        try {
            vm.run(program, ".");
            throw std::logic_error("profile mismatch was accepted");
        } catch (const std::runtime_error&) {}

        std::cout << "compatibility profile tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
