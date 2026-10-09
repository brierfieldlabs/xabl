* dBASE III PLUS-style SET DELETED compatibility fixture.

USE customers
GO 2
DELETE

SET DELETED ON
GO TOP
? TRIM(name)
SKIP
? TRIM(name)
SKIP -1
? TRIM(name)

* Direct GO addresses the physical record even when deleted rows are hidden.
GO 2
? TRIM(name)
? DELETED()

SET DELETED OFF
GO TOP
SKIP
? TRIM(name)

* Leave the shared fixture clean for later tests.
RECALL