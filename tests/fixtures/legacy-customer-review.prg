* dBASE III PLUS-style compatibility fixture.
* The syntax is intentionally classic rather than modernised XABL.

USE customers
GO TOP

DO WHILE .NOT. EOF()
    IF balance > 100
        ? TRIM(name)
        REPLACE status WITH "REVIEW"
    ENDIF
    SKIP
ENDDO