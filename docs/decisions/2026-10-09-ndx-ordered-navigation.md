# ADR: Navigate read-only NDX in physical-record-pointer order

Status: Accepted, 9 October 2026

dBASE's active index governs ordinary navigation even when physical DBF
record layout differs from lexical key order. The old runtime treated
GO TOP and SKIP as physical operations despite an open index.

The read-only NDX reader now traverses the 512-byte page B-tree in order,
collecting the physical DBF row pointers stored with keys. A work area's
GO TOP, GO BOTTOM and SKIP follow that ordered snapshot when an NDX is
active; SET FILTER and SET DELETED visibility compose with it. GO n is
still physical-record addressing. EOF/BOF semantics remain visible-record
boundaries.

Traversal refuses out-of-file, repeated/cyclic or over-deep page references,
duplicate physical records and invalid physical row pointers. A regression
fixture intentionally swaps the physical DBF rows and updates the NDX
pointers without changing key order so tests cannot accidentally succeed
by walking the original physical order.

The NDX remains read-only and is reparsed for navigation. This slice does
not provide indexed writes, duplicate-key collation compatibility,
key-expression updates, locking or independent genuine dBASE-generated
NDX interoperability certification.
