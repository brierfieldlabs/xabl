* dBASE-style LOCATE/CONTINUE compatibility fixture.

USE customers

LOCATE FOR balance > 100
? FOUND()
? name

CONTINUE
? FOUND()
? name

CONTINUE
? FOUND()
? EOF()