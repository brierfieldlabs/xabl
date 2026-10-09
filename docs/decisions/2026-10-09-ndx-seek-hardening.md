# ADR: Bound NDX SEEK on untrusted legacy indexes

Status: Accepted, 9 October 2026

The NDX reader has two navigation paths: in-order traversal and direct SEEK.
The ordered path already validates cycles, bad page references and key counts,
but SEEK initially lacked equivalent protections. Malformed NDX page pointers
could therefore make SEEK loop indefinitely.

SEEK now checks physical page count and size, rejects repeated/cyclic or
out-of-range page references, validates entry count before any entry reads,
and verifies numeric-key length and supported key types. The VM rejects an
NDX SEEK match pointing beyond the opened DBF's physical record count.

Regression fixtures include intentionally cyclic pages, impossible key counts
and out-of-range physical DBF row pointers. These guards do not establish
full historical NDX dialect interoperability or indexed write support.
