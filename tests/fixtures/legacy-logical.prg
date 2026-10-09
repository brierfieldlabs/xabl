* dBASE-style logical operator compatibility fixture.

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

LOCATE FOR balance > 100 .AND. name <> "Alice"
? name