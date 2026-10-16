# XABL

**XABL** (eXtensible Application Base Language) is a modern continuation of the dBASE/xBase tradition, developed by Brierfield Labs.

XABL preserves classic dBASE syntax, semantics, workflow, data formats, and compatibility wherever practical, while adding modern tooling, cross-platform application targets, debugging, packaging, extensions, and contemporary front-end capabilities.

## Status

Active pre-release compiler/runtime development. A tested dBASE III PLUS
subset supports DBF/NDX operations, expressions, control flow and typed
date fields. This is **not yet suitable for concurrent editing of live DBFs**;
proper record locking, index maintenance and DBT support remain pending.

There are now two executables:

- `xabl`: compile/run (or compile-check without execution) a legacy `.prg` source from the shell.
- `xabl-tui`: DOS-inspired text IDE with a full-screen blue editor, keyboard
  shortcuts, syntax colouring, F3 file browser, source-file editing,
  compile-only check, source debugging and run/output screens.
  Built automatically where the optional ncurses development library is found.

To build on Linux:

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/xabl-tui tests/fixtures/legacy-customer-review.prg
```

An optional VS Code extension is available under extensions/vscode;
it uses the same C++ xabl compiler for Check/Run tasks and provides syntax
highlighting. Use xabl --check file.prg for compile-only diagnostics.
The choice of a future full Qt GUI remains open in issue #45.

The TUI is terminal-hosted, not an MS-DOS executable. See
`docs/user/text-studio.md` for shortcuts, limitations and precautions.

## Core principle

> Preserve the dBASE way of programming. Modernise the environment around it.

## Issue tracker and planned work

[GitHub Issues](https://github.com/brierfieldlabs/xabl/issues) is the
source of truth for actionable XABL development ideas, TODOs, requests,
known limitations and decisions requiring follow-up. The
[roadmap index](https://github.com/brierfieldlabs/xabl/issues/43)
groups older design-record proposals and links the individual work items.
Before starting or parking a task, search existing issues and update the
appropriate one rather than creating duplicates. Link pull requests to
their relevant issues and close them when the work is complete.

**Current editor strategy discussion:** issue
[#45](https://github.com/brierfieldlabs/xabl/issues/45) compares a full
Qt GUI against using VS Code/Zed integrations. This is not yet a final
product decision. Text Studio remains a first-class Borland-inspired
terminal IDE.

## Licence

GPL-3.0-or-later.

## Documentation

See `docs/developer/architecture/xabl-design-decisions.md` for the current design record.
