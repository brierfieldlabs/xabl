* dBASE III PLUS-style SET DELETED compatibility fixture.

USE customers
GO 2
DELETE

SET DELETED ON
GO TOP
? name
SKIP
? name
SKIP -1
? name

* Direct GO addresses the physical record even when deleted rows are hidden.
GO 2
? name
? DELETED()

SET DELETED OFF
GO TOP
SKIP
? name

* Leave the shared fixture clean for later tests.
RECALL