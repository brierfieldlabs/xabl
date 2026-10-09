# XABL / successor-name handover — 9 October 2026

## Authoritative continuation point

- Repository: `brierfieldlabs/xabl` (public)
- Main branch continuation base: `eeaeb018fd574d2f7e34df05b19f35980f684eeb`
- Handover branch: `history/handover-2026-10-09-xabl-bootstrap-naming`
- Workbench: `git-workbench-brierfield`
- Local checkout: `/home/workbench/xabl-design`
- Licence: GPL-3.0-or-later
- Attribution: Brierfield Labs

The project is a new open-source cross-platform xBase/dBASE-family programming language and application platform. The working name remains **XABL** for repository/source naming until a replacement name is chosen. Do not rename the repository or source tree yet.

## Naming status

The user is **not struck with XABL** and asked for a better language name.

Current expansion:
- XABL = eXtensible Application Base Language

Alternative considered:
- DABL = Data Application Base Language

DABL was rejected after a collision sweep. Relevant problems:
- an existing Python package named `dabl` (Data Analysis Baseline Library);
- previous Digital Asset DABL developer-platform usage;
- active `dabl.io` software/fintech branding;
- a fresh US standard-character `DABL` trademark filing dated 2 October 2026 in Class 42 for software/technology services;
- other software/research GitHub uses.

The user agreed the next step is a **brandability-first naming pass**. Do not simply invent raw acronyms. Produce a shortlist of roughly 10–15 candidates only after checking:
1. obvious software/programming/company collisions;
2. GitHub usage;
3. package ecosystems;
4. domain plausibility;
5. obvious trademark/brand risks;
6. whether the name sounds natural with “Studio”, “Runtime”, “Server”, “SDK”, etc.

Naming principle agreed in conversation:
> Find a name that sounds like a language first; give it a sensible expansion second.

Until a new name is approved, continue to use XABL internally.

## Governing product philosophy

Hard rule:
> XABL is dBASE evolved, not a generic modern language inspired by dBASE.

Also locked:
> Preserve the dBASE way of programming. Modernise the environment around it.

> Maximum legacy compatibility is a hard rule.

Do not turn each historical dBASE semantic quirk into a user decision. For each selected compatibility profile, reproduce that dialect/version as faithfully as practical.

Compatibility roadmap:
1. dBASE III PLUS
2. dBASE IV
3. Clipper
4. FoxPro
5. Visual FoxPro
6. Visual dBASE
7. dBASE PLUS/dBL
8. Harbour
9. xHarbour
10. modern XABL

## Architecture already agreed

- Implementation language: **C++23**
- Style: restrained/maintainable C++, simple value types, RAII, explicit ownership, no template acrobatics or inheritance jungles
- Stable native extension boundary: **C ABI**, not C++ ABI
- UI: Qt 6, Qt Widgets first
- Compiler/runtime owns parser, compatibility semantics, IR/bytecode, VM, runtime, debugger
- LLVM only for native code generation
- Stack-based versioned VM initially
- CMake + Ninja
- Windows/Linux first, ARM64 later
- Runtime UI-independent
- Studio/CLI separate
- OCI/Docker official deployment target
- Multiple runtimes and compatibility packs preserved indefinitely
- TOML manifest + exact lockfile
- Modules/plugins for anything that does not need to live in minimal core
- Security/provenance robust but quiet in ordinary UX

Do not reopen C versus C++ unless the user explicitly revisits it. We checked C, Rust and COBOL and retained C++23.

## Current implementation state

The compiler/runtime is now a real executable vertical slice, not just design documentation.

Implemented language/runtime features include:
- line-oriented compiler into stack VM instructions
- literals: numeric, string, logical
- variables via `STORE ... TO` and direct assignment
- arithmetic `+ - * /`
- comparisons `> < = == != <> >= <=`
- unary numeric signs
- logical operators:
  - `.NOT.` and `NOT`
  - `.AND.` and `AND`
  - `.OR.` and `OR`
- parenthesised expression grouping
- `IF ... ENDIF`
- `DO WHILE ... ENDDO`
- `? expression`
- `USE`
- `USE ... ALIAS ...`
- `SELECT n`
- `SELECT alias`
- alias-qualified `alias->field`
- `SET INDEX TO`
- `SEEK`
- `FOUND()`
- `GO TOP`
- `GO n`
- `SKIP`
- signed `SKIP n`
- `EOF()`
- `BOF()`
- `RECNO()`
- `RECCOUNT()`
- `REPLACE ... WITH ...`
- `DELETE`
- `RECALL`
- `DELETED()`
- `LOCATE FOR ...`
- `CONTINUE`
- `SET FILTER TO ...`
- clearing a filter with bare `SET FILTER TO`
- `SET DELETED ON`
- `SET DELETED OFF`

## DBF / index behavior already working

### DBF
Real dBASE III DBF reader/writer.

Current behavior:
- reads dBASE III-compatible table headers/field descriptors
- character, numeric/float and logical field support
- physical records preserved exactly
- deletion marker preserved
- `REPLACE` persists changes to the DBF
- `DELETE` / `RECALL` persist the record deletion marker
- physical record numbering maintained
- direct `GO n` addresses physical rows

### NDX
Read-only dBASE III-style NDX implementation:
- 512-byte header/page model
- root page
- key length/type/record length
- expression
- B-tree traversal
- character and numeric key comparison path
- NDX returns physical DBF record number
- `SEEK` wired through work areas

The current NDX fixture is generated according to researched format. It should eventually be verified against independently-created genuine dBASE NDX files.

## Work areas / visibility semantics

Work areas are first-class:
- independent table/index/current-record state
- alias support
- independent FOUND state
- `SELECT` by area or alias
- alias-qualified fields

Filter behavior currently follows classic dBASE semantics:
- filters are per work area
- setting a filter does not immediately move the record pointer
- `GO TOP` and sequential `SKIP` honor the filter
- direct `GO n` may still land on a filtered-out physical record
- clearing filter restores ordinary navigation

`SET DELETED` currently:
- defaults OFF
- is global runtime state, not per work area
- when ON, ordinary sequential navigation skips deleted records
- direct `GO n` can still land on a deleted physical record
- filter visibility and deleted visibility compose through one visibility predicate

## Tests / fixtures

Current fixture set includes:
- `legacy-customer-review.prg`
- `legacy-seek.prg`
- `legacy-total.prg`
- `legacy-work-areas.prg`
- `legacy-record-status.prg`
- `legacy-locate.prg`
- `legacy-filter.prg`
- `legacy-logical.prg`
- `legacy-set-deleted.prg`

Generated customer DBF/NDX fixtures are produced by Python tooling and ignored from Git.

Latest full local verification before this handover:
- build succeeded
- CTest fixture-generation test passed
- smoke test passed
- total: 2/2 tests, 0 failures

## Latest LOC checkpoint

At main `eeaeb018...`:

- C++: **2,216 total lines**, 1,854 non-blank
- Python test tooling: **113**
- Legacy/XABL fixtures: **169**
- Docs/licence: **1,245**
- Other text: **16**
- Whole repo: **3,759 total lines**, 3,118 non-blank

The C++ implementation was 1,456 lines at the first LOC question in this room, so it grew by 760 lines during this development session.

Continue surfacing LOC at meaningful checkpoints. The user explicitly asked for that.

## Recent commits on main

Newest first:
- `eeaeb018` feat: add dBASE SET DELETED semantics
- `2d59885` feat: add parenthesised expression grouping
- `f9183d3` feat: add dBASE logical expression operators
- `2667352` feat: add dBASE work-area filter semantics
- `dc48b97` feat: add dBASE locate and continue semantics
- `feea97c` feat: add dBASE record navigation and status semantics
- `4881ab8` fix: restore complete work-area runtime source

Earlier implementation milestones:
- compiler/VM/DBF vertical slice
- NDX seek + variables
- work areas and aliases

## Important GitHub transport warning

Direct `git push` from the workbench currently fails because HTTPS credentials are not installed there.

GitHub writes have therefore been performed through the connected GitHub app/API.

Critical gotcha:
- Remote Desktop Commander `read_file` can cap around 1000 lines.
- `libs/runtime/src/xabl.cpp` is well beyond 1000 lines.
- An earlier upload was accidentally truncated because it was read in one call.
- That incident was repaired in `4881ab8`.

When mirroring large files through the GitHub connector:
- read them in explicit chunks;
- concatenate exactly;
- create blob/tree/commit;
- then fetch/reset the workbench to the remote commit;
- rebuild/test after the remote reconstruction.

Never assume one `read_file` call contains an entire >1000-line source file.

## Scheduled Tasks / “night shift” status

The user asked whether the old ChatGPT Scheduled Tasks “night shift” could again use plugins such as Remote Desktop Commander and GitHub.

A fresh one-shot task named **XABL Plugin Canary** was created for 9 Oct 2026. It was instructed to:
1. use Remote Desktop Commander on `git-workbench-brierfield` to create:
   `/tmp/xabl-scheduled-plugin-canary-20261009.txt`
2. only after that succeeded, create one GitHub issue in `brierfieldlabs/xabl` titled:
   `Scheduled task plugin canary`

The Scheduled Task did run (its last-run timestamp updated and the one-shot task completed/disabled), but verification showed:
- marker file **does not exist**
- GitHub search returned **zero matching issues**

Conclusion:
> ChatGPT Scheduled Tasks still failed to use Remote Desktop Commander and GitHub in the fresh canary.

Do not recreate the old night-shift lanes yet. The plugin capability problem remains unresolved.

The old Publishing Hub overnight tasks are mostly disabled/completed and should not simply be re-enabled for XABL.

## Separate server-side automation note

Do not confuse ChatGPT Scheduled Tasks with Brierfield server timers.

CT204 Documentation Compiler is healthy:
- nightly full timer enabled/active
- six-hour incremental timer enabled/active

That is unrelated to the broken ChatGPT Scheduled Tasks plugin boundary.

## Next implementation work

After naming work, continue practical runtime development. Good next steps:

1. Split the growing monolithic `libs/runtime/src/xabl.cpp` before it becomes unwieldy:
   - language/compiler/parser
   - runtime/value/vm
   - storage/dbf
   - storage/ndx
   The repository already has a structure intended for this.

2. Replace increasingly fragile line/string parsing with a real lexer/parser architecture. Parentheses/logical/filter work has pushed the current parser far enough that this is becoming important.

3. Add an explicit dBASE III PLUS compatibility-profile object/config instead of continuing to hard-code semantics.

4. Continue useful dBASE III compatibility slices, likely:
   - APPEND BLANK / APPEND semantics
   - USE/CLOSE semantics
   - PACK/ZAP only after safety model is explicit
   - SCAN / ENDSCAN
   - richer LOCATE/FOR/WHILE/NEXT/REST/ALL scope semantics
   - index-order navigation behavior
   - SET EXACT and string comparison behavior
   - dates and classic date functions
   - memory-variable/scoping details

5. Continue tests and docs with every slice.

Preferred near-term milestone remains:
> Run a small, nontrivial old-style dBASE mini-application under the new runtime using genuine DBF/NDX structures and multiple work areas.

Do not start Studio GUI work until the runtime can do a useful mini application.

## User interaction expectations

- Act as senior software engineer.
- Continue without asking unnecessary compatibility micro-questions.
- Keep the user updated during tool work.
- Batch design decisions into sensible commits instead of one commit per thought.
- Do not use the user's laptop for git.
- Workbench is `git-workbench-brierfield`.
- Keep GitHub and the workbench exact and clean.
- Surface LOC at meaningful checkpoints.
- Maximum compatibility is the default answer to legacy-semantic questions unless a real architectural fork exists.