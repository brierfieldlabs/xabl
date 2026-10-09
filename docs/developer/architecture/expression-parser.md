# Expression lexer and parser

Implementation: `libs/runtime/src/expression.cpp` with a private
`expression.hpp` interface consumed by `compiler.cpp`. This is not a public
extension API. A token stream is parsed with precedence climbing (Pratt-style)
and directly emits stack VM instructions; the VM has not changed.

## Currently supported grammar

From weakest to strongest precedence:

1. `.OR.` or `OR`
2. `.AND.` or `AND`
3. Binary comparisons `>`, `<`, `=`, `==`, `!=`, `<>`, `>=`, `<=`
4. `+`, `-`
5. `*`, `/`
6. Unary `+`, `-`

Unary `.NOT.` and `NOT` consume a comparison expression but bind more
tightly than AND/OR, so `NOT balance < 100` means `NOT (balance < 100)`.
The `=` equality opcode respects the VM's SET EXACT state, while `==` emits
a distinct strict-equality opcode. Binary operators are left-associative. Parentheses explicitly override
precedence. Tokenisation respects quoted strings and the compact
`alias->field` syntax.

Literals: floating-point numeric values including exponents, quoted strings
with doubled-quote escaping, `.T.`/`.F.` and TRUE/FALSE.
Built-ins with zero arguments: EOF(), BOF(), FOUND(), RECNO(), RECCOUNT(),
DELETED(). Variable and field references become `LoadName` bytecode.

Invalid characters, unclosed quotes/parentheses, junk trailing tokens,
incomplete expressions, and unsupported calls now fail at compile time.
These errors are wrapped by the command compiler with line number; the
expression parser supplies the column. It does not silently interpret
unknown syntax as a variable name.

## Compatibility constraints and future work

The above is an executable subset, not a full dBASE III PLUS parser.
Operator precedence and remaining case/comparison semantics must ultimately be
validated against independently created historical reference cases in
each explicit compatibility profile. Present VM evaluation is eager:
logical short-circuit semantics, exact/substring comparison modes, date
arithmetic, additional functions, macro expansion and user functions are
not yet implemented.

Keep the lexer isolated from command-level grammar and retain the
`xabl_expressions` CTest suite. Add dialect-specific fixtures before
broadening supported syntax; don't let modern C++ semantics silently
override historical language behavior.
