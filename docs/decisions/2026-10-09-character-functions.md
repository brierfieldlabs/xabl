# ADR: Share byte-oriented legacy character functions

Status: Accepted, 9 October 2026

dBASE III PLUS character functions operate over the existing single-byte
DBF-compatible string representation. Introduce LEN, UPPER, LOWER, TRIM /
RTRIM (synonyms), and LTRIM as one-argument operations. The expression
compiler rejects missing/extra arguments and still rejects unsupported
function names.

The full VM and the separate filter bytecode evaluator must use the same
private apply_text_function helper. This prevents a filter from producing
different results to an expression with the same source.

For this vertical slice, functions require a character-typed operand.
LEN measures stored bytes, TRIM/RTRIM remove only ASCII trailing spaces,
LTRIM removes only ASCII leading spaces, and UPPER/LOWER convert ASCII
letters without altering high-bit bytes. The implementation must not claim
full legacy DOS codepage casing or Unicode grapheme awareness.

Documented reference: dBASE Language Handbook, TRIM()/RTRIM()
https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
