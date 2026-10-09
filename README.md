# XABL

**XABL** (eXtensible Application Base Language) is a modern continuation of the dBASE/xBase tradition, developed by Brierfield Labs.

XABL preserves classic dBASE syntax, semantics, workflow, data formats, and compatibility wherever practical, while adding modern tooling, cross-platform application targets, debugging, packaging, extensions, and contemporary front-end capabilities.

## Status

Active pre-release compiler/runtime development. A tested dBASE III PLUS
subset supports DBF/NDX operations, expressions, control flow and typed
date fields. This is **not yet suitable for concurrent editing of live DBFs**;
proper record locking, index maintenance and DBT support remain pending.

There are now two executables:

- `xabl`: compile/run a legacy `.prg` source from the shell.
- `xabl-tui`: DOS-inspired text IDE with a full-screen blue editor, keyboard
  shortcuts, source-file editing, compile-only check and run/output screen.
  Built automatically where the optional ncurses development library is found.

To build on Linux:

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/xabl-tui tests/fixtures/legacy-customer-review.prg
```

The TUI is terminal-hosted, not an MS-DOS executable. See
`docs/user/text-studio.md` for shortcuts, limitations and precautions.

## Core principle

> Preserve the dBASE way of programming. Modernise the environment around it.

## Licence

GPL-3.0-or-later.

## Documentation

See `docs/developer/architecture/xabl-design-decisions.md` for the current design record.
