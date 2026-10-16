# ADR: Compile structured dBASE control-flow with block patching

Status: Accepted, 9 October 2026

The line-oriented compiler emits VM jumps directly. Each open IF/DO WHILE
frame stores its unresolved conditional jump. IF frames also track whether
an ELSE branch has been used. ELSE emits one unconditional jump past the
ELSE body and patches the prior conditional jump to start the ELSE body.
ENDIF patches the final outstanding jump.

LOOP targets the nearest enclosing DO WHILE's condition address, not the
innermost IF, so it reevaluates the condition before a new iteration.
EXIT emits a placeholder unconditional jump, recorded on the nearest
enclosing DO WHILE frame. ENDDO patches all its EXIT targets to the
instruction after that loop. This preserves nested-loop structure.

Malformed ELSE/ENDIF/ENDDO pairings and EXIT/LOOP outside a loop remain
compile errors with source line numbers. This slice does not implement
ELSEIF, FOR/NEXT, SCAN/ENDSCAN or user-defined procedures.

The regression suite exercises nested branches, nested DO WHILE loops,
DBF iteration, skipping, early exit and invalid blocks.


## Follow-up: DO CASE (16 October 2026)

The original compiler block/jump implementation now supports the
dBASE III PLUS DO CASE, CASE expression, optional OTHERWISE and ENDCASE
statements. Conditions are evaluated in order, and only the first true
branch executes. With no matching CASE and no OTHERWISE, nothing runs.
Nested cases use the existing control-block stack and VM jump opcodes,
without a separate interpreter subsystem. Regression coverage lives
in the existing control-flow suite. Linked to issue #52.
