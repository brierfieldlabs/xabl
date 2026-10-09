# ADR: Add actual dBASE III PLUS date values and DBF D fields

Status: Accepted, 9 October 2026

## Data representation

Date is a separate Value alternative (DateValue), never an alias for
character or double. DateValue stores year, month and day as components,
with all zeros representing the historical blank date.

The DBF III PLUS D field uses exactly eight ASCII bytes in YYYYMMDD
order, or eight space bytes for a blank. A D field descriptor is admitted
only when its width is 8 and its decimal count 0. The decoder checks
ASCII digits, AD 1..9999, valid calendar month and day, including leap
years. Invalid field values fail on read, not silently become strings.
The encoder validates the date before a record is mutated and writes
the exact eight bytes. APPEND BLANK automatically produces eight spaces.

## Expression functions

- CTOD(text) produces a typed DateValue. The initial profile accepts the
  documented default MM/DD/YY and explicit MM/DD/YYYY forms. A two-digit
  year uses 1900+YY until SET EPOCH / SET CENTURY support is implemented.
  Malformed text produces a blank date; an excess day in a valid month is
  normalised forward as described in the original III PLUS handbook.
- DTOC(date) produces MM/DD/YY output. Both direct PRINT of a DateValue
  and DTOC use the default display format.
- DTOS(date) produces eight-character YYYYMMDD text, or eight spaces for
  blank dates, suitable for chronological lexical sorting.
- YEAR(date), MONTH(date) and DAY(date) return numeric components, 0 for
  a blank date.

All six opcodes use the same helper in normal VM execution and embedded
SET FILTER bytecode. Comparison of two nonblank DateValue operands is
chronological, and unlike string comparisons cannot apply SET EXACT.
Mixed-type comparisons throw. **Blank date comparisons are currently
refused**, rather than guessing at historic equality/inequality quirks.

## Scope and limitations

This slice is an executable dBASE III PLUS subset, not full date parity.
It does not implement SET DATE, SET EPOCH, SET CENTURY, dynamic DATE(),
date arithmetic/differences, literal dates for later dialects, macro
substitution, alternate locales, or the historical blank-date comparison
peculiarities. Original-product tests are required to confirm these
boundaries. Stored fields are byte-compatible and never auto-converted
through the local machine timezone.

A new independent nine-test CTest suite tests synthetic but real DBF
bytes containing leap days, blank dates, and later years, byte-exact
date replacement and APPEND BLANK, comparison/filters, invalid calendar
inputs, descriptor widths and byte preservation on rejected writes.
The existing DBF schema-validation test was updated to reject an
invalid-width D field rather than incorrectly claiming that all D
descriptors are unsupported.

## References

- Programming With dBASE III PLUS, CTOD correction and date comparison:
  https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
- dBASE Language Handbook, CTOD and date operand type:
  https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
- dBASE file structure, the eight-character D descriptor:
  https://www.dbase.com/Knowledgebase/int/db7_file_fmt.htm
