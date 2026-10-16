# XABL language support for VS Code

This is a small developer preview using VS Code's editor and the
existing C++ XABL compiler/runner. No second parser or runtime.

## Features

- Language registration and basic TextMate highlighting for .xabl and
  .prg files: classic commands, comments, strings and built-in functions.
- Command Palette: XABL: Check Current Source. Runs xabl --check on the
  active saved source, **without executing program statements**.
  Compiler errors appear in the Problems view using the built-in
  GCC-style problem matcher.
- Command Palette: XABL: Run Current Source. Executes the same xabl
  CLI. **Running may modify DBF databases.**

This extension does not change VS Code's shortcuts. The separate
Text Studio preserves its DOS/Borland-style keyboard layout.

## Local development and installation

From the repository root, build the CLI:

    cmake -S . -B build
    cmake --build build -j2

Open the extensions/vscode folder in VS Code and press F5 to launch
an Extension Development Host. Alternatively package as a VSIX with
the VS Code packaging tool (@vscode/vsce) and install that VSIX.
No npm dependencies or TypeScript compilation are needed to run this
extension; it relies on VS Code's built-in extension API.

Configure xabl.executablePath in VS Code Settings to the absolute
path of the just-built xabl executable, or place xabl on PATH of the
VS Code extension host. For remote sessions, that means the remote
environment. Then open a .prg/.xabl file and use Ctrl+Shift+P for
the two XABL commands. Unsaved buffers are saved before execution.

## Limits

- Lexical colouring does not guarantee dialect compatibility.
- No completion, symbol navigation or graphical debugger yet. The
  shared LSP and DAP follow-ups are Issues #47 and #48.
- The CLI currently implements dBASE III PLUS only. DBF writes are
  single-writer; do not use important/live data for testing.
- This optional extension does **not** settle the Qt GUI question.
  See Issue #45. Extension milestone is Issue #46.
