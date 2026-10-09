# ADR: Fail closed on unsupported DBF field descriptors

Status: Accepted, 9 October 2026

The original DBF III field reader treated every type other than N/F/L
as character text, and the writer used the same character fallback.
That silently interpreted D (typed date), M (DBT memo pointer) and
unknown later-version types as ordinary character fields. A future
REPLACE could corrupt the original typed field data.

In the current explicitly incomplete dBASE III PLUS profile, the DBF
file reader now accepts only the implemented field types C, N, F, and L.
Unimplemented D, M and other field descriptors cause a clear runtime
error when opening the table, before any rows are read or mutated.
The field() and replace() paths also reject unexpected field types
rather than implicitly encoding them as strings.

Additional descriptor validation now rejects duplicate case-normalised
field names, L types with a width other than one, decimal counts on
C/L types, and numeric fields with a decimal count not smaller than
field width. This deliberately constrains only the current supported
subset; legitimate special field variants in later xBase dialects
must be enabled under their own versioned profiles and tests.

Regression tests mutate descriptor bytes in disposable synthetic DBF
copies to simulate D, M, unknown types, inconsistent character/numeric
decimals and duplicate names. They verify errors and byte-for-byte
file preservation. Do not change original test fixtures or user DBFs.

This refusal is temporary, NOT a claim that dBASE III PLUS lacks
proper date or memo support. Implement typed D values and DBT memo
resolution before enabling those descriptor types.
