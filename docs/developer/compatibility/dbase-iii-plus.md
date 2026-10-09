# dBASE III PLUS compatibility profile (implementation notes)

The first runtime target is dBASE III PLUS. Maximum practical compatibility is
the project's governing direction, but **support remains incomplete**.
An explicit version-specific compatibility object now exists, while
independent original-product reference files are still outstanding.

## Supported in these slices

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
- `GO BOTTOM` or `GOTO BOTTOM`: move to the last visible physical record
  when no index is active. A filter and global SET DELETED compose.
- Regression tests use real DBF byte persistence and disposable table copies.
  `tests/fixtures/legacy-mini-ledger.prg` exercises multiple work areas,
  aggregation, filters, searching, append and updates.

## Explicit restrictions

- `APPEND` without BLANK invokes dBASE's interactive data editor and is not
  yet implemented. Unsupported commands must fail, not silently alias to
  non-interactive `APPEND BLANK`.
- The current NDX implementation is read-only. `APPEND BLANK` refuses a
  work area with an active NDX index to avoid silently stale index records.
  `GO BOTTOM` also refuses active NDX until indexed-order navigation exists.
- String ordering, collations, locale/code-page handling and later-dialect
  `==` differences require separate compatibility work.
- The DBF writer is single-writer only. There is no cross-process locking,
  transaction rollback, crash-consistency guarantee, or automatic refresh
  of tables already open elsewhere. Do not use it concurrently on live data.
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
