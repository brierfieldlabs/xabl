#!/usr/bin/env python3
"""Generate dBASE III DBF and NDX fixtures without third-party dependencies.

The DBF contains NAME, BALANCE, and STATUS fields. The NDX indexes NAME and is
small enough to fit in a single leaf/root page, while still using the genuine
dBASE III 512-byte NDX page structure.
"""

from __future__ import annotations

from datetime import date
from pathlib import Path
import struct

HERE = Path(__file__).parent
DBF_OUT = HERE / "customers.dbf"
NDX_OUT = HERE / "customers.ndx"

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


def write_dbf() -> None:
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

    with DBF_OUT.open("wb") as handle:
        handle.write(header)
        for field in FIELDS:
            handle.write(field_descriptor(*field))
        handle.write(b"\r")
        for row in ROWS:
            handle.write(encode_row(row))
        handle.write(b"\x1a")


def write_ndx() -> None:
    key_length = 20
    key_record_length = 28  # lower-page ptr + record no + 20-byte key
    max_keys = 18

    header = bytearray(512)
    struct.pack_into("<I", header, 0, 1)  # root page
    struct.pack_into("<I", header, 4, 2)  # next/EOF page
    struct.pack_into("<H", header, 12, key_length)
    struct.pack_into("<H", header, 14, max_keys)
    struct.pack_into("<H", header, 16, 0)  # character key
    struct.pack_into("<I", header, 18, key_record_length)
    header[23] = 0  # non-unique
    header[24:29] = b"NAME\x00"

    page = bytearray(512)
    struct.pack_into("<I", page, 0, len(ROWS))

    for index, (name, _balance, _status) in enumerate(ROWS):
        offset = 4 + index * key_record_length
        struct.pack_into("<I", page, offset, 0)  # leaf: no lower page
        struct.pack_into("<I", page, offset + 4, index + 1)
        page[offset + 8 : offset + 28] = name.encode("ascii").ljust(20, b" ")

    tail = 4 + len(ROWS) * key_record_length
    struct.pack_into("<I", page, tail, 0)

    with NDX_OUT.open("wb") as handle:
        handle.write(header)
        handle.write(page)


def main() -> None:
    write_dbf()
    write_ndx()


if __name__ == "__main__":
    main()