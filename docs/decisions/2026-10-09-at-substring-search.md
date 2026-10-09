# ADR: Add dBASE III PLUS AT() substring search

Status: Accepted, 9 October 2026

AT(search, target) returns the one-based position of the first substring
match, or numeric zero if the search text is empty or not found. The
current dBASE III PLUS profile accepts precisely two character arguments,
and rejects the later optional occurrence argument rather than quietly
treating it as a valid III PLUS construct.

The lexer/compiler use the existing two-expression function-argument
parser, emitting a dedicated CallAt instruction. The normal VM and
separately evaluated DBF filter bytecode call the same apply_at_function
helper with the operands in source order. Comparison is case-sensitive
and byte-oriented; DOS codepage differences remain unimplemented.

The expression suite tests string search, nesting, malformed argument
counts and type errors. The DBF suite tests AT inside filters and correct
visibility after positive and negative matches.

Reference: Programming With dBASE III PLUS, Computer History Museum
archive, chapter 5 'Substring Position'.
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
