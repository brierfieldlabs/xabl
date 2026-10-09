// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Legacy dBASE III PLUS APPEND BLANK and navigation regression tests.
// Always write to disposable copies of generated DBF fixtures.
#include <xabl/runtime/xabl.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

struct TemporaryDirectory {
    fs::path path;

    ~TemporaryDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string content(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream), "cannot read test fixture " + path.string());
    std::ostringstream result;
    result << stream.rdbuf();
    return result.str();
}

void expect_runtime_error(const xabl::Compiler& compiler, xabl::Vm& vm,
                          const std::string& script, const fs::path& directory) {
    bool failed = false;
    try {
        vm.run(compiler.compile(script), directory);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    require(failed, "expected runtime error for: " + script);
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    try {
        const fs::path fixtures = argv[1];
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        TemporaryDirectory temp{fs::temp_directory_path() /
                                ("xabl-append-" + std::to_string(stamp))};
        fs::create_directories(temp.path);
        fs::copy_file(fixtures / "customers.dbf", temp.path / "append.dbf");

        const std::string original = content(fixtures / "customers.dbf");
        xabl::Compiler compiler;
        std::ostringstream output;
        xabl::Vm vm(output);
        vm.run(compiler.compile(
            "USE append ALIAS ADDED\n"
            "? RECCOUNT()\n"
            "APPEND BLANK\n"
            "? RECCOUNT()\n"
            "? RECNO()\n"
            "REPLACE NAME WITH 'Doris'\n"
            "REPLACE BALANCE WITH 55.5\n"
            "REPLACE STATUS WITH 'NEW'\n"
            "? NAME\n"
            "SET FILTER TO BALANCE > 100\n"
            "GO BOTTOM\n"
            "? NAME\n"
            "SET FILTER TO\n"
            "GO BOTTOM\n"
            "? NAME\n"
            "DELETE\n"
            "SET DELETED ON\n"
            "GO BOTTOM\n"
            "? NAME\n"
            "SET DELETED OFF\n"
            "GO BOTTOM\n"
            "? DELETED()\n"
            "RECALL\n"
            "? DELETED()\n"
            "USE\n"), temp.path);

        require(output.str() ==
                    "3\n4\n4\nDoris\nCharlie\nDoris\nCharlie\n.T.\n.F.\n",
                "APPEND BLANK / GO BOTTOM / filtering / close output mismatch: " +
                    output.str());
        expect_runtime_error(compiler, vm, "SELECT ADDED", temp.path);
        expect_runtime_error(compiler, vm, "GO TOP", temp.path);

        xabl::DbfTable reopened(temp.path / "append.dbf");
        require(reopened.reccount() == 4, "APPEND count not persisted");
        reopened.go_bottom();
        require(reopened.recno() == 4, "APPEND physical record number invalid");
        require(reopened.field("NAME").as_string() == "Doris",
                "APPEND character value not persisted");
        require(reopened.field("BALANCE").as_number() == 55.5,
                "APPEND numeric value not persisted");
        require(!reopened.deleted(), "RECALL was not persisted");
        require(content(fixtures / "customers.dbf") == original,
                "original fixture was modified");

        const std::string bytes = content(temp.path / "append.dbf");
        const auto count = static_cast<unsigned char>(bytes[4]) |
                           (static_cast<unsigned char>(bytes[5]) << 8);
        require(count == 4, "DBF header count not updated");
        require(static_cast<unsigned char>(bytes.back()) == 0x1A,
                "DBF 0x1A EOF marker missing");

        // A separate NDX file is read-only. Reject an append rather than
        // advertising an index that silently omits the new record.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "indexed.dbf");
        fs::copy_file(fixtures / "customers.ndx", temp.path / "indexed.ndx");
        const auto before_indexed = content(temp.path / "indexed.dbf");
        xabl::Vm indexed_vm(output);
        expect_runtime_error(compiler, indexed_vm,
                             "USE indexed\nSET INDEX TO indexed\nAPPEND BLANK",
                             temp.path);
        require(content(temp.path / "indexed.dbf") == before_indexed,
                "indexed table modified despite rejected APPEND");
        indexed_vm.run(compiler.compile("SET INDEX TO\nAPPEND BLANK"), temp.path);
        require(xabl::DbfTable(temp.path / "indexed.dbf").reccount() == 4,
                "SET INDEX TO did not clear active index");

        // Do not truncate unknown trailers, even if they follow the last
        // physical record: fail without changing the damaged table.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "trailer.dbf");
        { std::ofstream tail(temp.path / "trailer.dbf", std::ios::binary | std::ios::app);
          tail << "unknown"; }
        const auto before_trailer = content(temp.path / "trailer.dbf");
        xabl::Vm trailer_vm(output);
        expect_runtime_error(compiler, trailer_vm,
                             "USE trailer\nAPPEND BLANK", temp.path);
        require(content(temp.path / "trailer.dbf") == before_trailer,
                "unknown DBF trailer was modified");

        // Legacy DBF files without an EOF byte can also be appended to.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "noeof.dbf");
        fs::resize_file(temp.path / "noeof.dbf",
                        fs::file_size(temp.path / "noeof.dbf") - 1);
        xabl::DbfTable no_eof(temp.path / "noeof.dbf");
        no_eof.append_blank();
        require(no_eof.reccount() == 4, "no-EOF DBF append failed");
        require(static_cast<unsigned char>(content(temp.path / "noeof.dbf").back()) ==
                    0x1A, "EOF terminator not restored");

        // Execute an actual legacy-style multi-work-area mini-application
        // on disposable DBF copies. It performs aggregation, updating,
        // appending, searching, filtering and independent area navigation.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "ledger.dbf");
        fs::copy_file(fixtures / "customers.dbf", temp.path / "branch.dbf");
        std::ostringstream app_output;
        xabl::Vm app_vm(app_output);
        app_vm.run(compiler.compile(content(fixtures / "legacy-mini-ledger.prg")),
                   temp.path);
        require(app_output.str() ==
                    "325.5\n4\nCharlie\nCharlie\n.T.\nDoris\nAlice\n",
                "legacy mini-application result mismatch: " + app_output.str());
        xabl::DbfTable ledger(temp.path / "ledger.dbf");
        require(ledger.reccount() == 4, "mini-app did not append a physical record");
        ledger.go_top();
        require(ledger.field("STATUS").as_string() == "REVIEW",
                "mini-app did not persist review status");
        ledger.go_bottom();
        require(ledger.field("NAME").as_string() == "Doris",
                "mini-app did not persist appended name");
        require(content(fixtures / "customers.dbf") == original,
                "mini-application wrote to source fixture");

        // SET EXACT is global runtime state and must also be respected
        // inside separately compiled work-area filter expressions.
        std::ostringstream exact_output;
        xabl::Vm exact_vm(exact_output);
        exact_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO NAME = 'Ali'\n"
            "GO TOP\n"
            "? EOF()\n"
            "SET EXACT ON\n"
            "GO TOP\n"
            "? EOF()\n"
            "SET EXACT OFF\n"
            "GO TOP\n"
            "? NAME\n"), temp.path);
        require(exact_output.str() == ".F.\n.T.\nAlice\n",
                "SET EXACT was not applied to active work-area filters: " +
                    exact_output.str());

        std::cout << "dBASE III append/navigation tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "append tests failed: " << ex.what() << '\n';
        return 1;
    }
}
