# ADR: Line navigation and grounded compiler-error jumps

Status: Accepted, 9 October 2026

Text Studio adds a one-based source-line navigation primitive to the
terminal-independent editor model. It returns false for zero or a line
beyond the loaded source, without altering the cursor or document.
No source bytes or undo history are changed by navigation.

Ctrl+G prompts for a line number. Input must be a complete unsigned
decimal integer accepted by from_chars and inside the current source
length; bogus strings, signed numbers, overflow and zero are rejected.

The existing compiler prefixes its source diagnostics with 'line N:'.
The editor exposes a strict parser for that prefix rather than
extracting arbitrary numbers from exception messages. F9 compiles
without executing. If compilation itself fails and yields a valid
line number, F8 on the output screen navigates to that source line.
Runtime errors are not mislabelled as compiler diagnostics.

The prompt, document model and compiled-output navigation are tested
headlessly and through a real curses pseudo-terminal, including a
two-line invalid PRG. These behaviours are part of the DOS-style
terminal frontend, not modifications to bytecode or interpreter
semantics. Full diagnostic ranges and multi-file source maps remain
future development.
