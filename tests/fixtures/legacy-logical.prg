* dBASE-style logical operator and grouping compatibility fixture.

USE customers
GO TOP

IF balance > 100 AND name = "Alice"
    ? "word-and"
ENDIF

IF balance < 100 .OR. name = "Alice"
    ? "dot-or"
ENDIF

IF NOT balance < 100
    ? "word-not"
ENDIF

* Parentheses must override normal AND/OR precedence.
IF (name = "Alice" .OR. balance < 100) .AND. balance > 200
    ? "wrong-group"
ENDIF

IF (name = "Alice" .OR. balance < 100) .AND. balance > 100
    ? "grouped"
ENDIF

? (10 + 2) * 3

LOCATE FOR balance > 100 .AND. name <> "Alice"
? TRIM(name)