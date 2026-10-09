# ADR: Add dBASE III PLUS VAL and STR character/numeric conversions

Status: Accepted, 9 October 2026

Both conversions are required for traditional dBASE III PLUS source.
VAL(<expC>) reads a numeric prefix after leading ASCII blanks, returning
zero when the string does not begin with a number. It accepts one character
expression. STR(<expN>[,width[,decimals]]) right-aligns a numeric value
inside its requested output width, with explicit decimal places. The
defaults are ten characters and zero decimals.

A dedicated VAL opcode and one-to-three-argument STR opcode share
runtime and embedded SET FILTER evaluation helpers. Mixed implicit
string/numeric arithmetic remains disallowed until a verified dialect
coercion profile is implemented. STR emits asterisks when a valid field
width cannot hold a value, including its sign.

As documented in the original dBASE III PLUS programming guide,
a requested STR decimal precision cannot exceed the available width
after reserving space for one whole digit and the decimal point. Invalid
width/decimal specifications are rejected instead of returning asterisks.

Implementation safety: STR width must be between 1 and 1 MiB, decimal
precision between 0 and 18, numeric arguments must be finite, and
VAL refuses a non-finite conversion result. Counts are truncated toward
zero. These constraints are XABL safety choices, not a declaration of
the exact original dBASE range. The decimal rounding and exponent-prefix
behaviour still require comparison with an original interpreter.

Regression coverage includes VAL leading blanks and numeric prefixes,
invalid text returning zero, STR right alignment, asterisk overflow,
zero and negative values, nested functions, invalid argument counts
and types, width/decimal errors, and calls inside SET FILTER.

Historical references:
- Programming With dBASE III PLUS, pages P5-17 to P5-19:
  https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
- dBASE Language Handbook VAL:
  https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
