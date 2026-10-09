#!/usr/bin/env python3
"""Integration-smoke the actual curses IDE under an isolated pseudo-terminal.

This runs a harmless arithmetic program, never a DBF-writing program.
No actual user's source files are changed; all files live in TemporaryDirectory.
"""
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import tempfile
import termios
import time
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        return 2
    with tempfile.TemporaryDirectory(prefix="xabl-tui-pty-") as directory:
        source = Path(directory) / "program.prg"
        source.write_bytes(b"? 2 + 3\n")
        pid, fd = pty.fork()
        if pid == 0:
            os.environ["TERM"] = "xterm-256color"
            os.environ["LC_ALL"] = "C"
            os.execv(sys.argv[1], [sys.argv[1], str(source)])
        transcript = bytearray()

        def wait_for(token: bytes, timeout: float = 5.0) -> None:
            start = len(transcript)
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                if token in transcript[max(start - 100, 0):]:
                    return
                try:
                    if select.select([fd], [], [], 0.1)[0]:
                        part = os.read(fd, 16384)
                        if not part:
                            break
                        transcript.extend(part)
                except OSError:
                    break
            raise AssertionError("terminal did not display " + repr(token) +
                                 "; transcript end=" + repr(transcript[-500:]))

        def send(keys: bytes) -> None:
            os.write(fd, keys)

        try:
            fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 25, 100, 0, 0))
            wait_for(b"XABL Text Studio")
            wait_for(b"F1 Help")
            # F9: compile-only. Program output should say "Compilation"
            # without executing the program or showing its result.
            send(b"\x1b[20~")
            wait_for(b"Compilation successful.")
            send(b"\x1b[17~")  # F6: editor
            wait_for(b"EDITOR")
            send(b"\x1b[15~")  # F5: run, user must explicitly approve
            wait_for(b"Run source?")
            send(b"y")
            wait_for(b"Program completed.")
            send(b"\x1b[17~")  # F6: editor
            wait_for(b"EDITOR")
            # Save a harmless edit via Ctrl+S, then quit via Ctrl+Q.
            send(b"* harmless note\n")
            send(b"\x13")
            wait_for(b"Saved ")
            send(b"\x11")
            end = time.monotonic() + 5
            exit_status = None
            while time.monotonic() < end:
                completed, status = os.waitpid(pid, os.WNOHANG)
                if completed == pid:
                    exit_status = status
                    break
                time.sleep(0.1)
            if exit_status is None or not os.WIFEXITED(exit_status) or                     os.WEXITSTATUS(exit_status) != 0:
                raise AssertionError("IDE did not exit cleanly")
            result = source.read_bytes()
            if not result.startswith(b"* harmless note\n? 2 + 3\n"):
                raise AssertionError("Ctrl+S did not save terminal editor contents: "
                                     + repr(result))
            print("terminal IDE pseudo-terminal check passed")
            return 0
        finally:
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                os.waitpid(pid, os.WNOHANG)
            except ChildProcessError:
                pass
            os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
