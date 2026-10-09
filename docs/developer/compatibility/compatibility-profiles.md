# Compatibility profiles

The compiler/runtime must preserve differences between historical xBase/dBASE
versions instead of gradually mutating one undefined interpretation. Profiles
are value objects in `xabl/runtime/profile.hpp`; compiled Program objects
carry a dialect identifier and the VM rejects programs stamped for a different
dialect.

## Explicitly supported now

| Canonical ID | Current status |
| --- | --- |
| `dbase-iii-plus` | Executable subset, with DBF/NDX and language gaps |
| `dbase-iv` | Reserved; rejected |
| `clipper` | Reserved; rejected |
| `foxpro` | Reserved; rejected |
| `visual-foxpro` | Reserved; rejected |
| `visual-dbase` | Reserved; rejected |
| `dbase-plus` | Reserved; rejected |
| `harbour` | Reserved; rejected |
| `xharbour` | Reserved; rejected |
| `modern` | Reserved; rejected |

This list is a roadmap, **not** a compatibility certification.

The default CLI behaviour is equivalent to:
`xabl --dialect dbase-iii-plus path/to/program.prg`.

Selecting a recognised but unimplemented profile intentionally raises an
error. This prevents silently executing dBASE IV or Visual FoxPro programs
with III PLUS semantics. Unknown names are rejected separately.

The current stamp is in-memory C++ metadata, **not yet** a versioned on-disk
bytecode format or a manifest lockfile. Once the VM is serialised, its format
must contain both a bytecode version and the dialect/profile revision so that
old executables retain their behaviour. Embedded filter bytecode is stamped
with the same compiler profile.

## Compatibility work still required

1. Independently generated fixtures from original dBASE III PLUS tools.
2. Parser/compiler feature gates: SCAN is dBASE IV, not III PLUS.
3. SET EXACT and strict equality behaviour across later dialects.
4. Locale/codepage, date semantics, index order, memo records and locking.
5. Versioned bytecode with permanent compatibility packs.
6. CLI and package metadata selection, including reproducible profile locks.

Do not treat the profile identifier alone as proof that all historical
semantics are implemented.
