# ADR: Fail closed for unsupported legacy profiles

Status: Accepted, 9 October 2026

We want permanently retained historical behaviour, not a single modern
dialect that gradually drifts. Introduce a first-class CompatibilityProfile,
canonical textual IDs, and a dialect stamp on compiled Program objects.
Compiler and VM constructors reject unimplemented profiles; the VM also
checks program/runtime profile agreement before execution.

This does not change existing default dBASE III PLUS behaviour and remains
source compatible with existing default constructors. The CLI can explicitly
select the implemented profile. Later dialect identifiers are reserved but
cannot be executed until their compatibility tests and feature gates land.

An in-memory Program stamp is not yet a persisted/locked bytecode format.
Those require separate versioning and backward-compatibility policy.
