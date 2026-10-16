# XABL Text Studio: DOS-style terminal IDE

**Status:** executable developer preview with source debugging, 16 October 2026.

Text Studio provides an optional 80-column-inspired, keyboard-first front
end to the **same** C++ compiler, bytecode and VM used by the command-line
XABL runner. It is not a separate language implementation and not yet a
native DOS/MS-DOS binary. The temporary XABL executable name remains until
the final brand is cleared.

## Launching

On Debian or another Linux system with the ncurses development library:

```sh
cmake -S . -B build
cmake --build build -j2
./build/xabl-tui                 # untitled buffer
./build/xabl-tui my-program.prg  # edit existing source
```

Aim for an 80x25 terminal or larger (minimum supported 45x12). The editor
uses a traditional white-on-blue workspace, cyan menu/status bars,
gutter line numbers, source position indicators and function-key controls.
It does not require a mouse. Ctrl+S/Q bypass terminal flow control and
Ctrl+Z bypasses terminal suspend on supported Unix systems, restoring
terminal behaviour after normal editor exit.

## Keyboard layout

Text Studio follows the Borland/Turbo Pascal IDE conventions where the
present XABL features allow it. The
[Free Pascal IDE keyboard reference](https://www.freepascal.org/daily/doc/user/userse32.html)
preserves many historical DOS-era Borland key combinations.

| Key | Operation |
| --- | --- |
| F1 | Help |
| F2 / Ctrl+S | Save |
| F3 | Browse/open a PRG or XABL file |
| Ctrl+O | Open by path |
| Ctrl+N | New/empty file |
| F4 | Run to cursor or an earlier breakpoint |
| F5 | Zoom/unzoom editor |
| F6 | Switch editor/output |
| F7 / F8 | Step one source statement |
| F9 / Alt+F9 | Build/compile only, without executing |
| F10 | Command menu (fallback for modified F-keys) |
| Ctrl+F8 | Toggle an executable-line breakpoint |
| Ctrl+F9 | Run or continue |
| Ctrl+F2 | Stop/reset debugging |
| Ctrl+F7 | Inspect current variables (read-only) |
| Alt+F5 | Program output |
| Alt+X / Ctrl+Q | Quit, confirming unsaved changes |
| Ctrl+F / Ctrl+G | Find / Go to source line |
| Ctrl+E / F12 | Jump to reported compiler error |
| Ctrl+Z | Undo (up to 40 steps) |
| Ctrl+D / Ctrl+X | Move right / down (WordStar-style) |
| Arrows / Home / End | Move cursor |
| Page Up / Page Down | Move by twelve lines |
| Backspace / Delete | Delete or join lines |
| Enter / Tab | New line / four spaces |
| Esc | Return to editor or close menu/browser |

F7 and F8 both perform statement-level stepping: separate
procedure-level step-into and step-over are not yet supported. F9 and
Alt+F9 both compile the current source file; a multi-file make/project
system does not exist.

The common modified-key sequences are supported for xterm/Konsole
terminals, although terminal configurations differ. F10 provides an
always available letter-driven command menu with equivalent actions.

An asterisk marks a breakpoint in the line gutter and a greater-than
symbol marks the next paused statement. A multiline statement continued
with a terminal semicolon has its executable breakpoint on the **first**
physical line, not its continuation-only lines. F4 uses a temporary breakpoint
at the cursor while honouring existing breakpoints. Ctrl+F9 executes
normally without breakpoints, or starts/continues debugging when
breakpoints are enabled. Ctrl+F7 displays sorted, read-only variables
while paused. It does not implement watch expressions.

Ctrl+G accepts a one-based source line. Ctrl+E or F12 jumps to the last
numbered compiler error. F3 browses the source directory (or current
directory for untitled sources), skipping symlinks. Ctrl+O opens a
specific path. Editing changes clear line-based breakpoints.

The editor prompts before discarding unsaved text. Saving uses a sibling
staging file and refuses symlink replacements, but does not promise
crash-proof or cross-device transactional writes.

## Runtime and data safety

Ctrl+F9, F4, F7 and F8 may execute database-writing statements such as
APPEND, REPLACE, DELETE and RECALL. They require a confirmation when
starting execution. F9 and Alt+F9 compile only; setting a breakpoint
does not execute code. Stopping the debugger cannot undo DBF writes.

Saving a source sets its parent directory as the working directory
for execution. Untitled sources run relative to the current process
directory. While paused, source editing, opening, compiling and saving
are disabled until the debugger ends or is reset, preventing stale
compiled code. Do not use valuable or shared DBF files while debugging;
the DBF runtime is currently single-writer.

## Known preview limitations

- The editor currently processes printable ASCII keystrokes, and supports
  a source file up to 1 MiB. Full UTF-8 grapheme navigation, DOS CP437/
  CP850 rendering and Unicode-friendly printing are later work.
- Files containing binary NUL are rejected. CRLF is preserved for
  consistently CRLF-formatted sources; mixed EOL sequences normalise to
  the first detected line-ending style when saved after editing.
- Basic syntax colouring and a read-only directory browser are present.
  Neither is a project/workspace manager. Conditional breakpoints, watch
  expressions, mouse support, autocomplete, multi-document tabs,
  clipboard integration and
  an integrated database-browser pane remain future work.
- ncurses is optional. On systems without a compatible curses library,
  `xabl` and its testable text-buffer core still build; terminal UI is
  omitted. A Windows console/PDCurses backend is future work.
- Program output is captured into memory and displayed on a separate
  screen; interactive dBASE keyboard input is not yet implemented.

## Tests

The existing xabl_text_editor, xabl_debugger and xabl_tui_pty suites
cover the editor model, VM and actual terminal interaction. The PTY
test uses disposable arithmetic source and checks Borland-style keys,
debugger stepping, menu access and read-only variable inspection.
No additional keyboard test suite was introduced.
