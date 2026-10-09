* dBASE-style work area and alias compatibility fixture.

SELECT 1
USE customers ALIAS cust
GO TOP
SKIP

SELECT 2
USE customers ALIAS copy
GO TOP

? cust->name
? copy->name

SELECT cust
? name