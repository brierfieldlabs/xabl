# ADR: Expose ASC and CHR as byte-oriented legacy functions

Status: Accepted, 9 October 2026

Original dBASE III PLUS offers ASC(character-expression) to get the
code of the leftmost character, and CHR(numeric-expression) to construct
a character from a numeric byte code.

In the current VM, strings store raw DBF-compatible bytes. Implement
ASC as the value of the first unsigned byte, returning a numeric 0..255,
and CHR as exactly one byte with an integer code 0..255. The code
argument must be a finite number, truncated toward zero before the
byte cast. Out-of-range codes and wrong operand types raise an explicit
runtime error. ASC of an empty string also raises an error, avoiding
undefined reads; exact original empty-string handling remains to be
cross-checked with a historical interpreter.

Both functions compile to distinct one-argument opcodes, with shared
implementations for normal VM and embedded SET FILTER evaluation.
Regressions cover ASCII codes and extended codes, CHR(0) retaining a
one-byte NUL, nested operations, source-code handling, numeric range
errors and DBF filtering.

Bytes 128..255 are preserved, not mapped to Unicode or the current
terminal character set. The meaning and display of those bytes depend
on DOS language drivers and codepages such as CP437 and CP850, which
are not yet implemented. This slice does not claim codepage parity.

Historical sources:
- dBASE III PLUS programming guide, ASC/CHR examples:
  https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
- dBASE company language drivers and codepages:
  https://www.dbase.com/Knowledgebase/faq/language_drivers.asp
