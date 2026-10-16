# Source debugging architecture (initial slice)

The compiler, VM, CLI and Text Studio all use the same `xabl_core` library.
Source inspection is available without creating or running a VM.

## Metadata contract

`xabl::Instruction::source` is a `SourceLocation` value. Source lines and
columns are one-based; zero means no source location. An instruction currently
maps to the first non-whitespace byte of its source statement. Lowered
expression instructions from the same statement share the same source
coordinates. The compiler attaches these coordinates after it has emitted all
instructions for a logical statement, using a scoped annotation guard.
A semicolon-continued statement is attributed to its first physical
source line; continuation-only lines cannot accept a breakpoint.

Control-flow `target` offsets remain unchanged: source information is
additional metadata, not a replacement for branches. Synthetic HALT does not
have source coordinates. `ENDIF` and source comments have no bytecode entry.

## Resolving a breakpoint

```cpp
#include <xabl/runtime/debugger.hpp>

xabl::Program program = xabl::Compiler{}.compile(source);
xabl::BreakpointMap breakpoints(program);
const auto available = breakpoints.executable_lines();
if (breakpoints.instruction_at(12)) {
    breakpoints.enable(12);
}
const bool stop_here = breakpoints.should_pause(instruction_offset);
```

`BreakpointMap` owns its lookup tables, **not** the source `Program`.
It cannot execute code, pause a VM, inspect variables, or open database
tables. `enable` rejects blank, comment-only, missing, and non-executable
control-marker lines; `disable` is idempotent. Resolved offsets identify
the first emitted instruction at a source line, preventing a repeated stop
for each operand of an expression. Recreate the map on every compile.

## Headless VM execution control (16 October 2026)

The VM is resumable without coupling execution to a terminal or Qt event loop.
Typical API usage:

    xabl::Program program = xabl::Compiler{}.compile(source);
    xabl::BreakpointMap breakpoints(program);
    breakpoints.enable(12);
    std::ostringstream output;
    xabl::Vm vm(output);
    vm.begin(program, working_directory);  // does not execute anything
    xabl::DebugStop reason = vm.resume(breakpoints);
    if (reason == xabl::DebugStop::Breakpoint) {
        const auto location = vm.debug_location(); // next statement
        reason = vm.step();  // execute one source statement
    }
    vm.cancel_debugging();  // discard remaining execution, no rollback

begin() validates the profile, resets the operand stack and instruction offset,
and records the chosen directory. It does **not** open files or execute
commands. resume() stops before enabled source breakpoints and bypasses a
just-hit breakpoint once, including when a loop revisits the same source line.
step() executes one *source statement* rather than one VM opcode. Expression
evaluation (including embedded filter expressions) is part of its statement.
debug_location() points at the next instruction, not the one just executed.

The VM borrows the Program for an active session: keep it alive and unchanged
until completion/cancellation. A BreakpointMap must be made for that exact
compiled program. A breakpoint map for a different program is rejected before
execution. A runtime instruction exception invalidates the debug session.
Completed and cancelled sessions cannot be resumed until a new begin() call.

Ordinary Vm::run() still runs synchronously to completion; it does not use
the breakpoint index or pay per-instruction debugger lookup costs. The
debugging API is headless and does not alter dBASE syntax or opcode behaviour.

**Important:** Explicit resume() and step() may change DBF files if the
program contains writable statements. cancel_debugging() only prevents
*future* commands and cannot roll back statements already executed. This is
not a read-only sandbox or transactional debugger. Use disposable DBF copies
for debugging writable source. The debugger adds no multi-user record locking.

## Further integration work

Text Studio and the future Qt frontend still need keyboard actions, a
visible paused-line indicator, variable inspection and a deliberate run
confirmation before any writable script executes. There is no frontend
debugger integration yet. Source mapping does not claim equivalence to the
historical dBASE debugger and does not implement macro/source-span tracking.


## Text Studio keyboard integration (16 October 2026)

The ncurses frontend directly uses the existing Vm and BreakpointMap;
no separate debugger service or framework is introduced. Keyboard
bindings now follow Borland/Free Pascal conventions:

- Ctrl+F8: breakpoint. Ctrl+F9: run/continue. Ctrl+F2: stop/reset.
- Ctrl+F7: variable snapshot; not a watch-expression editor.
- F7 and F8: the same statement step until procedure calls need
  distinct trace-into and step-over behaviour.
- F4: run to cursor via a temporary breakpoint, without modifying
  existing user breakpoints.
- F9 and Alt+F9: compile the current source file only.
- Alt+F5: output screen. F10: command-menu fallback.

Common xterm/Konsole modified F-key escape sequences are registered
with ncurses define_key, but terminal configurations vary. The
existing PTY test covers keys and menu access. Editor modifications,
source switching and saving are blocked while a debug session is
paused; editing after reset clears line-based breakpoints.

The frontend is synchronous, so a long-running or looping statement
cannot be interrupted while executing. DBF writes are not transactional.
