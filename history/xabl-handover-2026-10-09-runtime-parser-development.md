# XABL runtime/parser development handover

Date: 2026-10-09
Project: Brierfield Labs XABL, an open-source C++23 dBASE/xBase-inspired runtime
Status: fully merged and tested; no public product rename

## Authoritative continuation point

- Repository: `brierfieldlabs/xabl`
- Exact source continuation branch: `main`
- Exact main commit: `95e6ba880736edc1355b4f6b800fed65733c48df`
- Exact main Git tree: `5ef94fe32e780d08f030c03f190b22e1229a1512`
- Workbench device: `git-workbench-brierfield` via Remote Desktop Commander
- Workbench checkout: `/home/workbench/xabl-design`
- Separate history branch: `history/handover-2026-10-09-runtime-parser-strings`
- This handover branch is archival documentation only; work should resume from
  **the exact main commit above**, not from the handover commit.

## Verification and LOC

All merges were re-fetched from GitHub to the workbench. The latest `main`
was rebuilt from commit `95e6ba8`; CTest **8 of 8 passed**. A fresh separate
GCC 14.2.0 Debug build with AddressSanitizer and UndefinedBehaviorSanitizer
ran all eight test suites successfully with **no sanitizer errors reported**.

Sanitizer CMake flags:
`-fsanitize=address,undefined -fno-omit-frame-pointer` and linker
`-fsanitize=address,undefined`; tests used
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`.

The disposable `build-sanitized/` directory was removed afterwards.
Workbench main and its remote match; tracked/untracked worktree is clean.

C++ source plus header count (`git ls-files '*.cpp' '*.hpp' | xargs cat | wc -l`):
**4,191 physical lines**; **3,725 non-blank lines**.
Earlier baseline was 2,216, therefore +1,975 lines during development.

Test groups:
1. `xabl_fixture_generate`: synthetic DBF/NDX fixture creation
2. `xabl_smoke`: existing legacy DBF/NDX/work-area regression
3. `xabl_expressions`: precedence, functions, comparisons, syntax errors
4. `xabl_append`: writable DBF and multi-work-area app/filter regressions
5. `xabl_control_flow`: nested branches and DO WHILE/EXIT/LOOP
6. `xabl_profiles`: dialect stamps and unsupported-profile refusal
7. `xabl_ndx_navigation`: shuffled indexed order, seek, corrupt pages
8. `xabl_source_comments`: NOTE/*/&& comments, quoted clauses and compact output

## Landed work in this continuation

All following merged to `main` via SHA-verified GitHub text-blob/tree
transfers followed by workbench re-fetch and test:

- PR #11: `LEN, UPPER, LOWER, TRIM/RTRIM, LTRIM` in VM and filters.
  https://github.com/brierfieldlabs/xabl/pull/11
- PR #12: `LEFT, RIGHT, SUBSTR` (2/3 argument forms), nested calls.
  https://github.com/brierfieldlabs/xabl/pull/12
- PR #13: character `+` and `-` with trailing-blank behaviour; numeric
  arithmetic unchanged and mixed-type operations currently fail explicitly.
  https://github.com/brierfieldlabs/xabl/pull/13
- PR #14: quote-aware `&&`, `*`, `NOTE` comments and compact `?expr`;
  preserved line diagnostics and dedicated PRG fixture.
  https://github.com/brierfieldlabs/xabl/pull/14
- PR #15: bytewise character relational comparison `<, >, <=, >=` and
  `#` not-equal; filtered record visibility uses same operator logic.
  https://github.com/brierfieldlabs/xabl/pull/15
- PR #16: two-argument one-based `AT(needle, haystack)` function, including
  filters and nested expressions.
  https://github.com/brierfieldlabs/xabl/pull/16
- PR #17: quote-aware and nesting-aware parsing of `STORE ... TO`,
  `REPLACE ... WITH`, and `USE ... ALIAS` clauses, with regressions.
  https://github.com/brierfieldlabs/xabl/pull/17

Prior existing milestones from the previous handover: runtime subsystem split
(PR #1), tokenised precedence parser (#3), physical DBF APPEND BLANK/close/
GO BOTTOM (#4), SET EXACT (#5), structured control flow (#6), dialect
selection (#7), DBF validation (#8), NDX ordered navigation (#9), and
corrupt NDX SEEK rejection (#10).

The architecture and dialect notes live in `docs/developer/architecture/`,
`docs/developer/compatibility/` and `docs/decisions/`; source API symbols
remain XABL. Legacy .PRG tests are in `tests/fixtures/`.

## Active project and naming policy

- DB++ is the user's preferred future public name, **not legally cleared**.
  No source, repository, binary or public metadata was renamed.
- Keep existing `xabl` identifiers until authoritative UK/EU/US and related
  trademark searches, prior German db++ ownership checks and explicit
  product-name approval are completed.
- Earlier DABL candidate was rejected. Do not restart with unrelated
  invented/person-name brands without user request.
- Maintain maximum practical compatibility with **each named historical
  legacy version separately**, not a single drifting compatibility target.
  Currently **only dBASE III PLUS has an executable subset**.
  dBASE IV, Clipper, FoxPro, Visual FoxPro, dBASE PLUS, Harbour etc. are
  reserved profiles that currently fail closed.

## Known compatibility and safety gaps (must not overclaim)

1. DBF `C` fields currently become right-trimmed `Value` strings; original
   fixed-width dBASE behaviour, e.g. `LEN(field)`, may require preserving
   field width. Compare with true historical reference programs *before*
   changing semantics.
2. `SET EXACT`, `==` strict comparison, padded fields, ordered comparisons,
   string/case handling and mixed-type conversions vary between dialects.
   Current string ordering is unsigned bytewise and ASCII casing only.
3. Historical codepages, memo DBT fields, date fields, nullable values,
   date arithmetic, user functions/procedures, macro substitution and
   semicolon line continuation are not implemented.
4. `SCAN ... ENDSCAN` is dBASE IV-era syntax and MUST NOT be enabled
   implicitly in the default dBASE III PLUS profile.
5. NDX is read-only; indexed DBF APPEND/REPLACE is explicitly refused.
   NDX ordering and SEEK are protected from malformed page cycles and
   row references but are not independently certified against real
   historical binary indexes.
6. DBF updates are single-writer only; there is no record locking, atomic
   multi-record transaction, recovery journal or concurrent refresh.
   Do not operate on real/shared publishing/business DBF data.
7. Review unsuccessful `SEEK` cursor semantics: the current VM may send
   a failed search to physical BOF (`go_record(0)`) rather than the expected
   EOF boundary. Add an executable legacy fixture and verify against
   authoritative dBASE III PLUS documentation before changing behaviour.
8. The expression lexer and line-oriented statement parser remain an
   evolving subset; no complete grammar, source map, or serialized versioned
   bytecode/package profile lock exists yet. Check numeric overflow,
   recursion limits, bounded input size and long-running loop budgets.
9. The fixtures are synthetic. Independent actual historical dBASE III PLUS
   programs and DBF/NDX samples are still needed to verify authenticity.
10. Sanitizer success covers the current test corpus only, not all fuzzed
    or concurrent inputs. Add file-format fuzzing and failure-injection tests.

## Suggested next engineering batches

- Verify failed SEEK EOF/BOF/FOUND() semantics with a real reference program
  and add a regression before correcting the cursor.
- Add explicit DBF character field width metadata/policy and test LEN()/
  TRIM()/comparison behaviour of padded field values under SET EXACT.
- Build a locked, version-labelled compatibility test matrix for dBASE
  III PLUS versus IV, Clipper, FoxPro, Visual FoxPro etc.; obtain genuine
  output/reference data rather than inferring from modern products.
- Add controlled syntax support (line continuations, selected functions
  such as SPACE/REPLICATE after verifying vintage availability, ELSEIF if
  appropriate for the selected profile).
- Expand safe DBF/NDX handling: transactional writes, file locking and
  a deliberate index-maintenance/rebuild design; do not silently write to
  live indexed data.
- Add CTest fixtures for invalid scripts, corrupt inputs, boundary dates
  and codepages, then repeat ASan/UBSan and consider fuzz builds.

## Operations and publication rules

- Workbench: `/home/workbench/xabl-design`; CI should run on approved
  CI containers (CT141/CT140), not the user's laptop.
- Use feature branches + tests + SHA-verified GitHub blob/tree transfers,
  re-fetch and compare HEAD/tree after upload, then PR/review/merge.
  Remote workbench Git HTTPS push credentials were unavailable, so we used
  connected GitHub create_blob/create_tree/create_commit/update_ref methods.
  Large text uploads have previously been truncated; ALWAYS compare each
  uploaded blob SHA with its local `git ls-tree` SHA before publication.
- No unrequested deployment, no rewriting old original binary files, and
  preserve exact-byte originals.
- Historical canary finding, **not retested during this session**: ChatGPT
  scheduled task ran but wrote neither Commander marker nor GitHub issue,
  therefore the 'night shift' cannot be relied on for connected plugin work.
- Keep running documentation and handovers, with a distinct history branch
  and a clean `main` for continuation.
