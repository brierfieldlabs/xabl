# ADR: Source location mapping and read-only breakpoints

**Date:** 9 October 2026
**Status:** Accepted for initial debugging foundations
**Compatibility scope:** All compiled profiles; no new dBASE syntax

## Context

Text Studio can compile and navigate compiler errors, but the bytecode has
no source coordinates. A debugger must not infer a line number by rescanning
source after the compiler has lowered an expression or rewritten control flow.

## Decision

1. Each bytecode `Instruction` carries `SourceLocation {line, column}`.
   Coordinates are one-based and relate to the original source buffer, before
   trimming, stripping trailing comments, or normalising case.
2. All bytecode emitted while compiling a single logical source statement,
   including expression bytecode, gets that statement's coordinates. A local
   scope guard ensures mapping is applied on every successful `continue` path.
3. Generated `Halt` has zero/zero coordinates, meaning no source statement.
   Blank lines and comments emit no instructions. Control-only source lines
   such as `ENDIF` also have no executable offset.
4. An embedded `SET FILTER` expression retains the parent statement's
   coordinates, but its generated `Halt` is unsourced.
5. The `BreakpointMap` indexes the first instruction at each source line.
   Breakpoints must target an executable line and never trigger on subsequent
   instructions within one expression. The map is constructed from an immutable
   `Program` and must be rebuilt after recompilation.
6. This slice introduces no VM pausing, code execution, keyboard shortcuts or
   DBF access. Later work will add a VM debug hook and test it headlessly before
   allowing Text Studio to step or continue execution.

## Trade-offs and limits

- Columns identify the first non-whitespace source byte of a statement, not
  each operator or token. Unicode code-point-aware cursor columns are deferred.
- An `ELSE` has a jump instruction only on the true branch. `ENDIF` emits
  no instruction. A source breakpoint must therefore not promise a stop for
  every visually present control-flow keyword.
- The current language subset has one compiled statement per physical line.
  Future multi-line statements and macros will need source-span mapping.
- Offsets in the debugger map are only valid for the bytecode used to
  construct it. No persisted bytecode format or compatibility guarantee is
  implied.
- No implicit execution or mutation of customer data for source inspection.

## Verification

The `xabl_debugger` CTest covers CRLF and original indentation; ignored
comments and zero-location HALT; nested IF/ELSE/DO WHILE bytecode; first-opcode
breakpoint behaviour; safe enable/disable and rejection of invalid lines;
embedded filter origins; and hand-authored bytecode with no source metadata.
