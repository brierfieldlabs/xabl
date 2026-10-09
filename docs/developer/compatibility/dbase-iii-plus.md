# dBASE III PLUS compatibility profile (implementation notes)

The first runtime target is dBASE III PLUS. Maximum practical compatibility is
the project's governing direction, but **support remains incomplete**.
An explicit version-specific compatibility object now exists, while
independent original-product reference files are still outstanding.

## Supported in these slices

- PRG line comments starting with `*`, `NOTE` and `&&`; inline
  `&&` comments are stripped without damaging quoted literals. Bare
  `?` prints a blank line, while `?expression` works without a space.
  Semicolon line continuation remains unimplemented.
- STORE TO, REPLACE WITH and USE ALIAS separators are now recognised
  outside quoted character expressions and parenthesised calls; embedded
  keyword text is retained as a literal, not a spurious clause.

- Basic character `>`, `<`, `>=`, `<=` relational comparisons
  now use unsigned bytewise lexical ordering. The traditional `#`
  not-equal spelling is recognised. Mixed numeric/character relational
  values raise an explicit type mismatch.
- The character `+` and `-` operators concatenate strings.
  Plus preserves both operands verbatim; minus moves trailing ASCII
  spaces from the left operand behind the concatenated right operand.
  Numeric arithmetic retains its normal behaviour. Mixed types currently
  raise an explicit error pending dialect-specific conversions.
- Character functions `LEN()`, `UPPER()`, `LOWER()`,
  `TRIM()`/`RTRIM()`, `LTRIM()`, `LEFT()`, `RIGHT()`, and
  `SUBSTR()` with an optional length argument, and two-argument `AT()`
  for one-based substring search. Nested expressions and active filters
  share byte-oriented results. ASCII case mapping is implemented;
  original DOS codepage mapping remains pending.
- `SPACE(n)` generates `n` ASCII spaces and `REPLICATE(text,n)`
  repeats a character expression `n` times. Both validate finite
  numeric counts and apply a documented 1 MiB implementation safety cap.
  This limit is not asserted to be the historical interpreter's limit.

- `SET EXACT ON/OFF`: global character equality mode; OFF (default)
  compares the left string against the right prefix, ON compares after
  trimming trailing ASCII spaces. `==` is separate strict byte equality.
  Filter programs share the same runtime setting.
- `IF/ELSE/ENDIF` and `DO WHILE/LOOP/EXIT/ENDDO`: nested control
  flow, condition rechecking and early-loop exit.
- `APPEND BLANK`: append a physical blank DBF record, update the 32-bit
  header record count and the last-update date, retain/write the 0x1A EOF byte,
  and leave the record pointer on the new row.
- `USE` with no filename: close current work area's table, index, filter,
  FOUND state and alias, without closing another work area.
- `SET INDEX TO` with no filenames: close current active NDX index.
- `GO TOP`, `GO BOTTOM`, `GOTO BOTTOM` and `SKIP`:
  navigate NDX key order when indexed, physical order otherwise. Filters
  and global SET DELETED visibility compose with both modes; direct
  `GO n` still addresses physical record numbers. Extreme SKIP
  distances saturate at BOF/EOF; non-finite and unrepresentable numeric
  counts are rejected before conversion to the VM's native index type.
- Regression tests use real DBF byte persistence and disposable table copies.
  `tests/fixtures/legacy-mini-ledger.prg` exercises multiple work areas,
  aggregation, filters, searching, append and updates.

## Explicit restrictions

- `APPEND` without BLANK invokes dBASE's interactive data editor and is not
  yet implemented. Unsupported commands must fail, not silently alias to
  non-interactive `APPEND BLANK`.
- The current NDX implementation is read-only. `APPEND BLANK` and
  `REPLACE` refuse a work area with an active NDX index to avoid silently
  stale key expressions and index records.
  Indexed navigation is read-only; there is no NDX write maintenance yet.
  NDX SEEK and ordered traversal both validate physical page references,
  key counts and out-of-range records, rejecting corrupt files.
  A missing SEEK key sets FOUND() false and moves to physical EOF
  (RECNO() = RECCOUNT()+1), not BOF.
- String ordering, collations, locale/code-page handling and later-dialect
  `==` differences require separate compatibility work.
- The DBF writer is single-writer only. Record updates now validate the
  physical row preimage, header and trailer before writing and restore
  cached values after a rejected write. This prevents some sequential stale
  updates but is **not** a cross-process lock, atomic transaction, crash-
  consistency guarantee, or automatic refresh. Do not use it concurrently
  on live data.
- DBF header validation now rejects impossible physical record counts and
  malformed field terminators before allocating. The reader supports empty
  DBFs with a one-byte field-descriptor terminator.
- Unknown trailing DBF content is refused, not truncated. Only an optional
  standard 0x1A terminator is recognised. Real-world legacy DBF/NDX samples
  remain essential before claiming broad file-format compatibility.
- `SCAN ... ENDSCAN` is a dBASE IV-era construct. Do not enable it by
  default in the III PLUS profile. An explicit dialect framework should
  gate later-dialect language syntax.

## Reference documentation

- Historic command summary:
  https://pic.hallikainen.org/techref/language/dbase/commands.htm
- DBF III PLUS header/record layout:
  https://blogs.embarcadero.com/dbase-dbf-file-structure/
- Later USE close semantics:
  https://www.dbase.com/help/2019_0/Xbase/IDH_XBASE_USE.htm
- Language Handbook (dialect-labelled SCAN / APPEND / GO):
  https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf

- Historical SET EXACT reference:
  https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf

## Fixed-width character values

DBF C fields preserve their declared length, including trailing ASCII
spaces. LEN(NAME) reports field width, while LEN(TRIM(NAME)) reports the
unpadded content length. Printing an untrimmed field also prints spaces.
Legacy test programs now use TRIM() explicitly for compact names. See
the 2026-10-09 fixed-width character-fields ADR. Codepages remain pending.

## Numeric/character conversion

VAL(text) converts a leading numeric prefix to a number, returning zero for
non-numeric text. STR(number[,width[,decimals]]) returns right-aligned,
fixed-decimal character output (default 10 characters, 0 decimals). It emits
asterisks when a valid width is too small for the number. Invalid formatting
width/precision is refused separately. Both functions run in ordinary and
filter expressions. Current runtime safety limits are 1 MiB width and 18
decimal places; these are not claimed as historical dBASE III PLUS limits.

## Basic numeric functions

ABS() calculates absolute value, INT() removes fractional digits toward
zero, and MIN()/MAX() compare two numeric arguments. All require finite
numeric operands. They work inside SET FILTER as well as normal expressions.
ROUND() remains pending explicit per-version tests for documented historical
dBASE III PLUS rounding defects.

## Character-code conversion

ASC(text) returns the numeric byte code of the first character;
CHR(code) creates a one-byte string from code 0..255. Extended bytes
are preserved verbatim. These functions do not translate codepages or
provide Unicode characters. Empty ASC inputs, non-finite/wrong-typed
codes and out-of-range CHR inputs are rejected pending exact original
interpreter boundary tests.

## String editing

STUFF(target,start,quantity,replacement) performs one-based byte-oriented
replacement and insertion. Four arguments are required, including an
explicit replacement string. Start positions beyond the target append;
nonpositive quantities insert without deletion. Results are bounded to
1 MiB as a modern XABL safety restriction, not an asserted DOS limit.

## Substring comparisons

The $ operator tests whether its left character expression occurs
anywhere within its right character expression. It is case-sensitive,
byte-oriented, false for an empty substring, and independent of
SET EXACT. Its boolean result differs from AT()'s numeric position.
DBF filters use the same bytecode operation as normal expressions.

## Safe numeric GO record addressing

GO with a numeric physical record target validates that the value is
finite, nonnegative and representable as the host record index before
conversion. Values beyond RECCOUNT reach EOF; invalid targets are
rejected without moving the record pointer. This is a host safety rule.

## DBF field type admission

Only C, N, F and L descriptors are executable in the current DBF
reader/writer subset. D (date), M (memo) and unknown later-dialect
descriptors now fail explicitly, rather than being silently treated as
strings. This is a deliberate compatibility limitation pending proper
typed date storage and DBT support, NOT historical dBASE behaviour.
Duplicate field names and invalid width/decimal descriptors are refused.

## Work-area selection

SELECT 1 through SELECT 10, or SELECT A through SELECT J, chooses one
of the ten dBASE III PLUS work areas. File aliases remain usable as
selectors. Numbers outside this range and unknown aliases are rejected
without changing the active work area. Other historical dialects may
support different work-area limits.
