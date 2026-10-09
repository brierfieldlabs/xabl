# ADR: Optimistic preimage validation for DBF record writes

Status: Accepted, 9 October 2026

Legacy DBF files may be opened for long periods before REPLACE, DELETE or
RECALL writes a physical record. The previous writer used the currently
loaded header length but did not verify that the physical row being written
still matched the in-memory record snapshot. A second process could edit
the same record, and the stale work area would overwrite it.

For these single-record writes, retain the loaded DBF header length and:
- Re-read and validate on-disk file version, header length, record length
  and record count against the open table.
- Reject changed file sizes and invalid trailing bytes (the optional 0x1A
  terminator remains permitted).
- Compare the target physical record byte-for-byte against its original
  in-memory preimage before writing.
- Flush output and report write errors.
- Restore the in-memory record after a rejected or failed write.
- Reject REPLACE when positioned at BOF, as with DELETE/RECALL.

Regression tests perform real external edits, reject stale REPLACE/DELETE
without changing disk contents, verify in-memory rollback and successful
writing after restoring the original preimage. They also check header drift,
unexpected trailers and BOF rejection on disposable copies of generated
DBF fixtures.

**Safety limits:** this is optimistic stale-write detection, not a
cross-process lock or atomic transaction. Another writer can intervene
between the preimage comparison and writing; partial writes may still
occur after a disk error. Other already-open records are not refreshed.
Before supporting shared writable DBFs, add platform locking, durable
transaction/recovery policy and failure-injection testing.

This is not an assertion that historical dBASE versions implemented
equivalent error handling.
