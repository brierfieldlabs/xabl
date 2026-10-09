* dBASE III PLUS-style memory variable/arithmetic fixture.

USE customers
GO TOP
STORE 0 TO total

DO WHILE .NOT. EOF()
    total = total + balance
    SKIP
ENDDO

? total