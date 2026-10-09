// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Brierfield Labs
// Legacy dBASE III PLUS APPEND BLANK and navigation regression tests.
// Always write to disposable copies of generated DBF fixtures.
#include <xabl/runtime/xabl.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
            "? TRIM(NAME)\n"
            "SET FILTER TO BALANCE > 100\n"
            "GO BOTTOM\n"
            "? TRIM(NAME)\n"
            "SET FILTER TO\n"
            "GO BOTTOM\n"
            "? TRIM(NAME)\n"
            "DELETE\n"
            "SET DELETED ON\n"
            "GO BOTTOM\n"
            "? TRIM(NAME)\n"
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
        require(reopened.field("NAME").as_string() == std::string("Doris") + std::string(15, ' '),
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
        require(ledger.field("STATUS").as_string() == std::string("REVIEW") + std::string(4, ' '),
                "mini-app did not persist review status");
        ledger.go_bottom();
        require(ledger.field("NAME").as_string() == std::string("Doris") + std::string(15, ' '),
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
            "? TRIM(NAME)\n"), temp.path);
        require(exact_output.str() == ".F.\n.T.\nAlice\n",
                "SET EXACT was not applied to active work-area filters: " +
                    exact_output.str());

        // A valid zero-record DBF has only a one-byte field terminator
        // after its descriptors. The loader must not demand 32 more bytes.
        const std::size_t header_size =
            static_cast<unsigned char>(original[8]) |
            (static_cast<unsigned char>(original[9]) << 8);
        std::string empty_bytes = original.substr(0, header_size);
        for (int index = 4; index < 8; ++index) empty_bytes[index] = '\0';
        empty_bytes.push_back(static_cast<char>(0x1A));
        const fs::path empty_path = temp.path / "empty.dbf";
        { std::ofstream file(empty_path, std::ios::binary);
          file.write(empty_bytes.data(), static_cast<std::streamsize>(empty_bytes.size())); }
        xabl::DbfTable empty(empty_path);
        require(empty.reccount() == 0, "empty DBF record count wrong");
        empty.go_top();
        require(empty.eof(), "zero-record DBF must be at EOF");
        empty.append_blank();
        require(empty.reccount() == 1 && empty.recno() == 1,
                "APPEND to zero-record DBF failed");
        require(empty.field("NAME").as_string() == std::string(20, ' '),
                "APPEND BLANK must retain an entirely space-padded C field");
        empty.replace("NAME", xabl::Value(std::string("First")));
        xabl::DbfTable reloaded_empty(empty_path);
        reloaded_empty.go_top();
        require(reloaded_empty.field("NAME").as_string() == std::string("First") + std::string(15, ' '),
                "first record in initially empty DBF not persisted");

        // Corrupt headers must fail safely before allocating the declared
        // record count or attempting to read past the actual file.
        const fs::path bad_count_path = temp.path / "hugecount.dbf";
        std::string bad_count = original;
        bad_count[4] = static_cast<char>(0xFF);
        bad_count[5] = static_cast<char>(0xFF);
        bad_count[6] = static_cast<char>(0xFF);
        bad_count[7] = static_cast<char>(0x7F);
        { std::ofstream file(bad_count_path, std::ios::binary);
          file.write(bad_count.data(), static_cast<std::streamsize>(bad_count.size())); }
        bool rejected = false;
        try { (void)xabl::DbfTable(bad_count_path); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "corrupt DBF record count was accepted");

        const fs::path missing_terminator = temp.path / "noterm.dbf";
        std::string bad_header = original;
        bad_header[header_size - 1] = 'X';
        { std::ofstream file(missing_terminator, std::ios::binary);
          file.write(bad_header.data(),
                     static_cast<std::streamsize>(bad_header.size())); }
        rejected = false;
        try { (void)xabl::DbfTable(missing_terminator); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "missing DBF field terminator was accepted");

        // An index can use any expression, not necessarily the name of the
        // replaced field. Until indexed writes are supported REPLACE must
        // never silently make the NDX stale.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "indexedit.dbf");
        fs::copy_file(fixtures / "customers.ndx", temp.path / "indexedit.ndx");
        const std::string edit_original = content(temp.path / "indexedit.dbf");
        xabl::Vm edit_vm(output);
        expect_runtime_error(compiler, edit_vm,
                             "USE indexedit\nSET INDEX TO indexedit\n"
                             "REPLACE NAME WITH 'BAD'", temp.path);
        require(content(temp.path / "indexedit.dbf") == edit_original,
                "REPLACE modified indexed DBF despite rejection");
        edit_vm.run(compiler.compile("SET INDEX TO\nREPLACE NAME WITH 'Good'"),
                    temp.path);
        xabl::DbfTable verified_edit(temp.path / "indexedit.dbf");
        verified_edit.go_top();
        require(verified_edit.field("NAME").as_string() == std::string("Good") + std::string(16, ' '),
                "REPLACE failed after index closed");
        require(content(fixtures / "customers.dbf") == original,
                "DBF integrity tests modified the original fixture");

        // The same character functions must execute in filter bytecode,
        // including nested calls and a nontrivial expression result.
        std::ostringstream text_filter_output;
        xabl::Vm text_filter_vm(text_filter_output);
        text_filter_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO UPPER(NAME) = 'BOB'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO LEN(TRIM(UPPER(NAME))) > 5\n"
            "GO TOP\n? TRIM(NAME)\n"), temp.path);
        require(text_filter_output.str() == "Bob\nCharlie\n",
                "character functions in filters failed: " +
                    text_filter_output.str());

        // Multiple arguments, nesting and one-based extraction must
        // execute identically inside SET FILTER bytecode.
        std::ostringstream slice_output;
        xabl::Vm slice_vm(slice_output);
        slice_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO LEFT(NAME,3) == 'Cha'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO SUBSTR(NAME,2,2) == 'ob'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO RIGHT(TRIM(NAME),1) == 'e'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SKIP\n? TRIM(NAME)\n"), temp.path);
        require(slice_output.str() == "Charlie\nBob\nAlice\nCharlie\n",
                "string slicing in filters failed: " + slice_output.str());

        std::ostringstream concatenation_output;
        xabl::Vm concat_vm(concatenation_output);
        concat_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO TRIM(NAME) + '!' == 'Charlie!'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO LEFT(NAME,1) + RIGHT(TRIM(NAME),1) == 'Ae'\n"
            "GO TOP\n? TRIM(NAME)\n"), temp.path);
        require(concatenation_output.str() == "Charlie\nAlice\n",
                "string concatenation in DBF filters failed: " +
                    concatenation_output.str());

        std::ostringstream ordering_output;
        xabl::Vm ordering_vm(ordering_output);
        ordering_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO NAME >= 'Bob'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SKIP\n? TRIM(NAME)\n"
            "SET FILTER TO NAME < 'Bob'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO NAME # 'Bob'\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SKIP\n? TRIM(NAME)\n"), temp.path);
        require(ordering_output.str() ==
                    "Bob\nCharlie\nAlice\nAlice\nCharlie\n",
                "character ordering in filters failed: " +
                    ordering_output.str());

        std::ostringstream search_output;
        xabl::Vm search_vm(search_output);
        search_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO AT('ob', NAME) > 0\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO AT('ar', NAME) > 0\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SET FILTER TO AT('not-here', NAME) > 0\n"
            "GO TOP\n? EOF()\n"), temp.path);
        require(search_output.str() == "Bob\nCharlie\n.T.\n",
                "AT substring search in filters failed: " +
                    search_output.str());

        std::ostringstream with_output;
        xabl::Vm with_vm(with_output);
        with_vm.run(compiler.compile(
            "USE branch\nGO TOP\n"
            "REPLACE STATUS WITH UPPER('with note')\n"
            "? TRIM(STATUS)\nUSE\n"), temp.path);
        require(with_output.str() == "WITH NOTE\n",
                "quoted WITH in REPLACE expression parsed incorrectly: " +
                    with_output.str());

        std::ostringstream replicate_filter_output;
        xabl::Vm replicate_filter_vm(replicate_filter_output);
        replicate_filter_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO RIGHT(TRIM(NAME),1) == REPLICATE('e',1)\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SKIP\n? TRIM(NAME)\n"
            "SET FILTER TO LEN(SPACE(2)) == 2\n"
            "GO TOP\n? TRIM(NAME)\n"), temp.path);
        require(replicate_filter_output.str() == "Alice\nCharlie\nAlice\n",
                "SPACE or REPLICATE inside filter failed: " +
                    replicate_filter_output.str());

        // An external writer changing a row after USE must not be silently
        // overwritten by a stale REPLACE or DELETE. The in-memory record
        // must also roll back when that optimistic write is rejected.
        const fs::path stale_path = temp.path / "stale.dbf";
        fs::copy_file(fixtures / "customers.dbf", stale_path);
        xabl::DbfTable stale(stale_path);
        stale.go_top();
        const std::string original_name = stale.field("NAME").as_string();
        require(!original_name.empty(), "stale-write fixture has no first name");
        const std::string stale_bytes = content(stale_path);
        const std::size_t stale_header =
            static_cast<unsigned char>(stale_bytes[8]) |
            (static_cast<unsigned char>(stale_bytes[9]) << 8);
        // NAME is the first field after the one-byte deleted flag.
        { std::fstream file(stale_path, std::ios::binary | std::ios::in | std::ios::out);
          file.seekp(static_cast<std::streamoff>(stale_header + 1));
          file.put('Z'); }
        const auto external_change = content(stale_path);
        bool stale_rejected = false;
        try { stale.replace("NAME", xabl::Value(std::string("Wrong"))); }
        catch (const std::runtime_error&) { stale_rejected = true; }
        require(stale_rejected, "stale REPLACE overwrote an external change");
        require(content(stale_path) == external_change,
                "stale REPLACE altered the on-disk DBF");
        require(stale.field("NAME").as_string() == original_name,
                "failed REPLACE left the in-memory row mutated");
        stale_rejected = false;
        try { stale.set_deleted(true); }
        catch (const std::runtime_error&) { stale_rejected = true; }
        require(stale_rejected, "stale DELETE overwrote an external change");
        require(!stale.deleted(), "failed DELETE left record deleted in memory");
        require(content(stale_path) == external_change,
                "stale DELETE altered the external DBF record");

        // Once the exact original preimage is restored, a normal update
        // succeeds, including persistence and the deleted flag.
        { std::fstream file(stale_path, std::ios::binary | std::ios::in | std::ios::out);
          file.seekp(static_cast<std::streamoff>(stale_header + 1));
          file.put(stale_bytes[stale_header + 1]); }
        stale.replace("NAME", xabl::Value(std::string("Fresh")));
        stale.set_deleted(true);
        xabl::DbfTable verified_stale(stale_path);
        verified_stale.go_top();
        require(verified_stale.field("NAME").as_string() == std::string("Fresh") + std::string(15, ' ') &&
                    verified_stale.deleted(), "recovered DBF update did not persist");

        stale.go_record(0);
        stale_rejected = false;
        try { stale.replace("NAME", xabl::Value(std::string("BOF"))); }
        catch (const std::runtime_error&) { stale_rejected = true; }
        require(stale_rejected, "REPLACE at BOF unexpectedly modified first record");

        // The writer must refuse header-count changes and arbitrary trailers
        // without overwriting an original row.
        const fs::path altered_header_path = temp.path / "altered-header.dbf";
        fs::copy_file(fixtures / "customers.dbf", altered_header_path);
        xabl::DbfTable altered_header(altered_header_path);
        altered_header.go_top();
        { std::fstream file(altered_header_path,
                           std::ios::binary | std::ios::in | std::ios::out);
          file.seekp(4);
          file.put(static_cast<char>(0xFF)); }
        const auto bad_header_bytes = content(altered_header_path);
        stale_rejected = false;
        try { altered_header.replace("NAME", xabl::Value(std::string("Wrong"))); }
        catch (const std::runtime_error&) { stale_rejected = true; }
        require(stale_rejected && content(altered_header_path) == bad_header_bytes,
                "DBF header drift was not rejected");

        const fs::path altered_trailer_path = temp.path / "altered-trailer.dbf";
        fs::copy_file(fixtures / "customers.dbf", altered_trailer_path);
        xabl::DbfTable altered_trailer(altered_trailer_path);
        altered_trailer.go_top();
        { std::ofstream file(altered_trailer_path,
                             std::ios::binary | std::ios::app);
          file.put('X'); }
        const auto bad_trailer_bytes = content(altered_trailer_path);
        stale_rejected = false;
        try { altered_trailer.set_deleted(true); }
        catch (const std::runtime_error&) { stale_rejected = true; }
        require(stale_rejected && content(altered_trailer_path) == bad_trailer_bytes &&
                    !altered_trailer.deleted(),
                "DBF trailer drift was not rejected cleanly");
        require(content(fixtures / "customers.dbf") == original,
                "write-safety tests modified the original DBF fixture");

        // Extreme navigation must saturate without signed overflow.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "wide-skip.dbf");
        xabl::DbfTable wide_skip(temp.path / "wide-skip.dbf");
        wide_skip.go_top();
        wide_skip.skip(std::numeric_limits<std::ptrdiff_t>::max());
        require(wide_skip.eof(), "large positive DBF SKIP missed EOF");
        wide_skip.skip(std::numeric_limits<std::ptrdiff_t>::min());
        require(wide_skip.bof(), "minimum signed SKIP missed BOF");
        wide_skip.skip(1);
        require(!wide_skip.bof() && wide_skip.recno() == 1,
                "SKIP 1 did not recover from BOF");
        wide_skip.skip(-1);
        require(wide_skip.bof(), "SKIP -1 did not enter BOF");
        wide_skip.skip(std::numeric_limits<std::ptrdiff_t>::max());
        require(wide_skip.eof(), "large SKIP from BOF missed EOF");
        wide_skip.skip(-1);
        require(wide_skip.recno() == 3, "SKIP -1 from EOF missed last row");

        std::ostringstream wide_output;
        xabl::Vm wide_vm(wide_output);
        wide_vm.run(compiler.compile(
            "USE wide-skip\nGO TOP\n"
            "SKIP 1e18\n? EOF()\n"
            "SKIP -1e18\n? BOF()\n"
            "SKIP 1\n? RECNO()\n"
            "SET FILTER TO BALANCE > 100\n"
            "SKIP 1e18\n? EOF()\n"
            "SKIP -1e18\n? BOF()\n"), temp.path);
        require(wide_output.str() == ".T.\n.T.\n1\n.T.\n.T.\n",
                "extreme filtered/unindexed SKIP state incorrect: " +
                    wide_output.str());

        std::ostringstream bad_skip_output;
        xabl::Vm bad_skip_vm(bad_skip_output);
        bad_skip_vm.run(compiler.compile("USE wide-skip\nGO TOP"), temp.path);
        expect_runtime_error(compiler, bad_skip_vm, "SKIP 1e100", temp.path);
        expect_runtime_error(compiler, bad_skip_vm, "SKIP -1e100", temp.path);
        bad_skip_vm.run(compiler.compile("? RECNO()"), temp.path);
        require(bad_skip_output.str() == "1\n",
                "invalid SKIP changed the record cursor");

        // Fixed-width C fields retain trailing DBF padding in expressions.
        // A user explicitly requests TRIM() when the field's actual text
        // length or an unpadded output is wanted.
        fs::copy_file(fixtures / "customers.dbf", temp.path / "fixed-width.dbf");
        xabl::DbfTable fixed_width(temp.path / "fixed-width.dbf");
        fixed_width.go_top();
        require(fixed_width.field("NAME").as_string() ==
                    std::string("Alice") + std::string(15, ' '),
                "DBF character field lost its declared 20-byte width");
        require(fixed_width.field("STATUS").as_string().size() == 10,
                "DBF status field did not retain its 10-byte width");
        std::ostringstream padded_output;
        xabl::Vm padded_vm(padded_output);
        padded_vm.run(compiler.compile(
            "USE fixed-width\nGO TOP\n"
            "? LEN(NAME)\n? LEN(TRIM(NAME))\n"
            "? LEFT(NAME,5)\n? RIGHT(NAME,1)\n? NAME\n"
            "? NAME = 'Alice'\n? NAME == 'Alice'\n"
            "? TRIM(NAME) == 'Alice'\n? LEN(UPPER(NAME))\n"
            "SET EXACT ON\n? NAME = 'Alice'\n"), temp.path);
        require(padded_output.str() ==
                    std::string("20\n5\nAlice\n \nAlice") +
                    std::string(15,' ') +
                    "\n.T.\n.F.\n.T.\n20\n.T.\n",
                "LEN, TRIM, RIGHT or displayed DBF padding is inconsistent");

        // The same conversion opcodes must work inside compiled DBF filters.
        std::ostringstream conversion_output;
        xabl::Vm conversion_vm(conversion_output);
        conversion_vm.run(compiler.compile(
            "USE branch\n"
            "SET FILTER TO VAL(TRIM(STR(BALANCE,10,2))) > 100\n"
            "GO TOP\n? TRIM(NAME)\n"
            "SKIP\n? TRIM(NAME)\n"
            "SET FILTER TO STR(BALANCE,10,2) == '    200.00'\n"
            "GO TOP\n? TRIM(NAME)\n"), temp.path);
        require(conversion_output.str() == "Alice\nCharlie\nCharlie\n",
                "STR/VAL in filters failed: " + conversion_output.str());

        std::cout << "dBASE III append/navigation tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "append tests failed: " << ex.what() << '\n';
        return 1;
    }
}
