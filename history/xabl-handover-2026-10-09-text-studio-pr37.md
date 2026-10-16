# XABL / proposed DB++: Text Studio development handover

**Date:** Friday 9 October 2026, UK time
**Status:** Completed, verified mainline; handover branch contains this document only
**Repository:** https://github.com/brierfieldlabs/xabl
**Exact continuation branch:** main
**Exact continuation HEAD:** 3f9603c96c8fde7555c9ef48b2251123fbe1136b
**Exact continuation tree:** be69b9bc992c320104dbf9ba0e477bdd22084617
**Handover branch:** history/handover-2026-10-09-text-studio-pr37
**Handover file:** history/xabl-handover-2026-10-09-text-studio-pr37.md
**CMake project version:** 0.0.1, pre-release; no new release tag made.

## 1. Mandatory starting context

Continue engineering the public, C++23 xBase/dBASE successor as a serious
evolution of dBASE with version-labelled compatibility, not a loosely
inspired new language. The source, application and GitHub project are
currently named XABL. **DB++ is only a proposed product name**, not yet
trademark-cleared or approved for a repository/namespace/binary rename.
Keep the original dBASE III PLUS profile historically accurate where
verifiable; add later dBASE IV, Clipper, FoxPro, Visual FoxPro, Visual
dBASE/dBASE PLUS, Harbour and xHarbour compatibility as separate
profiles and tested increments. Never assert full legacy compatibility
from synthetic fixtures alone.

Working machine is the dedicated Git workbench
git-workbench-brierfield (Remote Desktop Commander device
07c1d400-9390-48f2-99c8-e7f1f531a7ee).
Authoritative checkout: /home/workbench/xabl-design.
Do not run Git operations on the user's laptop, and use the
brierfieldlabs GitHub organisation, **not GitLab**.

The earlier isolated worktree /home/workbench/xabl-text-studio remains
at older feat/dos-text-studio commit 7daef03. It is *not* authoritative;
do not continue new development from that older feature head or
delete the worktree without checking that no other workflow needs it.

## 2. Exact latest mainline status

- Main HEAD: 3f9603c96c8fde7555c9ef48b2251123fbe1136b.
- Main Git tree: be69b9bc992c320104dbf9ba0e477bdd22084617.
- Verified clean workbench checkout and matching origin/main.
- 91 tracked files.
- Physical C++ .cpp + .hpp LOC: **7,177**.
- Nonblank C++ .cpp + .hpp LOC: **6,533**.
- CTest: **13/13 normal tests passed** on this exact merged main HEAD.
- CTest: **13/13 debug ASan/UBSan tests passed** on this exact HEAD,
  with no reported sanitizer errors.
- An additional **50 consecutive Text Studio pseudo-terminal integration
  runs** passed for the test-only startup redraw fix before PR #37 merge.
- The final sanitizer build folder was removed after qualification;
  normal build folder remains on the workbench.
- No runtime release or packaged DOS executable was produced.

The currently verified progression includes:
- PR #31 native typed dBASE III PLUS dates:
  https://github.com/brierfieldlabs/xabl/pull/31
- PR #32 initial optional ncurses Text Studio:
  https://github.com/brierfieldlabs/xabl/pull/32
- PR #33 typed date arithmetic:
  https://github.com/brierfieldlabs/xabl/pull/33
- PR #34 syntax colouring and F2 directory browser:
  https://github.com/brierfieldlabs/xabl/pull/34
- PR #35 pseudo-terminal redraw assertion stabilisation:
  https://github.com/brierfieldlabs/xabl/pull/35
- PR #36 Ctrl+G numbered-line navigation and F8 compiler-error jump:
  https://github.com/brierfieldlabs/xabl/pull/36
- PR #37 fix for merged startup title/footer redraw assertion race:
  https://github.com/brierfieldlabs/xabl/pull/37

## 3. Text Studio: implemented

The program xabl-tui is an optional, curses-backed, DOS-inspired full
screen text IDE using the **same** shared C++ compiler, bytecode, DBF
engine and VM as the command-line xabl program. It is not an MS-DOS
binary or an alternative language interpreter.

The UI includes white-on-blue editing with cyan panels, numbered lines,
position and modified flags, editable PRG/XABL source, ordinary and
CRLF source preservation, bounded edit/undo support, find/search,
syntax colours for commands/functions/numbers/logicals/strings/comments,
captured program output and confirmation before executing scripts.
Saving stages a sibling file before replacement and refuses symlink
targets. This does not guarantee fsync-based crash durability.

F2 is a keyboard directory browser: directories first, then PRG/XABL
files, Enter selects/opens, Backspace parent, Esc cancels. It excludes
symlinks and unrecognised/non-regular files, and neutralises terminal
control bytes in display names. Ctrl+O opens a path by typing it.
Browser is read-only and does not execute files.

Current important keys:
- F1: help.
- F2: directory/file browser; Ctrl+O: open by path.
- F3: new buffer; F4 or Ctrl+S: save.
- F5: compile and run with explicit confirmation (can write DBFs).
- F6: toggle editor and captured output.
- F7 / Ctrl+F: find; F8: next find or jump from compiler error output.
- F9: compile-only check; no program execution.
- F10 / Ctrl+Q: exit, prompting about unsaved source.
- Ctrl+G: go to a numbered source line.
- Ctrl+Z: undo; cursor/editing/navigation keys as in the user guide.
- Terminal IXON/IXOFF disabled for Ctrl+S/Q shortcuts while running.

Key source:
- apps/xabl-tui/main.cpp
- apps/xabl-tui/editor_buffer.hpp and editor_buffer.cpp
- apps/xabl-tui/syntax_highlight.hpp and syntax_highlight.cpp
- apps/xabl-tui/file_browser.hpp and file_browser.cpp
- docs/user/text-studio.md
- docs/decisions/2026-10-09-dos-text-studio.md
- docs/decisions/2026-10-09-text-studio-highlighting-browser.md
- docs/decisions/2026-10-09-text-studio-source-line-navigation.md

The editor model, syntax highlighting and file browser are individually
tested without curses; the actual ncurses UI also has a pseudo-terminal
end-to-end integration test.

## 4. Compiler, VM and database state

The modularised C++23 runtime currently executes the working subset of
dBASE III PLUS programs: arithmetic and logical expressions, variables,
IF/ENDIF, DO WHILE/ENDDO, printing, comments, USE, table SELECT via
numeric 1..10 / letters A..J / table alias, alias->field references,
DBF III C/N/F/L/D fields, appending blank records, navigating physical
and index order, NDX index loading, SEEK, FOUND, GO, SKIP, EOF/BOF,
RECNO/RECCOUNT, LOCATE and CONTINUE, FILTER, SET DELETED, REPLACE,
DELETE and RECALL. DateValue is a distinct runtime type.

Current functions and operators include character operations and
numeric conversions (VAL, STR, ABS, INT, MIN, MAX, ASC, CHR, STUFF,
AT, LEFT, RIGHT, SUBSTR, TRIM variants, SPACE, REPLICATE, LEN,
and related functions), $ containment, plus CTOD, DTOC, DTOS,
YEAR, MONTH, DAY and typed date +/- day / date differences.

Do not mistake partial support for production parity. Explicit known
gaps/limits:
- DBT memo handling and writable M fields are not implemented; refuse
  unsupported field types instead of interpreting them as strings.
- DOS codepages CP437/CP850, character collation and Unicode-aware
  editing are not implemented.
- SET DATE / CENTURY / EPOCH, dynamic DATE(), blank date comparison
  quirks and all historical-version date behaviours need review.
- ROUND historical behaviour was deliberately deferred due to original
  III PLUS quirks. Do not substitute arbitrary modern rounding.
- Record-level, cross-process and concurrent DBF/NDX locking and safe
  index maintenance remain incomplete. **Never test against valuable,
  shared or live customer data.**
- Real DOS, Clipper and FoxPro reference fixtures and genuine historical
  interpreter comparisons are still needed to justify compatibility.
- Future dialect profiles must not inherit the dBASE III PLUS ten-area
  limit or other version-specific rules without evidence.

Compatibility docs: docs/developer/compatibility/ and docs/decisions/.

## 5. Tests and safe starting commands

Run in the workbench checkout, not on a user's laptop:

~~~sh
cd /home/workbench/xabl-design
git fetch origin
git switch main
git merge --ff-only origin/main
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/xabl-tui --help
~~~

Where ncurses is installed, start the IDE using:

~~~sh
./build/xabl-tui
./build/xabl-tui path/to/example.prg
~~~

To run the independently verified sanitizer profile (normally clean
the generated build directory afterwards):

~~~sh
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitized -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ctest --test-dir build-sanitized --output-on-failure
~~~

13 suites at handover: fixture generation, smoke, expressions, append,
control flow, profile, NDX navigation, source comments, typed dates,
editor model, syntax-highlighter, file-browser and curses PTY test.

Synthetic fixtures may be generated under tests/fixtures. The IDE PTY
test uses temporary files and runs harmless arithmetic. It must not
open or rewrite real user databases.

## 6. Latest incident and mitigation

The final handover review discovered that the PTY integration suite
could sporadically time out even while its target text was visibly in
the terminal transcript. ncurses sometimes writes multiple screen
elements in the **same** PTY read. The prior test started a new text
search after already consuming the title redraw. PR #37 changed the
startup title and footer assertions to search the same original
output window. This is test-harness-only. It passed 50 repeated PTY
runs and the full CTest suite, both before merge and after fetching
the published fix. PRs #35/#37 record earlier related fixes.

Always inspect PTY transcript bytes before classifying a missing text
assertion as an IDE bug. Keep action-scoped capture markers to avoid
matching stale content, and test real terminal key sequences in ncurses
application-cursor mode. No code change is justified by a flaky test
until the failure is reproduced and understood.

The Remote Desktop Commander shell may emit
"sh: 0: getcwd() failed: No such file or directory" because the
connector's inherited working directory no longer exists. Explicit
cd /home/workbench/xabl-design commands still execute successfully.
This warning has not altered checkout or test results.

## 7. Suggested next development slice

Primary: Text Studio integrated debugging, starting with an explicit
source-location mapping in the compiler/bytecode and a **read-only
breakpoint model**. Then controlled stepping/continue, variable watch,
call/record state and error navigation, with deterministic fixtures
and headless tests before binding keyboard actions. Keep user data
protected: no implicit program execution while inspecting source.

Other subsequent priorities:
1. Project/workspace manifest and browser rather than file listing only.
2. Safe editor experience (syntax-aware highlighting parity, multi-file
   state, undo grouping, UTF-8/DOS codepage-aware rendering).
3. Proper DBF/DBT typed memo support, indexing/locking correctness and
   roundtrip fixtures from original programs.
4. Cross-platform Windows text frontend, Qt graphical Studio sharing
   core model, packaging and upgrade/CI targets.
5. Real historic dBASE dialect regression corpus with explicit profile
   differences; continue API/ADR/format documentation.
6. Final name checks on proposed DB++ before renaming any public APIs.

Keep development work on small, tested topic branches. Do not create
another unrelated handover unless requested.

## 8. GitHub publishing mechanism and integrity controls

Remote HTTPS Git push from the workbench has previously been unable to
authenticate. The connected **GitHub app** can create branches, blobs,
trees, commits and update refs and open/merge PRs. For publication:
1. Commit and fully test locally first; record the local HEAD/tree SHA.
2. Use the remote file reader to retrieve the complete source bytes;
   it has a ~1,000-line cap, so retrieve longer files in explicit
   offsets/chunks, preserve exact line endings, and verify blob SHAs.
3. Rebuild the GitHub tree from the exact base tree and uploaded blobs.
   Assert it matches the tested local Git tree **before** moving the ref.
4. Fetch the published branch into workbench and rerun tests; only then
   open/merge the PR.
5. Verify final merged origin/main and the workbench checkout match.

The scheduled-task connector canary did not reliably access GitHub and
Remote Desktop Commander on scheduled runs. Do not claim any background
coding or CI execution without verified connector access.

## 9. Explicit end state for next room

**Resume from main commit**
3f9603c96c8fde7555c9ef48b2251123fbe1136b.
That commit includes Text Studio PRs #32, #34, #36, the PTY fixes
#35/#37, and typed dates/arithmetic PRs #31/#33.

The history/handover branch is documentation only. No source changes
exist beyond the already merged PR #37, no Linux ISO or other unrelated
project was touched, and no deployment was attempted.

Before making any new changes, fetch main, verify its SHA and clean
worktree, then create a fresh feature branch from main. If main has
legitimately advanced, reconcile that newer history first rather than
resetting it to the older handover base.
