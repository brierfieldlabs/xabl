# XABL handover: DBF integrity, safe navigation, fixed-width characters

Date: 9 October 2026
Status: all implementation changes merged and retested on main
Repository: brierfieldlabs/xabl
Workbench: git-workbench-brierfield via Remote Desktop Commander
Checkout: /home/workbench/xabl-design

## Exact continuation point

- Resume feature development from main, NOT the handover history branch.
- Authoritative main commit: 0b93d895a09d83c4fe29354ddc509f40bddde8a6
- Authoritative main tree: 03e63f38a3b75b86c4c63de45d35c9c68f282b6f
- Handover branch: history/handover-2026-10-09-dbf-integrity-fixed-width
- C++ source and header physical LOC: 4,576
- C++ nonblank LOC: 4,092
- Earlier 9 October baseline: 2,216 physical C++ LOC
- Increase from initial baseline: +2,360 physical C++ LOC

## Verification of latest merged main

On the connected workbench, the exact main commit above was fetched from
GitHub. CMake build and ordinary CTest passed all eight suites (8/8).
A fresh separate Debug build with AddressSanitizer and UBSan also passed all
eight suites (8/8), with no sanitizer errors reported. The disposable
build-sanitized directory was removed after successful verification.

CTest suites:
1. xabl_fixture_generate
2. xabl_smoke
3. xabl_expressions
4. xabl_append
5. xabl_control_flow
6. xabl_profiles
7. xabl_ndx_navigation
8. xabl_source_comments

Main and remote main matched at the exact commit/tree listed above, with
a clean worktree.

## Work completed in the latest continuation

- PR #20: https://github.com/brierfieldlabs/xabl/pull/20
  DBF record updates now validate the on-disk preimage of a record plus
  the header layout, record count, physical file size and optional EOF byte
  before REPLACE/DELETE/RECALL. Rejected writes restore the cached record.
  Added tests with external record modification, header/trailer drift,
  recovered writes, and denial of REPLACE at BOF.
  Important: optimistic validation is NOT cross-process file locking.
- PR #21: https://github.com/brierfieldlabs/xabl/pull/21
  Removed signed integer overflow in DbfTable::skip; saturated oversized
  valid positive/negative movements to EOF/BOF; checked VM double-to-
  ptrdiff_t conversion; avoided negating PTRDIFF_MIN in filtered SKIP.
  Tests cover extreme physical, filtered and NDX-indexed navigation.
- PR #22: https://github.com/brierfieldlabs/xabl/pull/22
  DBF III PLUS C fields now preserve their descriptor-declared width,
  including all trailing ASCII spaces, rather than silently trimming on
  read. LEN(NAME) now returns 20 for the synthetic 20-byte NAME field,
  while LEN(TRIM(NAME)) returns 5 for Alice. Raw printing, RIGHT(),
  comparison and SET EXACT behaviour is now regression-tested.
  Synthetic PRG fixtures that intend unpadded output use explicit TRIM().
  All 17 changed files in PR #22 were Git blob SHA-verified before merge,
  and the reconstructed remote Git tree matched the local tested tree.

Earlier milestones PR #1 through #19, including dialect fail-closed profiles,
compiler/parser, VM, DBF/NDX physical navigation, string functions and safe
SEEK semantics, are recorded in:
history/xabl-handover-2026-10-09-runtime-parser-development.md
on the earlier handover branch history/handover-2026-10-09-runtime-parser-strings.

## Compatibility governance

XABL is still the public repository, program and source name. DB++ remains
the preferred future brand but legal/trademark clearance is unfinished.
Do not rename code, repo or public metadata until explicit approval.

Maximum practical compatibility is the target for EACH historical dialect,
not a generic blend. dBASE III PLUS currently has an incomplete executable
subset. Other profile identifiers remain fail-closed, not implemented.
Do not use later dBASE IV syntax (such as SCAN/ENDSCAN) by default in the
dBASE III PLUS profile without version-specific evidence.

## Known gaps and prioritised next work

1. Original-product interoperability corpus: obtain or create reference
   dBASE III PLUS programs, verify genuine DBF/NDX binary outputs, and
   record both input and expected outputs for later dialect profiles.
   Current fixtures are synthetic.
2. Genuine DOS codepage handling, character casing and collation: current
   character comparisons are unsigned bytewise, case conversion ASCII-only.
3. NDX remains read-only. Indexed REPLACE/APPEND is refused to avoid stale
   indexes; genuine index maintenance, collation, duplicate keys and rebuild
   semantics require a separate design.
4. DBF writes remain single-writer only. Add reliable record/file locking,
   transactional/recovery policy, failure injection and cross-process tests
   before using real/shared DBF files. Preimage checks are not atomic.
5. Date, memo/DBT, unknown field types, field-domain validation, null and
   legacy numeric formatting/coercions need dialect-labelled slices.
6. Interpreter/compiler resource limits: bound runaway loops, malformed
   nested input, huge source programs, stack growth and error reporting;
   add fuzzed DBF/NDX and parser fixtures with sanitizers.
7. Character printing now correctly includes padding. Never globally trim
   test output; use TRIM in scripts when compact output is intended.
8. More source syntax: macro expansion, semicolon line continuations,
   procedure/function scope, selected historically verified functions.
9. No live deployment/upgrade or binary rewrite is authorised by this
   handover. Keep original uploaded/legacy binary inputs byte-for-byte.

## Developer workflow

- Use feature branches from current main, document architectural decisions
  and public APIs, run tests, then merge by PR into main.
- GitHub org: brierfieldlabs. Do not use GitLab for this project.
- Local laptop is not the CI runner. Preferred CI remains CT141/CT140.
- GitHub upload via connected API was used because workbench Git push
  authentication was not available. Verify EVERY uploaded blob SHA and
  reconstructed Git tree against local Git before merging; a previous
  chunked upload was truncated.
- Fetch the remote branch back to the workbench, rebuild, run CTest, ensure
  no untracked build residue and compare HEAD/tree.
- Major handovers remain on a distinct history branch.
- Scheduled ChatGPT night-shift canary previously ran without creating
  either the Remote Desktop Commander marker or GitHub issue. That old
  failure was not retested this session; do not depend on scheduled tasks
  to perform connected GitHub/Commander development.
