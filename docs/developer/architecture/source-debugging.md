# Source debugging architecture (initial slice)

The compiler, VM, CLI and Text Studio all use the same `xabl_core` library.
Source inspection is available without creating or running a VM.

## Metadata contract

`xabl::Instruction::source` is a `SourceLocation` value. Source lines and
columns are one-based; zero means no source location. An instruction currently
maps to the first non-whitespace byte of its source statement. Lowered
expression instructions from the same statement share the same source
coordinates. The compiler attaches these coordinates after it has emitted all
instructions for a line, using a scoped annotation guard.

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

## Next integration requirements

Before wiring an IDE breakpoint key, provide a VM execution-control
interface that reports source location at statement-entry offsets; supports
step, continue and cancellation; and clearly distinguishes an inspected
program from one explicitly authorised to run. Headless tests must cover
loop revisits, error propagation, and deliberate table-write scripts in
throwaway fixture directories. No breakpoints or watches may modify data.

The initial mapping does not claim compatibility with the original dBASE
debugger, which varied across products and historical versions.
