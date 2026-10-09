* dBASE-style record navigation and status fixture.

USE customers
? RECCOUNT()

GO TOP
? RECNO()

SKIP 2
? RECNO()

SKIP 1
? EOF()
? RECNO()

GO TOP
SKIP -1
? BOF()
? RECNO()

GO 2
DELETE
? DELETED()
RECALL
? DELETED()