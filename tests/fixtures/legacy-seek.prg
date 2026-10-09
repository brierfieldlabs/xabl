* dBASE III PLUS-style NDX/SEEK compatibility fixture.

USE customers
SET INDEX TO customers
SEEK "Charlie"

IF FOUND()
    ? name
ENDIF