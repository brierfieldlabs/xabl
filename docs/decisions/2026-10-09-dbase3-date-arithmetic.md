# ADR: Add dBASE III PLUS date arithmetic with bounded day offsets

Status: Accepted, 9 October 2026

Original dBASE III PLUS supports adding/subtracting an integer number of
calendar days to/from a typed date value and subtracting two dates to
calculate the signed number of days between them.

The shared additive-expression runtime now resolves operand types before
numeric/string coercion:
- DATE + whole-number -> DATE
- DATE - whole-number -> DATE
- DATE - DATE -> numeric day difference
- Other date operand combinations fail explicitly rather than being
  misinterpreted as numbers or character strings.

Arithmetic uses C++23 chrono::sys_days and the proleptic Gregorian calendar
for the currently supported AD 1..9999 DateValue range. Original-product
calendar peculiarities and locale rules still require dialect-labelled
reference fixtures.

Defensive restrictions are explicit: both date operands must be valid and
populated. Blank dates, fractional day counts, non-finite values, and
absolute offsets over 3,660,000 days are rejected. The resulting date
must stay in the supported year range. These bounds are implementation
safety guards, not alleged original dBASE day-offset limits.

The ordinary VM and DBF SET FILTER evaluator already share the same
apply_additive_operator helper, so tests include date expressions
embedded in filters. Regression cases span leap days, year/century
boundaries, subtraction sign, negative results, overflow, fractional
values, invalid operand types and blank dates. Generated fixtures are
disposable and never modify a user's DBF.

Historical source: Programming With dBASE III PLUS, date arithmetic:
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
