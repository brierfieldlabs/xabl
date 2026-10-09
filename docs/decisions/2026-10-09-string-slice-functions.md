# ADR: Add multi-argument legacy character extraction

Status: Accepted, 9 October 2026

The dBASE III PLUS character-function subset now includes LEFT, RIGHT and
SUBSTR. The expression lexer recognises comma as an argument separator;
the parser compiles two expressions for LEFT/RIGHT, and two or three for
SUBSTR (whose optional third argument is a maximum length). The source
arguments can themselves contain parenthesised expressions or nested calls.

The compiler emits distinct stack opcodes; SUBSTR records the two/three
argument count in the instruction target field. The main VM and the
separate embedded-filter evaluator consume these operands in the same order
and call one private byte-oriented helper.

Positions are one-based for SUBSTR; for now nonpositive starting positions
produce an explicit runtime error rather than an unverified historical result.
Numeric positions/counts must be finite numbers, truncated towards zero.
Nonpositive LEFT/RIGHT lengths return empty strings; excessive lengths
stop at the available string length. These guards avoid out-of-range casts,
unbounded allocations and accidental conversions from nonnumeric values.

These are current *tested subset* semantics. Before certifying particular
historical versions, cross-check edge cases, error handling, code pages and
legacy maximum character lengths against original product binaries.

Reference: dBASE III PLUS Programming guide at Computer History Museum,
and current dBASE RIGHT() reference:
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
https://www.dbase.com/help/String_Objects/IDH_RIGHT_FUNC.htm
