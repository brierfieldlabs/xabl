// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Byte-level DBF III date read/write and typed expression tests.
#include <xabl/runtime/xabl.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>

namespace fs = std::filesystem;
namespace {
void require(bool ok, const std::string& explanation) {
    if (!ok) throw std::runtime_error(explanation);
}

std::string bytes(const fs::path& name) {
    std::ifstream input(name, std::ios::binary);
    require(bool(input), "cannot read " + name.string());
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

void script(xabl::Vm& vm, const xabl::Compiler& compiler,
            const std::string& code, const fs::path& dir) {
    vm.run(compiler.compile(code), dir);
}

void expect_failure(xabl::Vm& vm, const xabl::Compiler& compiler,
                    const std::string& source, const fs::path& dir) {
    bool failed = false;
    try { script(vm, compiler, source, dir); }
    catch (const std::runtime_error&) { failed = true; }
    require(failed, "date operation should fail: " + source);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp = fs::temp_directory_path() /
                          ("xabl-dates-" + std::to_string(stamp));
    try {
        fs::create_directories(temp);
        const fs::path input = fs::path(argv[1]) / "dates.dbf";
        const fs::path copy = temp / "dates.dbf";
        const std::string original = bytes(input);
        fs::copy_file(input, copy);
        xabl::DbfTable table(copy);
        require(table.reccount() == 3, "date DBF count");
        table.go_top();
        const xabl::Value started = table.field("STARTED");
        const auto* date = std::get_if<xabl::DateValue>(&started.storage());
        require(date && date->year == 1984 && date->month == 2 && date->day == 29,
                "DBF date failed leap-day decode");
        require(std::get_if<std::string>(&started.storage()) == nullptr,
                "date field was silently loaded as character storage");
        require(std::get<xabl::DateValue>(table.field("RETIRED").storage()).year == 0,
                "blank DBF date should be a typed blank value");

        xabl::Compiler compiler;
        std::ostringstream output;
        xabl::Vm vm(output);
        script(vm, compiler,
               "USE dates\nGO TOP\n"
               "? DTOS(STARTED)\n? DTOC(STARTED)\n"
               "? YEAR(STARTED)\n? MONTH(STARTED)\n? DAY(STARTED)\n"
               "? DTOS(RETIRED)\n? YEAR(RETIRED)\n"
               "? DTOS(CTOD('02/29/84'))\n"
               "? DTOS(CTOD('02/29/85'))\n"
               "? DTOS(CTOD('12/31/1999'))\n"
               "? DTOS(CTOD('not a date'))\n"
               "? CTOD('02/28/84') < STARTED\n"
               "? CTOD('03/01/84') > STARTED\n"
               "? CTOD('02/29/84') == STARTED\n"
               "SET FILTER TO YEAR(STARTED) >= 2000\n"
               "GO TOP\n? TRIM(NAME)\nSKIP\n? TRIM(NAME)\n"
               "SET FILTER TO MONTH(STARTED) == 2\n"
               "GO TOP\n? TRIM(NAME)\nSKIP\n? TRIM(NAME)\n",
               temp);
        require(output.str() ==
                "19840229\n02/29/84\n1984\n2\n29\n        \n0\n"
                "19840229\n19850301\n19991231\n        \n"
                ".T.\n.T.\n.T.\nBob\nCara\nAlice\nCara\n",
                "typed dates in expression/filter mismatch: " + output.str());

        // III PLUS calendar arithmetic operates on date values and
        // whole-day offsets, preserving Gregorian leap-day boundaries.
        std::ostringstream arithmetic_output;
        xabl::Vm arithmetic_vm(arithmetic_output);
        script(arithmetic_vm, compiler,
               "? DTOS(CTOD('02/28/2000') + 1)\n"
               "? DTOS(CTOD('03/01/2000') - 1)\n"
               "? DTOS(CTOD('12/31/99') + 1)\n"
               "? DTOS(CTOD('01/01/2000') - 1)\n"
               "? CTOD('03/01/84') - CTOD('02/29/84')\n"
               "? CTOD('01/01/2000') - CTOD('01/01/1999')\n"
               "? CTOD('01/01/1999') - CTOD('01/01/2000')\n"
               "? DTOS(CTOD('03/01/2000') - 366)\n"
               "? DTOS(CTOD('02/28/84') + 2)\n"
               "? DTOS(CTOD('03/01/84') - 2)\n"
               "? CTOD('12/31/1999') - CTOD('01/01/1999')\n"
               "USE dates\n"
               "SET FILTER TO STARTED + 1 >= CTOD('01/01/2001')\n"
               "GO TOP\n? TRIM(NAME)\n"
               "SET FILTER TO STARTED - CTOD('01/01/2000') > 0\n"
               "GO TOP\n? TRIM(NAME)\n", temp);
        require(arithmetic_output.str() ==
                "20000229\n20000229\n20000101\n19991231\n"
                "1\n365\n-365\n19990301\n19840301\n19840228\n364\nBob\nBob\n",
                "date arithmetic or date field filtering failed: " +
                    arithmetic_output.str());

        // Dates must be written as exactly eight YYYYMMDD bytes.
        script(vm, compiler, "SET FILTER TO\nGO TOP\n"
                             "REPLACE RETIRED WITH CTOD('12/31/99')\n"
                             "APPEND BLANK\n"
                             "? DTOS(STARTED)\n"
                             "REPLACE STARTED WITH CTOD('02/29/2000')\n"
                             "? YEAR(STARTED)\n", temp);
        require(output.str().ends_with("        \n2000\n"),
                "date append/replace output mismatch");
        xabl::DbfTable reopened(copy);
        reopened.go_top();
        require(std::get<xabl::DateValue>(reopened.field("RETIRED").storage()).year == 1999,
                "typed D-field replacement did not persist");
        reopened.go_bottom();
        require(std::get<xabl::DateValue>(reopened.field("STARTED").storage()).year == 2000,
                "appended date did not persist");
        const std::string updated = bytes(copy);
        const auto position = updated.find("20000229");
        require(position != std::string::npos, "DBF binary date not encoded YYYYMMDD");
        require(bytes(input) == original, "original date fixture modified");

        for (const auto* invalid : {
                "? YEAR('19840229')", "? DTOS('19840229')",
                "? DTOC(1984)", "? CTOD(1984)",
                "? YEAR()", "? DTOS(CTOD('01/01/84')) + 1",
                "? CTOD('01/01/84') + CTOD('01/02/84')",
                "? 1 + CTOD('01/01/84')",
                "? CTOD('01/01/84') + 1.5",
                "? CTOD('01/01/84') + '1'",
                "? CTOD('01/01/84') - .T.",
                "? CTOD('01/01/84') + 3660001",
                "? CTOD('01/01/84') - 1e100",
                "? CTOD('01/01/0001') - 1",
                "? CTOD('12/31/9999') + 1",
                "? CTOD('invalid') + 1",
                "? CTOD('invalid') - CTOD('01/01/84')",
                "? CTOD('02/29/84') > '19840229'",
                "? CTOD('02/29/84') == 1984",
                "REPLACE RETIRED WITH '19991231'"}) {
            expect_failure(vm, compiler, invalid, temp);
        }

        // A date descriptor must be exactly width 8, with zero decimals.
        for (const auto& [label, descriptor_offset, changed] :
                {std::tuple<std::string, std::size_t, char>{
                     "short-date", 32 + 32 + 16, char(7)},
                 {"date-decimals", 32 + 32 + 17, char(1)}}) {
            const fs::path altered_path = temp / (label + ".dbf");
            fs::copy_file(input, altered_path);
            auto raw = bytes(altered_path);
            raw[descriptor_offset] = changed;
            { std::ofstream out(altered_path, std::ios::binary | std::ios::trunc);
              out.write(raw.data(), static_cast<std::streamsize>(raw.size())); }
            bool refused = false;
            try { (void)xabl::DbfTable(altered_path); }
            catch (const std::runtime_error&) { refused = true; }
            require(refused, "invalid D-field descriptor accepted");
            require(bytes(altered_path) == raw, "invalid descriptor was rewritten");
        }

        const fs::path corrupt = temp / "invalid.dbf";
        fs::copy_file(input, corrupt);
        // Header 32 + three 32-byte field descriptors + 0x0D terminator.
        // First row: deletion byte, 12 name bytes, then 8-byte STARTED.
        const std::size_t date_offset = 32 + 3 * 32 + 1 + 1 + 12;
        auto corrupted = bytes(corrupt);
        corrupted.replace(date_offset, 8, "19850229");
        { std::ofstream out(corrupt, std::ios::binary | std::ios::trunc);
          out.write(corrupted.data(), static_cast<std::streamsize>(corrupted.size())); }
        xabl::DbfTable malformed(corrupt);
        malformed.go_top();
        bool rejected = false;
        try { (void)malformed.field("STARTED"); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "invalid calendar date accepted from DBF");
        require(bytes(corrupt) == corrupted, "invalid date read rewrote original bytes");
        bool wrong_date_rejected = false;
        try { table.replace("STARTED", xabl::Value(xabl::DateValue{2023, 2, 29})); }
        catch (const std::runtime_error&) { wrong_date_rejected = true; }
        require(wrong_date_rejected, "invalid typed date allowed into DBF");
        require(bytes(copy) == updated, "invalid typed date modified DBF on disk");

        fs::remove_all(temp);
        std::cout << "typed DBF date tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "date test failure: " << e.what() << "\n";
        std::error_code ignored;
        fs::remove_all(temp, ignored);
        return 1;
    }
}
