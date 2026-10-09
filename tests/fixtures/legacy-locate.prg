* dBASE-style LOCATE/CONTINUE compatibility fixture.

USE customers

LOCATE FOR balance > 100
? FOUND()
? TRIM(name)

CONTINUE
? FOUND()
? TRIM(name)

CONTINUE
? FOUND()
? EOF()