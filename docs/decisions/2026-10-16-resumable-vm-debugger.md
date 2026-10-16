# ADR: Resumable source-statement VM debugging

**Date:** 16 October 2026
**Status:** Accepted for headless integration
**Scope:** Execution-control API, no new language syntax

## Decision

- Preserve the synchronous Vm::run() API and its existing runtime behaviour.
- Provide explicit begin(), resume(), step(), debug_location(), and
  cancel_debugging() methods in the shared C++ runtime.
- Borrow immutable compiled program bytecode for a debug session and reject a
  BreakpointMap created from a different program object.
- Stop *before* executing the first instruction of a breakpoint statement.
  A resumed breakpoint is skipped exactly once; revisiting it through a loop
  can stop again. Source stepping stops at the next executable statement
  boundary, never in the middle of expression bytecode.
- Preserve the VM operand stack and work areas across pauses. Do not rerun
  instructions after resuming.
- Terminate the debug session after HALT, cancellation or a VM instruction
  exception; do not auto-restart. Cancellation is not transactional rollback.

## Safety and compatibility

Compiling, mapping breakpoints and begin() are non-executing operations.
Only explicit resume() or step() permits bytecode to execute. A statement
can write an on-disk DBF and effects are not reversible via cancellation.
Tests use disposable copies of a generated DBF fixture and verify there was
no write before reaching the APPEND BLANK command. The unchanged legacy
run() path uses no source lookup per instruction. No NDX/DBF storage
semantics, legacy parser rules, or compatibility dialects were changed.

## Deferred

No curses/Qt debugger buttons, watches, conditional breakpoints, database
transactions, cross-program source spans or macro expansion. No claim of
complete historical dBASE debugger compatibility.

## Verification

The existing xabl_debugger suite now covers pause-before-execution,
continue/resume, source-level stepping across branches and loop back-edges,
repeat breakpoints, map ownership, exception invalidation, explicit
cancellation and writes to temporary DBF copies.
