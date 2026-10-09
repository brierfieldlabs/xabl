# Runtime subsystem boundaries

Status: implemented source split, 9 October 2026. Project name remains XABL in
source code until a successor brand is approved.

The public API remains in `libs/runtime/include/xabl/runtime/xabl.hpp`. The
following source files implement it, without changing existing semantics:

| Source | Responsibility |
| --- | --- |
| `compiler.cpp` | Line-oriented command compilation |
| `expression.cpp` | Expression lexer, precedence parser and VM instruction emission |
| `value.cpp` | Tagged language values and legacy conversions |
| `vm.cpp` | Stack execution, work areas, navigation, visibility and state |
| `dbf.cpp` | dBASE III-compatible physical DBF records and record persistence |
| `ndx.cpp` | Read-only dBASE III-compatible NDX B-tree lookup |
| `internal.hpp` | Private text/case helpers needed by multiple components |

`internal.hpp` is not installed and is not a supported extension API. It
contains only internal inline helpers; exported interface changes belong in the
public header with compatibility tests. Do not introduce dependencies from DBF
or NDX storage back to the compiler/VM.

The original split was deliberately mechanical. The subsequent expression
lexer/parser is documented separately; it does **not** establish a complete command parser, dBASE compatibility-profile framework, production-grade write
transactions or NDX write support. Those remain separate tracked work.

Build via CMake and execute CTest. The fixture generator creates customer DBF
and NDX files before the smoke tests. Avoid silently changing:
- work-area selection/aliases and independent FOUND state;
- global SET DELETED visibility;
- direct physical record navigation, including invisible records;
- per-work-area filters and normal filtered navigation;
- DBF byte-preserving record persistence;
- read-only NDX index lookup.

When adding lexical/parser structure, first preserve existing compatibility
fixtures and add regression examples for operator precedence, quoting and
nested expressions. Keep legacy dialect differences explicit.

The DBF physical append/close/navigation slice is documented in
`docs/developer/compatibility/dbase-iii-plus.md`.
