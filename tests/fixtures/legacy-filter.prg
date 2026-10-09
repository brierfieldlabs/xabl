* dBASE-style SET FILTER TO compatibility fixture.

USE customers
SET FILTER TO balance > 150

* Filter does not move the current record until navigation occurs.
? name

GO TOP
? name

* Direct GO can still land on a record hidden by the active filter.
GO 2
? name

* Sequential navigation resumes through the filter.
SKIP
? name

SET FILTER TO
GO TOP
SKIP
? name