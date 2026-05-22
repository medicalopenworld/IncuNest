"""Minimal ESP32 NVS partition image generator.

Produces a single 4 KiB NVS page containing:
  namespace : mb_cfg
  key       : serial  (uint16, range 0-9999)

CRC algorithm: standard CRC32 (same as zlib.crc32), which matches
the ESP-IDF crc32_le(0xFFFFFFFF, data) followed by bitwise NOT.
"""
import struct
import zlib
from pathlib import Path
from typing import Optional

_PAGE_SIZE   = 4096
_HDR_SIZE    = 32
_BITMAP_SIZE = 32
_ENTRY_SIZE  = 32

_PAGE_ACTIVE = 0xFFFFFFFE
_NVS_VER2    = 0xFE

_T_U8  = 0x01
_T_U16 = 0x02

_DEFAULT_NVS_OFFSET = 0x9000


def _crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def _make_entry(ns: int, typ: int, key: str, value: bytes) -> bytes:
    e = bytearray(_ENTRY_SIZE)
    e[0] = ns  & 0xFF
    e[1] = typ & 0xFF
    e[2] = 1        # span (number of 32-byte slots used)
    e[3] = 0xFF     # chunk_index: 0xFF = primitive / short value
    # [4:8] = CRC32, computed below
    e[8:24] = key.encode('ascii')[:16].ljust(16, b'\x00')
    e[24:32] = (value + b'\xFF' * 8)[:8]
    struct.pack_into('<I', e, 4, _crc32(bytes(e[0:4]) + bytes(e[8:32])))
    return bytes(e)


def find_nvs_offset(partitions_bin: Path) -> int:
    """Parse a compiled partition table binary and return the NVS partition offset.
    Falls back to 0x9000 (ESP32 default) if the file cannot be read or parsed.
    """
    try:
        data = partitions_bin.read_bytes()
    except OSError:
        return _DEFAULT_NVS_OFFSET
    for i in range(0, len(data), 32):
        row = data[i:i + 32]
        if len(row) < 32 or row[0] == 0xFF:
            break
        # magic=0xAA50, type=0x01 (data), subtype=0x02 (nvs)
        if row[0] == 0xAA and row[1] == 0x50 and row[2] == 0x01 and row[3] == 0x02:
            return struct.unpack_from('<I', row, 4)[0]
    return _DEFAULT_NVS_OFFSET


def generate_serial_nvs(serial_number: int) -> bytes:
    """Return a 4096-byte NVS partition image with mb_cfg::serial = serial_number."""
    if not (0 <= serial_number <= 9999):
        raise ValueError(f"serial_number must be 0-9999, got {serial_number}")

    # Entry 0 — namespace declaration: 'mb_cfg' is assigned index 1
    e0 = _make_entry(0, _T_U8,  'mb_cfg', bytes([1]))
    # Entry 1 — data: ns=1, type=uint16, key='serial'
    e1 = _make_entry(1, _T_U16, 'serial', struct.pack('<H', serial_number))

    # Entry-state bitmap (32 bytes = 128 entries × 2 bits, packed LSB-first per uint32).
    # Written = 0b10, Empty = 0b11.
    # Word 0 encodes entries 0-15; entries 0 and 1 written, rest empty:
    #   bits[1:0]=entry0=10, bits[3:2]=entry1=10, bits[31:4]=all 1s → 0xFFFFFFFA
    bitmap = struct.pack('<I', 0xFFFFFFFA) + b'\xFF' * 28

    # Page header: state(4) + seqnum(4) + version(1) + reserved(19) + crc32(4)
    hdr_payload = struct.pack('<IIB', _PAGE_ACTIVE, 0, _NVS_VER2)
    header = hdr_payload + b'\xFF' * 19 + struct.pack('<I', _crc32(hdr_payload))

    page = bytearray(header) + bytearray(bitmap) + bytearray(e0) + bytearray(e1)
    page += b'\xFF' * (_PAGE_SIZE - len(page))
    assert len(page) == _PAGE_SIZE
    return bytes(page)
