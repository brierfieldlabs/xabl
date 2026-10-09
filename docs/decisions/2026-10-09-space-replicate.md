# ADR: Guard legacy SPACE and REPLICATE allocations

Status: Accepted, 9 October 2026

dBASE III PLUS includes SPACE(number) to create blank text and
REPLICATE(character-expression, numeric-expression) to repeat text.
These are valid in memory-variable expressions and work-area filters.

The compiler emits one-argument SPACE and two-argument REPLICATE opcodes.
Both VM execution paths use the same helper and require a finite numeric
count; REPLICATE additionally requires a character-string input.
Counts are truncated toward zero and non-positive counts return empty
text in the current executable subset.

For safety, each function enforces a 1 MiB **implementation allocation
ceiling**. This is not represented as a historical dBASE string limit;
original products had different constraints and even documented failure
cases with very large REPLICATE calls. The ceiling avoids overflow and
runaway allocations before reserving the resulting memory. The helper
supports repeating multi-character strings, not just one character.

Regression coverage includes nested functions, filter expressions,
zero/negative/fractional counts, a 1 MiB successful result, impossible
counts, type errors and mistaken argument counts.

References: dBASE Language Handbook, SPACE and REPLICATE (dBASE III PLUS).
https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
