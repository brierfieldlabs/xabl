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
        faulty = Path(directory) / "bad.prg"
        pid, fd = pty.fork()
        if pid == 0:
            os.environ["TERM"] = "xterm-256color"
            os.environ["LC_ALL"] = "C"
            os.execv(sys.argv[1], [sys.argv[1], str(source)])
        transcript = bytearray()

        def wait_for(token: bytes, timeout: float = 5.0,
                     since: int | None = None) -> None:
            # One PTY read can contain several screen updates. Use the
            # operation's launch marker to match text in the complete
            # post-action window, even when earlier waits consumed it.
            start = len(transcript) if since is None else since
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                if token in transcript[start:]:
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
            # The terminal may draw the title and footer in one read.
            # Search both within the full initial-screen window, rather
            # than starting the footer search after title detection.
            startup_mark = len(transcript)
            wait_for(b"XABL Text Studio", since=startup_mark)
            wait_for(b"F1 Help", since=startup_mark)
            browser_mark = len(transcript)
            send(b"\x1bOR")  # F3: Browse/Open
            wait_for(b"FILE BROWSER", since=browser_mark)
            wait_for(b"program.prg", since=browser_mark)
            send(b"\x1bOB")  # xterm application-cursor Down in curses mode
            send(b"\r")      # open highlighted source file
            wait_for(b"Opened ")
            # The completed editor redraw (including EDITOR) may arrive
            # in the same pty read as the 'Opened' status text.
            goto_mark = len(transcript)
            send(b"\x07")  # Ctrl+G: go to a source line
            wait_for(b"Go to line", since=goto_mark)
            send(b"2\r")
            wait_for(b"Line 2")
            goto_mark = len(transcript)
            send(b"\x07")
            wait_for(b"Go to line", since=goto_mark)
            send(b"1\r")
            wait_for(b"Line 1")
            # F9: Build/check only. Program output should say "Compilation"
            # without executing the program or showing its result.
            send(b"\x1b[20~")
            wait_for(b"Compilation successful.")
            send(b"\x1b[17~")  # F6: editor
            wait_for(b"EDITOR")
            send(b"\x1b[20;5~")  # Ctrl+F9: Run, explicit confirmation
            wait_for(b"Run source?")
            send(b"y")
            wait_for(b"Program completed.")
            send(b"\x1b[17~")  # F6: editor
            wait_for(b"EDITOR")
            zoom_mark = len(transcript)
            send(b"\x1b[15~")  # F5: zoom editor
            wait_for(b"Editor zoomed.", since=zoom_mark)
            unzoom_mark = len(transcript)
            send(b"\x1b[15~")  # F5: unzoom
            wait_for(b"restored.", since=unzoom_mark)
            # Save harmless edit via F2, then exit via Alt+X.
            send(b"* harmless note\n")
            send(b"\x1bOQ")  # F2 Save
            wait_for(b"Saved ")
            # Exercise the debugger in the existing terminal PTY suite.
            debug_source = Path(directory) / "debug.prg"
            debug_source.write_bytes(b"STORE 0 TO n\nn = n + 1\n? n\n")
            debug_open = len(transcript)
            send(b"\x0f")  # Ctrl+O
            wait_for(b"Open source file", since=debug_open)
            send(os.fsencode(debug_source) + b"\r")
            wait_for(b"Opened ")
            jump_mark = len(transcript)
            send(b"\x07")  # Ctrl+G
            wait_for(b"Go to line", since=jump_mark)
            send(b"2\r")
            wait_for(b"Line 2", since=jump_mark)
            bp_mark = len(transcript)
            send(b"\x1b[19;5~")  # Ctrl+F8 breakpoint
            wait_for(b"Breakpoint set on line 2", since=bp_mark)
            debug_mark = len(transcript)
            send(b"\x1b[20;5~")  # Ctrl+F9 debug/run
            wait_for(b"Start debugging?", since=debug_mark)
            send(b"y")
            wait_for(b"Breakpoint before line 2", since=debug_mark)
            vars_mark = len(transcript)
            send(b"\x1b[18;5~")  # Ctrl+F7 read-only variables
            wait_for(b"DEBUG VARIABLES", since=vars_mark)
            wait_for(b"N = 0", since=vars_mark)
            send(b"\x1b[18;5~")  # return to source editor
            locked_mark = len(transcript)
            send(b"x")  # Editing paused source must be refused.
            wait_for(b"Editor paused", since=locked_mark)
            step_mark = len(transcript)
            send(b"\x1b[19~")  # F8 statement step
            wait_for(b"Stepped to line 3", since=step_mark)
            vars_mark = len(transcript)
            send(b"\x1b[18;5~")
            wait_for(b"N = 1", since=vars_mark)
            send(b"\x1b[18;5~")
            done_mark = len(transcript)
            send(b"\x1b[20;5~")  # Ctrl+F9: continue
            wait_for(b"Program completed.", since=done_mark)
            if debug_source.read_bytes() != b"STORE 0 TO n\nn = n + 1\n? n\n":
                raise AssertionError("Debugging changed the source file")
            send(b"\x1b[17~")  # F6: editor
            wait_for(b"EDITOR")
            # Revisit breakpoint, then cancel without changing the file.
            restart_mark = len(transcript)
            send(b"\x1b[20;5~")
            wait_for(b"Start debugging?", since=restart_mark)
            send(b"y")
            wait_for(b"Breakpoint before line 2", since=restart_mark)
            stop_mark = len(transcript)
            send(b"\x1b[1;5Q")  # Ctrl+F2: reset debugger
            wait_for(b"Debugger stopped.", since=stop_mark)
            undo_mark = len(transcript)
            send(b"\x1a")  # Ctrl+Z now works, not locked by debugger
            wait_for(b"Nothing to undo", since=undo_mark)
            # F10 menu must actually operate the debugger, not merely draw.
            menu_mark = len(transcript)
            send(b"\x1b[21~")  # F10
            wait_for(b"XABL COMMAND MENU", since=menu_mark)
            remove_mark = len(transcript)
            send(b"b")  # Toggle existing breakpoint via menu
            wait_for(b"Breakpoint removed from line 2", since=remove_mark)
            # F4 runs to the cursor without creating a permanent breakpoint.
            line_mark = len(transcript)
            send(b"\x07")
            wait_for(b"Go to line", since=line_mark)
            send(b"3\r")
            wait_for(b"Line 3", since=line_mark)
            cursor_run_mark = len(transcript)
            send(b"\x1bOS")  # F4 in xterm-256color
            wait_for(b"Start debugging?", since=cursor_run_mark)
            send(b"y")
            wait_for(b"Breakpoint before line 3", since=cursor_run_mark)
            stop_mark = len(transcript)
            send(b"\x1b[1;5Q")
            wait_for(b"Debugger stopped.", since=stop_mark)

            faulty.write_bytes(b"? 2 + 3\nUNKNOWN COMMAND\n")
            # F9 checks only. Ctrl+E handles compiler error navigation
            # because F8 is reserved for debugging step.
            path_mark = len(transcript)
            send(b"\x0f")  # Ctrl+O: direct open
            wait_for(b"Open source file", since=path_mark)
            send(os.fsencode(faulty) + b"\r")
            wait_for(b"Opened ")
            check_mark = len(transcript)
            send(b"\x1b[20~")
            wait_for(b"Error: line 2:", since=check_mark)
            error_mark = len(transcript)
            send(b"\x05")  # Ctrl+E: compiler error jump
            wait_for(b"Compiler error at line 2", since=error_mark)
            menu_mark = len(transcript)
            send(b"\x1b[21~")  # F10: command menu fallback
            wait_for(b"XABL COMMAND MENU", since=menu_mark)
            send(b"q")  # close menu
            send(b"\x1bx")  # Alt+X: exit
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
                raise AssertionError("F2 did not save terminal editor contents: "
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
