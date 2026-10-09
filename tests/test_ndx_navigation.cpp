// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Validate that NDX key traversal is not confused with DBF physical order.
#include <xabl/runtime/xabl.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace fs = std::filesystem;

std::string load(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open fixture");
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}
void save(const fs::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary);
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!out) throw std::runtime_error("cannot save temporary fixture");
}
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Temp {
    fs::path path;
    ~Temp() { std::error_code error; fs::remove_all(path, error); }
};

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        const fs::path fixtures = argv[1];
        Temp temp{fs::temp_directory_path() /
                  ("xabl-ndx-" + std::to_string(
                      std::chrono::steady_clock::now().time_since_epoch().count()))};
        fs::create_directories(temp.path);
        std::string dbf = load(fixtures / "customers.dbf");
        const auto header_size = static_cast<unsigned char>(dbf[8]) |
                                 (static_cast<unsigned char>(dbf[9]) << 8);
        const auto record_size = static_cast<unsigned char>(dbf[10]) |
                                 (static_cast<unsigned char>(dbf[11]) << 8);
        check(record_size > 0, "invalid DBF fixture record size");
        // Reorder physical records: Charlie, Bob, Alice.
        const std::string first = dbf.substr(header_size, record_size);
        const std::string last = dbf.substr(header_size + 2 * record_size, record_size);
        dbf.replace(header_size, record_size, last);
        dbf.replace(header_size + 2 * record_size, record_size, first);
        save(temp.path / "shuffled.dbf", dbf);

        std::string ndx = load(fixtures / "customers.ndx");
        check(ndx.size() >= 1024, "invalid NDX fixture");
        // The NDX leaf entries remain sorted by key: Alice, Bob, Charlie.
        // Point them at DBF physical record numbers 3, 2 and 1.
        for (std::size_t i = 0; i < 3; ++i) {
            const auto entry_record = 512 + 4 + 28 * i + 4;
            const auto new_recno = static_cast<unsigned char>(3 - i);
            ndx[entry_record] = static_cast<char>(new_recno);
            ndx[entry_record + 1] = 0;
            ndx[entry_record + 2] = 0;
            ndx[entry_record + 3] = 0;
        }
        save(temp.path / "shuffled.ndx", ndx);

        xabl::NdxIndex index(temp.path / "shuffled.ndx");
        check(index.ordered_records() == std::vector<std::size_t>{3, 2, 1},
              "NDX B-tree traversal did not return physical rows in key order");
        check(index.seek(xabl::Value(std::string("Alice"))) == 3,
              "SEEK failed on shuffled physical records");

        std::ostringstream output;
        xabl::Vm vm(output);
        xabl::Compiler compiler;
        vm.run(compiler.compile(
            "USE shuffled\n"
            "SET INDEX TO shuffled\n"
            "GO TOP\n? NAME\n? RECNO()\n"
            "SKIP\n? NAME\n"
            "SKIP\n? NAME\n"
            "GO BOTTOM\n? NAME\n"
            "SKIP -2\n? NAME\n"
            "GO 1\n? NAME\n"
            "SKIP -1\n? NAME\n"
            "SET FILTER TO BALANCE > 100\n"
            "GO TOP\n? NAME\n"
            "SKIP\n? NAME\n"
            "SKIP\n? EOF()\n"
            "GO BOTTOM\n? NAME\n"
            "SET FILTER TO\n"
            "GO TOP\n? NAME\n"), temp.path);
        const std::string expected =
            "Alice\n3\nBob\nCharlie\nCharlie\nAlice\n"
            "Charlie\nBob\nAlice\nCharlie\n.T.\nCharlie\nAlice\n";
        check(output.str() == expected,
              ("unexpected NDX order:\n" + output.str()).c_str());
        check(load(fixtures / "customers.dbf") != dbf,
              "shuffled fixture unexpectedly equals original");

        // A failed SEEK must leave EOF true, BOF false, FOUND false, and
        // RECNO one past the last physical record, even from the first row.
        // A subsequent successful SEEK restores a valid current record.
        std::ostringstream seeks;
        xabl::Vm seek_vm(seeks);
        seek_vm.run(compiler.compile(
            "USE shuffled\nSET INDEX TO shuffled\n"
            "GO TOP\nSEEK 'NOT PRESENT'\n"
            "? EOF()\n? BOF()\n? FOUND()\n? RECNO()\n"
            "SEEK 'Bob'\n? EOF()\n? FOUND()\n? RECNO()\n"
            "SEEK 'ZZZZZZ'\n? EOF()\n? FOUND()\n? RECNO()\n"
            "SKIP -1\n? NAME\n"), temp.path);
        check(seeks.str() ==
                  ".T.\n.F.\n.F.\n4\n"
                  ".F.\n.T.\n2\n"
                  ".T.\n.F.\n4\nCharlie\n",
              ("unexpected failed SEEK pointer behaviour: " + seeks.str()).c_str());

        // A cyclic NDX page pointer is corrupted input, not a navigation path.
        ndx[512 + 4] = 1;
        ndx[512 + 5] = 0;
        ndx[512 + 6] = 0;
        ndx[512 + 7] = 0;
        save(temp.path / "cycle.ndx", ndx);
        bool rejected = false;
        try { (void)xabl::NdxIndex(temp.path / "cycle.ndx").ordered_records(); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "cyclic NDX was accepted");
        rejected = false;
        try {
            (void)xabl::NdxIndex(temp.path / "cycle.ndx")
                .seek(xabl::Value(std::string("Alice")));
        } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "cyclic NDX SEEK was accepted");

        // A corrupt record pointer must never make FOUND() true while the
        // table cursor points to EOF, and an impossible page key count must
        // be rejected before a byte-offset calculation can overflow.
        std::string bad_record = load(temp.path / "shuffled.ndx");
        const auto first_record_pointer = std::size_t(512 + 4 + 4);
        bad_record[first_record_pointer] = static_cast<char>(0xFF);
        bad_record[first_record_pointer + 1] = static_cast<char>(0xFF);
        bad_record[first_record_pointer + 2] = static_cast<char>(0x7F);
        bad_record[first_record_pointer + 3] = 0;
        save(temp.path / "badpointer.ndx", bad_record);
        rejected = false;
        try {
            xabl::Vm invalid_vm(output);
            invalid_vm.run(compiler.compile(
                "USE shuffled\nSET INDEX TO badpointer\nSEEK 'Alice'"), temp.path);
        } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "out-of-range NDX SEEK record pointer was accepted");

        std::string bad_count = load(temp.path / "shuffled.ndx");
        for (std::size_t i = 512; i < 516; ++i) {
            bad_count[i] = static_cast<char>(0xFF);
        }
        save(temp.path / "badcount.ndx", bad_count);
        rejected = false;
        try {
            (void)xabl::NdxIndex(temp.path / "badcount.ndx")
                .seek(xabl::Value(std::string("Alice")));
        } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "absurd NDX key count was accepted");

        std::cout << "NDX ordered navigation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "NDX navigation test failed: " << error.what() << '\n';
        return 1;
    }
}
