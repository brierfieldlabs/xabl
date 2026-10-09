// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
#pragma once

#include <string_view>

namespace xabl {
class Program;

// Private compiler entry point; not a public language/extension ABI.
// Appends postfix bytecode to the supplied program and rejects malformed input.
struct ExpressionCompiler {
    Program& program;
    void emit(std::string_view source) const;
};
} // namespace xabl
