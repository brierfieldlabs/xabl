# ADR: Make extreme SKIP navigation overflow-safe

Status: Accepted, 9 October 2026

The original DbfTable::skip() performed signed addition of the current
record index and a caller-provided ptrdiff_t. This could overflow signed
integers, causing undefined behaviour for very large SKIP counts.
The VM also converted arbitrary doubles directly to ptrdiff_t without
checking their representable range. Filtered reverse SKIP negated a
negative count and could overflow on PTRDIFF_MIN.

The DBF cursor now uses unsigned, bounded distances to reach EOF or BOF
without doing signed current-position addition. Movement from BOF and EOF
retains legacy one-record boundary behaviour. For negative counts, use
-(count+1)+1 so the smallest representable signed value is handled safely.

The VM rejects non-finite and out-of-range numeric distances before
conversion, leaving the cursor unchanged after rejection. Distances
inside the supported range are truncated toward zero as before.
For filters and indexes, navigation exits when it encounters EOF or BOF
instead of iterating up to an enormous requested count.

Tests cover max/min ptrdiff_t directly; VM counts of +/-1e18;
filtered and index-ordered navigation to boundaries; recovery from BOF/EOF;
and rejection of +/-1e100 without moving the cursor.

These are XABL's documented defensive bounds, not a claim that all
historical dBASE products accepted arbitrarily large distances.
