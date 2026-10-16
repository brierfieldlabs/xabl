# ADR: Command clauses are separators, not arbitrary substrings

Status: Accepted, 9 October 2026

The initial line-oriented compiler used uppercase string.find() for
command delimiters: STORE <expression> TO <name>, REPLACE <field> WITH
<expression> and USE <table> ALIAS <alias>.

That broke ordinary source such as:
    STORE "READY TO GO" TO message
because " TO " inside the character literal was mistaken for the
command separator. It also mishandled nested string expressions.

Introduce one private scanner, find_command_marker(), that recognises
top-level case-insensitive clauses while skipping:
- both quote delimiters, including doubled matching delimiters;
- nested parenthesised function arguments and grouped expressions.

Reuse it for ALIAS, TO and WITH clause detection. Keep the source
expression unchanged for the dedicated expression lexer. This is a
targeted correction, not a complete statement grammar or macro parser.

New regression cases cover quoted TO, doubled quote escapes, TO inside
UPPER() and a quoted WITH in a REPLACE value, including DBF persistence.
Macro substitution and same-line semicolon statement separation remain
future parser work.

## Follow-up: terminal semicolon continuation (16 October 2026)

A small logical-line reader now joins source lines when a semicolon is
the last non-comment, non-whitespace character **outside a string**.
It checks for inline && comments using the existing doubled-quote-aware
scanner. The first physical line/column remains the source location for
a compiled statement, including embedded expression bytecode and
compiler errors; subsequent continuation-only physical lines are not
new breakpoint targets.

Full-line comments and blank lines are ignored during continuation;
quoted semicolons do not continue a statement. Unterminated
continuation at end of file produces an explicit compiler error.
The original dBASE III PLUS NOTE/* comment-continuation variation
and same-line semicolon command separation remain unimplemented and
tracked by issue #52. In a follow-up, unquoted same-line separators
were made explicit compile errors until they can be mapped and stepped
separately in the debugger. This avoids silently creating malformed
variable names while keeping the compiler and VM simple. The changes
do not introduce a second parser.
