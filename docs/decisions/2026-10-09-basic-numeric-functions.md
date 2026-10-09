# ADR: Strict numeric ABS, INT, MIN and MAX

Status: Accepted, 9 October 2026

Add the foundational dBASE III PLUS numeric functions:
- ABS(number), an absolute value.
- INT(number), truncating the fractional part toward zero. For negative
  values this differs from floor; INT(-3.9) is -3.
- MIN(first,second) and MAX(first,second), selecting the smaller or larger
  numeric operand.

The expression compiler emits dedicated one- or two-argument opcodes.
The normal VM and SET FILTER interpreter call the same private helpers
in numeric_functions.hpp, with strict numeric storage and finite input
validation. Character expressions that look numeric must use VAL()
explicitly before these functions.

The returned numeric zero from INT is normalised to +0.0 for consistent
display. Other numeric operations remain double-based; historical floating
point precision, codepage and version-specific differences require
original-product verification.

ROUND() is deliberately excluded from this batch. The dBASE Language
Handbook records III PLUS rounding defects (including negative halves and
some 0.355 cases), so adding a modern ROUND() without a version-labelled
compatibility test would misrepresent the historical dialect.

Regression cases exercise positive, negative, nested, malformed, and
type-mismatched operations, plus filtering against DBF numeric fields.

References:
- dBASE III PLUS numeric function overview:
  https://pic.hallikainen.org/techref/language/dbase/commands.htm
- dBASE Language Handbook, ROUND() historical compatibility warnings:
  https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
