# XABL implemented command and function reference

**Status:** pre-release, verified against the current C++ command and expression
dispatch, not a promise of complete dBASE III PLUS compatibility. The
`dbase-iii-plus` profile is the only implemented interpreter profile.
Keywords and built-in function names are case-insensitive. Unless noted,
examples use the current single-file `.prg` compiler.

Start with the [quick start](quick-start.md). This is a reference to **what
the code currently accepts**, not the broader proposed language in
`docs/developer/architecture/xabl-design-decisions.md`.

## Running a PRG

| CLI form | Effect |
| --- | --- |
| `xabl --help` | Print CLI options |
| `xabl --check file.prg` | Compile only; do **not** execute DBF-changing statements |
| `xabl file.prg` | Compile then run, using the file directory as the working directory |
| `xabl --dialect dbase-iii-plus file.prg` | Select the one implemented profile |

Successful `--check` exits 0; compiler errors exit nonzero with
`file:line:column: error` locations. For Text Studio keys and editor
behaviour, see [Text Studio](text-studio.md).

## Supported program statements

The following are *implemented subsets*, not every historical option of
each command.

| Syntax | Current behaviour |
| --- | --- |
| `* comment`, `NOTE comment`, `&& comment` | Full-line comments; quote-aware inline `&&` supported |
| `? expression`, `?expression`, `?` | Print expression with a newline, or an empty line |
| `STORE expression TO name` | Store a value in a named variable |
| `name = expression` | Assign to a named variable |
| `IF expression` / `ELSE` / `ENDIF` | Conditional, optional ELSE, nested blocks |
| `DO CASE` / `CASE expression` / `OTHERWISE` / `ENDCASE` | First true CASE wins; optional OTHERWISE |
| `DO WHILE expression` / `ENDDO` | Re-evaluate condition and loop |
| `LOOP`, `EXIT` | Continue/leave innermost enclosing DO WHILE |
| `USE table` | Open a DBF by table name in current work area |
| `USE table ALIAS alias` | Open DBF with a work-area alias |
| `USE` | Close active work-area table |
| `SELECT n`, `SELECT letter`, `SELECT alias` | Select among dBASE III PLUS's ten work areas (1���10, A���J) |
| `SET INDEX TO index` / `SET INDEX TO` | Open/close a **read-only** NDX in the current area |
| `SET FILTER TO expression` / `SET FILTER TO` | Apply/clear filter |
| `SET DELETED ON` / `OFF` | Globally hide/show deleted DBF records during navigation |
| `SET EXACT ON` / `OFF` | Toggle character equality behaviour |
| `GO TOP`, `GOTO TOP`, `GO BOTTOM`, `GOTO BOTTOM` | Navigate to first/last visible record |
| `GO expression`, `GOTO expression` | Navigate to a **physical record number** |
| `SKIP`, `SKIP expression` | Move forward/backward through visible record order |
| `SEEK expression` | Lookup via active NDX |
| `LOCATE FOR expression` / `CONTINUE` | Locate matching row / resume prior locate |
| `APPEND BLANK` | Append a blank physical DBF record |
| `REPLACE field WITH expression` | Update one field in the current record |
| `DELETE` / `RECALL` | Set/clear current record's deletion flag |

The compact output form with two question marks emits one expression
without a trailing newline. Full dBASE expression-list formatting
and screen/printer controls remain out of scope for this slice.

Source statements normally end at a newline. A trailing semicolon outside
strings continues onto the next physical line, including when `&&` follows
the semicolon. Multiple semicolon-separated commands **on the same physical
line** are *not supported* and produce an explicit compiler error. Continued
statements map to the first physical line for diagnostics/breakpoints.

## Expressions and built-in functions

Numeric, character, logical and date values are currently implemented.
Variables and DBF fields can be referenced by name. Supported operators
include parentheses, unary `+` and `-`, arithmetic `+`, `-`, `*`,
`/`, numeric/character comparisons, strict `==`, dBASE `=` equality,
not-equal `#`, logical `.AND.`, `.OR.`, `.NOT.` and character
substring containment `$`. Booleans use `.T.` and `.F.`. Do not assume
short-circuit evaluation or every historical type coercion.

| Function | Arguments | Implemented result |
| --- | --- | --- |
| `EOF()`, `BOF()`, `FOUND()`, `DELETED()` | none | Work-area status |
| `RECNO()`, `RECCOUNT()` | none | Record position/count |
| `LEN(text)` | 1 | Byte length |
| `UPPER(text)`, `LOWER(text)` | 1 | ASCII case conversion |
| `TRIM(text)`, `RTRIM(text)`, `LTRIM(text)` | 1 | Remove ASCII spaces on selected edge(s) |
| `LEFT(text,n)`, `RIGHT(text,n)` | 2 | Byte-oriented text slices |
| `SUBSTR(text,start[,count])` | 2���3 | One-based text slice |
| `AT(needle,haystack)` | 2 | One-based search position |
| `SPACE(n)` | 1 | String of ASCII spaces |
| `REPLICATE(text,n)` | 2 | Repeated text |
| `VAL(text)` | 1 | Numeric conversion of leading text |
| `STR(number[,width[,decimals]])` | 1���3 | Width/precision-formatted number string |
| `ABS(number)`, `INT(number)` | 1 | Absolute value / truncated numeric value |
| `MIN(a,b)`, `MAX(a,b)` | 2 | Numeric extrema |
| `ASC(text)`, `CHR(byte)` | 1 | First byte value / byte-to-character |
| `STUFF(text,start,count,replacement)` | 4 | Byte-oriented string insertion/replacement |
| `CTOD(text)` | 1 | Parse a typed date |
| `DTOC(date)`, `DTOS(date)` | 1 | Date formatted as character text |
| `YEAR(date)`, `MONTH(date)`, `DAY(date)` | 1 | Numeric calendar component |

Character operations are currently byte-oriented, not Unicode/codepage
complete. Stored DBF character fields retain their fixed-width trailing
spaces; use `TRIM()` when presenting compact text. `SET EXACT` affects
`=` but not strict `==`. The implementation caps certain string and
formatting allocations; see [compatibility details](../developer/compatibility/dbase-iii-plus.md).

Dates use typed `D` values; `CTOD()` defaults to MM/DD/YY, with
two-digit years interpreted as 1900 + year until an epoch setting exists.
Blank date comparison and many locale/date settings remain unsupported.

## Deliberate gaps and DBF precautions

**Not implemented:** user-defined procedures/functions and parameters,
macro substitution, array declarations/expressions, `ELSEIF`, `FOR/NEXT`,
`SCAN/ENDSCAN`, multiple commands on one line, DBT memo support,
writable NDX maintenance, full historical codepages, or multi-user
record/file locking. A design proposal does **not** make these usable.

Running `REPLACE`, `DELETE`, `RECALL` or `APPEND BLANK` may modify a
DBF. **Use disposable copies**, never shared or production databases.
Writes are not rolled back by stopping the debugger, and modifying a table
with an active read-only NDX is deliberately rejected.

For detailed caveats, see
[dBASE III PLUS compatibility](../developer/compatibility/dbase-iii-plus.md)
and the [implementation backlog](https://github.com/brierfieldlabs/xabl/issues/52).
