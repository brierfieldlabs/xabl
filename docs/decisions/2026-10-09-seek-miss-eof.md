# ADR: A failed dBASE III PLUS SEEK positions at EOF

Status: Accepted, 9 October 2026

The NDX reader returns physical record number zero when a key is absent.
The initial VM forwarded this value directly to DbfTable::go_record(0),
which means BOF, not EOF. That contradicts original dBASE III PLUS
programming documentation: unsuccessful SEEK places the cursor just
after the last physical record, with EOF() true and FOUND() false.

On a miss, set the cursor to RECCOUNT()+1 using existing go_record()
semantics; do not conflate a failed search with GO 0 or backward SKIP.
On a successful search, preserve the physical row pointer, EOF false
and FOUND true. Keep the independent work-area FOUND state.

The shuffled-DBF NDX regression now checks BOF(), EOF(), FOUND(),
RECNO(), successful and missing SEEK, and SKIP -1 following a miss.

Reference: Programming With dBASE III PLUS, Computer History Museum
archive, 'The End-of-File Condition' and SEEK.
https://archive.computerhistory.org/resources/access/text/2024/05/102734488-05-0003-acc.pdf
