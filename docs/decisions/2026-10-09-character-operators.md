# ADR: Share numeric and legacy string + / - semantics

Status: Accepted, 9 October 2026

dBASE-family programs use + and - for both numeric arithmetic and character
concatenation. The old runtime always invoked as_number(), which broke
ordinary statements like:
    ? TRIM(first_name) + " " + last_name

For two character operands, + joins strings without removing any spaces;
- joins strings while moving all trailing ASCII spaces from the left operand
to the end of the resulting value. Thus "A  " - "B" is "AB  " of length four.
Both operations retain left-associative expression parsing and do not
silently remove any padded field width.

For numeric operands, preserve existing arithmetic. Reject a mixed
character/numeric pair with a descriptive runtime type error until explicit
conversions and historical coercion profiles are developed. A private
apply_additive_operator helper is used identically by main VM execution
and embedded SET FILTER evaluation.

The current subset does not yet implement DOS codepage collation, mixed
legacy date and numeric operations, string length limits of historical
versions, or dialect-specific implicit casts.

References: Computer History Museum dBASE II original guide (string -),
Clarion dBASE III key definitions (string +), current dBASE operator list:
https://eaw.app/Downloads/Manuals/CPM/dBASE_II_Users_Guide_Feb83.pdf
https://clarion.help/doku.php?id=dbaseiii_other.htm
https://www.dbase.com/help/9_2/Language_Definition/IDH_LDEF_OPS.htm
