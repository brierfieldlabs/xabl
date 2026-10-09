# ADR: A first-class DOS-inspired terminal IDE

Status: Accepted, 9 October 2026

The language shall eventually offer both a modern Qt desktop Studio and
a traditional character-cell terminal Studio. Both use xabl_core and the
same source/project files, dialect profiles, compiler, VM and data safety
rules. A text IDE is a first-class front end, not a second interpreter.

For the initial Linux implementation, ncurses is an optional build
dependency. The terminal-independent EditorBuffer core has its own unit
test and can be reused by a different Windows console adapter later.
The first UI supports opening, editing, saving, undo, finding text,
compile-only syntax checks, explicit confirmation before running code,
captured output, blue/cyan DOS-inspired colours, and keyboard F-keys.

Data safety: the editor prompts before discarding unsaved text, stages
saves in a sibling file before replacing targets, preserves conventional
CRLF, and rejects binary files and symbolic-link target replacement.
The interpreter still has no cross-process DBF locking; running a PRG
may modify open databases. No source is automatically executed on open.

The visual language evokes classic xBase/Turbo Pascal, without copying
brand graphics or claiming to emulate MS-DOS itself. This layer is
ASCII-oriented at first; real DOS codepage mapping, multi-document
management, breakpoint debugging and Qt UI remain separate milestones.
