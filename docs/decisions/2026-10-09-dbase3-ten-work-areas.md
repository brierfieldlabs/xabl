# ADR: Enforce the dBASE III PLUS ten-work-area selector

Status: Accepted, 9 October 2026

The dBASE III PLUS SELECT statement accepts work-area numbers 1-10,
letters A-J, or a table alias. The earlier runtime accepted all positive
numeric values, narrowed them unsafely to an int, and treated single
area letters as file aliases rather than area selectors.

The III PLUS profile now recognises SELECT 1..10 or SELECT A..J (case
insensitively) as the corresponding numbered work area. SELECT by an
open table alias retains its previous behaviour. Nonpositive numbers,
11 and above, unknown letters and unresolvable aliases raise errors
before changing the active work-area number.

This is a version-specific limit and must not be imposed on future
Clipper, Visual FoxPro or other dialects with different numbers of
work areas without separate profile-specific rules.

Regression coverage tests the A/J selectors, 1/10 numeric selectors,
independent record cursors, selecting by aliases, and failed numeric
and character selections that leave the previous area unchanged.

Historical reference: dBASE Language Handbook, SELECT, states that
work areas are numbered 1-10, labelled A-J, or identified by alias.
https://www.terrellamedia.com/wp-content/uploads/2022/01/dBASE-Language-Handbook-by-David-M-Kalman-Final.pdf
