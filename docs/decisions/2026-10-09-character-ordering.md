# ADR: Character relational operators use byte comparisons

Status: Accepted, 9 October 2026

The initial runtime interpreted > and < only as numeric comparisons.
This incorrectly rejected perfectly ordinary dBASE III PLUS expressions
such as 'A' < 'a' or '950' > '750'. Strings are not parsed as numbers for
these operations merely because they contain digits.

Use a shared comparison helper for both the normal VM and its embedded
SET FILTER expression evaluator. When both operands are character values,
compare unsigned byte values lexicographically for > / <. Derived <= / >=
bytecodes retain their existing NOT greater / NOT less transformation.
When neither operand is a string, retain numeric comparison.

Mixed character/noncharacter operands now fail with a type mismatch for
both ordinary string equality and ordered comparisons, instead of silently
coercing to display strings. The lexer also accepts # as the traditional
inequality synonym for <> and !=, without changing the compiled equality/
logical-negation bytecode sequence.

This does not yet implement historical DOS codepage collation, fixed-width
DBF string padding rules, advanced NULL values, date comparisons or
version-specific ordering under SET EXACT. In particular, independently
run dBASE reference examples remain necessary before asserting complete
relational compatibility.

Reference: Programming with dBASE III PLUS, 'Comparing Strings',
Computer History Museum archive.
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
