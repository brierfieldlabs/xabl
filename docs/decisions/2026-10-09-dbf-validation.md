# ADR: Validate DBF headers and refuse stale indexed writes

Status: Accepted, 9 October 2026

## Physical DBF validation

The dBASE III PLUS DBF field descriptor terminator is one byte (0x0D), not
another complete 32-byte descriptor. The reader first reads one byte and
only consumes the remaining 31 bytes for actual field descriptors. A valid
empty DBF can therefore be opened and appended to without requiring
fictitious padding after the terminator.

Reject invalid header/record lengths, declared record counts exceeding
physical file contents, missing field terminators, malformed field names
and field layouts extending beyond a record. Validate the file size before
reserving memory for the declared count to avoid huge allocations from a
corrupt header.

This validation does not imply support for all DBF version bytes, memo file
variants, proprietary trailers or cross-process writers.

## Index write safety

NDX remains read-only, with arbitrary key expressions. REPLACE is rejected
whenever an NDX is active, because replacing even an apparently unrelated
field could invalidate indexed ordering. Closing the index with bare
SET INDEX TO permits unindexed REPLACE, but index rebuild/refresh must be
explicit before reuse.

The DBF writer is still not concurrent/transactional. These guards prevent
a known stale-index failure, not all forms of data corruption.
