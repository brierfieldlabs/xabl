#!/usr/bin/env python3
"""Generate a tiny dBASE III DBF fixture without third-party dependencies.

The resulting table contains NAME, BALANCE, and STATUS fields and is used by
XABL's first compatibility smoke test.
"""

from __future__ import annotations

from datetime import date
from pathlib import Path
import struct

OUT = Path(__file__).with_name("customers.dbf")

FIELDS = [
    ("NAME", "C", 20, 0),
    ("BALANCE", "N", 10, 2),
    ("STATUS", "C", 10, 0),
]

ROWS = [
    ("Alice", 125.50, ""),
    ("Bob", 80.00, ""),
    ("Charlie", 200.00, ""),
]


def field_descriptor(name: str, kind: str, length: int, decimals: int) -> bytes:
    raw = bytearray(32)
    encoded = name.encode("ascii")[:11]
    raw[: len(encoded)] = encoded
    raw[11] = ord(kind)
    raw[16] = length
    raw[17] = decimals
    return bytes(raw)


def encode_row(row: tuple[str, float, str]) -> bytes:
    name, balance, status = row
    payload = bytearray(b" ")
    payload += name.encode("ascii").ljust(20, b" ")[:20]
    payload += f"{balance:10.2f}".encode("ascii")
    payload += status.encode("ascii").ljust(10, b" ")[:10]
    return bytes(payload)


def main() -> None:
    today = date.today()
    record_length = 1 + sum(field[2] for field in FIELDS)
    header_length = 32 + (32 * len(FIELDS)) + 1

    header = bytearray(32)
    header[0] = 0x03
    header[1] = today.year - 1900
    header[2] = today.month
    header[3] = today.day
    struct.pack_into("<I", header, 4, len(ROWS))
    struct.pack_into("<H", header, 8, header_length)
    struct.pack_into("<H", header, 10, record_length)

    with OUT.open("wb") as handle:
        handle.write(header)
        for field in FIELDS:
            handle.write(field_descriptor(*field))
        handle.write(b"\r")
        for row in ROWS:
            handle.write(encode_row(row))
        handle.write(b"\x1a")


if __name__ == "__main__":
    main()