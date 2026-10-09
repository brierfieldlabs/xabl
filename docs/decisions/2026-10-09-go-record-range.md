# ADR: Check GO record numeric conversions before casting

Status: Accepted, 9 October 2026

The GO/GOTO record-number bytecode previously rejected negative numeric
values but converted all other doubles directly to size_t, including
NaN, infinity and numbers beyond the range of the target's unsigned
index type. C++ floating-point-to-integer conversion outside the
representable range is undefined behaviour.

Check that the numeric operand is nonnegative, finite and less than
2^size_t_digits before conversion. The upper limit is exclusive because
IEEE double cannot represent every high integer below size_t's maximum,
while powers of two are represented exactly. Retain the current subset
semantics of truncating a representable fractional record number toward
zero and mapping zero to BOF; future original-product compatibility tests
may refine those rules.

An in-range GO to a number beyond RECCOUNT positions the cursor at EOF.
An invalid or unrepresentable GO raises a runtime error before moving
the cursor. Regression coverage checks GO 1e100, GO -1, valid GO 1e18
(clamped by DbfTable to EOF), direct GO 2 and GO 0 BOF navigation.

This is defensive host-integer handling, not an assertion of historical
64-bit dBASE record-number capability.
