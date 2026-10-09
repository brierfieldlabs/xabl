# ADR: Add the classic $ substring-comparison operator

Status: Accepted, 9 October 2026

dBASE III PLUS character expressions support the binary $ operator:
a $ b is true when the bytes of a occur as a substring of b.
It is part of the comparison precedence family, not arithmetic, and
returns a logical result. The source lexer treats the symbol inside
quoted strings as literal text, as it already does for other operators.

The expression compiler now emits a dedicated Contains bytecode
instruction. Main VM evaluation and SET FILTER execution use the same
contained_in helper in comparison.hpp. Both operands must be character
values in the current strict III PLUS subset.

The comparison is case-sensitive, byte-oriented, independent of SET
EXACT, and treats an empty needle as not contained anywhere, including
in an empty haystack. These semantics are distinct from both dBASE's
SET EXACT prefix equality and AT(), which returns a one-based position
rather than a logical result.

Regression coverage includes full/prefix/interior matches, nonmatches,
case differences, empty strings, embedded NUL bytes, expression
precedence, mixed-type errors and filters against padded DBF C fields.

DOS codepage collation, macros and dialect-specific coercion remain
outside this slice. Other dBASE-family profiles remain fail-closed.

References:
- dBASE III PLUS relationship operator descriptions:
  https://it.scribd.com/document/747449572/DBASE-III-PLUS-manual
- dBASE company comparison operator reference and $ empty behaviour:
  https://www.dbase.com/help/11_1/Operators_and_Symbols/IDH_OPS_COMPARISON.htm
