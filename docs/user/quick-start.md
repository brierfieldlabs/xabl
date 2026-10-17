# XABL quick start

XABL is a pre-release implementation of a **dBASE III PLUS subset**.
Its C++ compiler/runtime, command-line tool and DOS-inspired Text Studio
all use the same language implementation. It does **not** yet promise
compatibility with every old PRG or safe concurrent access to live DBFs.

## Build the tools

On a Debian/Linux development environment with a C++23 compiler, CMake,
Python 3 and optionally ncurses development headers:

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
~~~

This builds `build/xabl`. If ncurses is available it also builds
`build/xabl-tui`. XABL's **authoritative automated CI** builds and
runs the existing CTest suites on the Brierfield CT141 GitHub Actions
runner. Manual local builds are not a substitute for those PR checks.

## Write your first PRG

Create a file named `hello.prg` containing:

~~~xbase
* A small program using classic dBASE syntax.
STORE 3 TO n
DO WHILE n > 0
    ? n
    n = n - 1
ENDDO
~~~

Check the file **without executing it**:

~~~sh
./build/xabl --check hello.prg
~~~

A successful check exits with status zero and no program output. An
invalid command or expression produces a source-line error and a
non-zero exit status.

Run the program:

~~~sh
./build/xabl hello.prg
~~~

Expected output:

~~~text
3
2
1
~~~

There is no project manifest required for a single PRG. The CLI
uses the source file's parent directory as the program working
directory. The only implemented dialect is currently
`--dialect dbase-iii-plus` (the default). A requested unsupported
dialect is rejected rather than silently emulated.

## Use the DOS-inspired Text Studio

~~~sh
./build/xabl-tui hello.prg
~~~

Some useful keys: **F1** Help, **F2** Save, **F3** Open,
**F9** Build/check only, **Ctrl+F9** Run/continue,
**Ctrl+F8** Breakpoint, **F7/F8** step one statement,
and **F10** command menu when a terminal cannot send modified
function keys. The editor asks for confirmation before running.
See the [complete Text Studio guide](text-studio.md) for its
key bindings and current limitations.

## VS Code is optional

The [XABL VS Code extension](../../extensions/vscode/README.md)
provides `.prg` and `.xabl` syntax colouring and commands for
checking and running via the same C++ `xabl` executable.
It is not a separate implementation of the language. Graphical
debugger integration remains planned, not implemented.

## Before working with DBF files

XABL can read and modify selected classic DBF fields, but some
memo/index formats, index writes, multi-user locking and several
legacy language features remain incomplete. Use **copies of
disposable test data**, not production DBFs. Debugging or running
a program may write records; resetting a debug session does
not undo previously completed changes.

For the implemented subset and known gaps, see
[dBASE III PLUS compatibility](../developer/compatibility/dbase-iii-plus.md).
Development requests and next steps belong in
[GitHub Issues](https://github.com/brierfieldlabs/xabl/issues),
particularly [documentation issue #63](https://github.com/brierfieldlabs/xabl/issues/63).

For the precise currently supported commands and functions, see the
[implemented language reference](implemented-reference.md).
