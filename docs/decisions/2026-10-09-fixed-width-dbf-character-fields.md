# ADR: Preserve fixed-width DBF III PLUS character fields

Status: Accepted, 9 October 2026

In dBASE III PLUS a DBF character field is space-padded to its declared
width. The earlier DbfTable::field() removed trailing ASCII blanks on
read, destroying information used by expressions. A 20-byte NAME field
containing Alice must yield LEN(NAME) = 20, while LEN(TRIM(NAME)) = 5.

Return the entire raw C field value to expression evaluation. TRIM()
and RTRIM() remain explicit string operations. Printing the raw NAME
field now prints its trailing spaces; a program wanting compact display
output should explicitly request TRIM(NAME).

Synthetic PRG test fixtures were updated to use explicit trimming where
they expect compact output. Test harness output is NOT blanket-trimmed.
New strict regressions check 20-byte NAME and 10-byte STATUS values,
LEN, LEN(TRIM()), RIGHT, UPPER, equality and SET EXACT, a blank C field,
and raw printing with its exact trailing spaces.

This does not add DOS codepage support or change how unimplemented
memo, date, or other dialect types should behave. Separate genuine
historical dBASE output comparisons remain necessary for certification.

Original guide: Programming With dBASE III PLUS, chapter on trimming
and field length.
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf

DBF storage reference: character fields are padded with blanks to their
declared width.
https://www.dbase.com/Knowledgebase/int/db7_file_fmt.htm
