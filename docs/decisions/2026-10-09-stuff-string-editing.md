# ADR: Implement bounded four-argument STUFF string editing

Status: Accepted, 9 October 2026

STUFF(target, start, quantity, replacement) is a classic dBASE III PLUS
character operation with four required expression arguments. It removes
characters from the target beginning at a one-based position and inserts
a replacement string. Both the full VM and the DBF SET FILTER evaluator
must call exactly the same byte-oriented helper.

The expression compiler accepts four nested expressions and emits a
dedicated CallStuff opcode. The helper applies these currently tested
semantics:
- Start positions <= 1 are treated as 1.
- Start positions beyond the target length append the replacement.
- Zero/negative deletion quantities perform insertion only.
- A deletion quantity longer than the available suffix removes that suffix.
- Empty targets return the replacement.
- An empty replacement performs deletion without insertion.
- Numeric counts and positions must be finite and are truncated towards zero.

The resulting string is limited to 1 MiB, using checked arithmetic
before allocation. This is a XABL runtime safety policy rather than a
claim about the original DOS interpreter's string-size limit.

Tests check replacement, insertion, deletion, negative/oversized counts,
one-based starts, nested CHR output and embedded NUL bytes, argument
errors, bounded output and use in active record filters.

Historical versions and codepages may differ in subtle boundary cases;
retain separate reference fixtures rather than claiming full historical
interpreter parity on the basis of later dBASE documentation alone.

References:
- dBASE III PLUS teaching material for STUFF:
  https://www.studocu.com/en-us/document/fossil-ridge-high-school-colorado/portuguese/dbase-iii-plus-tutorial/59179356
- dBASE Language Reference 2.6, STUFF boundary behaviour:
  https://www.dbase.com/downloads/dBLLanguageReference2.6.pdf
