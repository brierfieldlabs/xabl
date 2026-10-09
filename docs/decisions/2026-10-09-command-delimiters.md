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
Macro substitution and semicolon continuation remain future parser work.
