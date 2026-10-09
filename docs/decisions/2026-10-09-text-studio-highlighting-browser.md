# ADR: Text Studio syntax colours and read-only directory browser

Status: Accepted, 9 October 2026

The ncurses Text Studio remains a frontend to the shared compiler and
runtime. Both syntax highlighting and file browsing are implemented in
terminal-independent modules. Neither parses or executes programs or
changes filesystem contents.

The highlighter returns byte-offset spans on one source line for dBASE
commands, supported functions, numbers, logical literals, quoted text
and comments. Doubled quotes are handled and comment markers within
strings do not accidentally become comments. The compiler remains the
only authority for valid syntax. Offsets reflect the present
ASCII-oriented text editor, not a full DOS/Unicode language driver.

The browser lists folders first and then .prg and .xabl files in
case-insensitive order. It has a parent entry and supports Up, Down,
Home, End, Page Up/Down, Enter and Backspace. Symlinks, device files,
unrecognised extensions and inaccessible entries are omitted. Names
containing terminal-control bytes are sanitised for display, without
modifying the underlying path. Browse lists up to 4096 entries.

F2 starts browsing from the current source's directory, or the current
working directory for an untitled file. Esc returns without changes;
Ctrl+O retains the path-by-name option. Selecting a file invokes the
existing load operation and unsaved-change confirmation. The browser
itself is read-only and cannot delete, move or execute files.

Unit suites test both pure components without a terminal; the PTY test
checks F2 through to loading a real throwaway PRG, followed by
compilation, execution, editing, saving and exit.

Full CP437/CP850 rendering, multi-document workspaces, breakpoints,
syntax-aware completion and a true project manager remain future work.
DOS-inspired styling does not mean this program is an MS-DOS binary.
