# XABL Text Studio: DOS-style terminal IDE

**Status:** first executable developer-preview slice, 9 October 2026.

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
It does not require a mouse. Ctrl+S and Ctrl+Q disable Unix software flow
control while inside the editor, restored on normal exit by curses.

## Keys

| Key | Operation |
| --- | --- |
| F1 | Toggle keyboard help |
| F2 | Open an existing source file |
| F3 | New/empty file |
| F4 or Ctrl+S | Save file; prompt for path if untitled |
| F5 | Compile and **run** the open source |
| F6 | Toggle output screen and editor |
| F7 or Ctrl+F | Search forward, wrapping at end of file |
| F8 | Find next occurrence |
| F9 | Compile only, **without running program statements** |
| F10 or Ctrl+Q | Exit, with unsaved-changes confirmation |
| Ctrl+Z | Undo an edit (up to 40 recent steps) |
| Arrows / Home / End | Move cursor |
| Page Up / Page Down | Move by twelve lines |
| Backspace / Delete | Remove character or join lines |
| Enter / Tab | New line / insert four spaces |
| Esc | Return from help or output to editor |

The editor prompts before discarding unsaved text. Saves first write a
sibling staging file, then replace the target on successful completion.
It refuses to replace symbolic links; it does not promise fsync-based
crash durability or cross-device transaction safety.

## Runtime and data safety

**F5 actually executes the program** and may modify DBF files through
APPEND/REPLACE/DELETE/RECALL; Text Studio asks before executing. F9 checks
syntax only. The working directory of a saved program is the parent of
the opened file, as in the command-line runner. An untitled program runs
relative to the process's current directory.

The DBF runtime is still single-writer only. Do not test against live,
shared, or valuable production DBF/NDX files. The GUI frontend should
eventually share this execution model and locking policy.

## Known preview limitations

- The editor currently processes printable ASCII keystrokes, and supports
  a source file up to 1 MiB. Full UTF-8 grapheme navigation, DOS CP437/
  CP850 rendering and Unicode-friendly printing are later work.
- Files containing binary NUL are rejected. CRLF is preserved for
  consistently CRLF-formatted sources; mixed EOL sequences normalise to
  the first detected line-ending style when saved after editing.
- A simple line editor and undo are present. There is no syntax
  highlighting, breakpoint debugging, mouse support, project browser,
  autocomplete, multi-document tabs, clipboard integration or integrated
  database-browser pane yet.
- ncurses is optional. On systems without a compatible curses library,
  `xabl` and its testable text-buffer core still build; terminal UI is
  omitted. A Windows console/PDCurses backend is future work.
- Program output is captured into memory and displayed on a separate
  screen; interactive dBASE keyboard input is not yet implemented.

## Tests

`xabl_text_editor` covers file IO (including CRLF), cursors, search,
undo, insert/delete and safe-save failure. `xabl_tui_pty` launches the
actual curses program under a pseudo-terminal and verifies F9 compilation,
F5 execution, F6 navigation, Ctrl+S saving and Ctrl+Q exit using only a
throwaway arithmetic script. The test suite never opens user databases.
