"""Minimal ESP32 NVS partition image generator.

Produces a single 4 KiB NVS page containing:
  namespace : mb_cfg
  key       : serial  (uint16, range 0-9999)

CRC algorithm: ESP-IDF stores crc32_le(0xFFFFFFFF, data) which is the
standard CRC32 table loop WITHOUT the final XOR inversion. zlib.crc32
DOES apply that inversion, so we must complement it:
  esp_idf_crc = (~zlib.crc32(data)) & 0xFFFFFFFF
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
_T_I32 = 0x14  # matches Preferences.putInt / nvs_set_i32

_DEFAULT_NVS_OFFSET = 0x9000


def _crc32(data: bytes) -> int:
    # Matches ESP-IDF crc32_le(0xFFFFFFFF, data) and the official nvs_partition_gen.py:
    # zlib.crc32(data, 0xFFFFFFFF) uses init=0xFFFFFFFF which internally starts table
    # loop from 0 (0xFFFF^0xFFFF), processes data, then XORs result with 0xFFFFFFFF.
    return zlib.crc32(data, 0xFFFFFFFF) & 0xFFFFFFFF


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


_DEFAULT_NVS_SIZE = 0x6000  # 24 KB — typical ESP32 NVS partition


def find_nvs_partition(partitions_bin: Path) -> tuple[int, int]:
    """Return (offset, size) of the NVS partition.
    Falls back to (0x9000, 0x6000) if the file cannot be read or parsed.
    """
    try:
        data = partitions_bin.read_bytes()
    except OSError:
        return _DEFAULT_NVS_OFFSET, _DEFAULT_NVS_SIZE
    for i in range(0, len(data), 32):
        row = data[i:i + 32]
        if len(row) < 32 or row[0] == 0xFF:
            break
        # magic=0xAA50, type=0x01 (data), subtype=0x02 (nvs)
        if row[0] == 0xAA and row[1] == 0x50 and row[2] == 0x01 and row[3] == 0x02:
            offset = struct.unpack_from('<I', row, 4)[0]
            size   = struct.unpack_from('<I', row, 8)[0]
            return offset, size
    return _DEFAULT_NVS_OFFSET, _DEFAULT_NVS_SIZE


def find_nvs_offset(partitions_bin: Path) -> int:
    """Kept for backwards compatibility."""
    return find_nvs_partition(partitions_bin)[0]


def generate_serial_nvs(serial_number: int, partition_size: int = _DEFAULT_NVS_SIZE) -> bytes:
    """Return an NVS partition image of partition_size bytes with mb_cfg::serial = serial_number.

    Writing the full partition size (not just the first 4 KB page) ensures any
    pre-existing NVS pages with higher seq_no values are overwritten with 0xFF
    (erased flash), so ESP-IDF picks our page as the only valid one.
    """
    if not (0 <= serial_number <= 9999):
        raise ValueError(f"serial_number must be 0-9999, got {serial_number}")
    if partition_size < _PAGE_SIZE or partition_size % _PAGE_SIZE != 0:
        raise ValueError(f"partition_size must be a multiple of {_PAGE_SIZE}, got {partition_size}")

    # Entry 0 — namespace declaration: 'mb_cfg' is assigned index 1
    e0 = _make_entry(0, _T_U8,  'mb_cfg', bytes([1]))
    # Entry 1 — data: ns=1, type=int32, key='serial' (matches Preferences.getInt/nvs_get_i32)
    e1 = _make_entry(1, _T_I32, 'serial', struct.pack('<i', serial_number))

    # Entry-state bitmap (32 bytes = 128 entries × 2 bits, packed LSB-first per uint32).
    # Written = 0b10, Empty = 0b11.
    # Word 0 encodes entries 0-15; entries 0 and 1 written, rest empty:
    #   bits[1:0]=entry0=10, bits[3:2]=entry1=10, bits[31:4]=all 1s → 0xFFFFFFFA
    bitmap = struct.pack('<I', 0xFFFFFFFA) + b'\xFF' * 28

    # Page header: state(4) + seqnum(4) + version(1) + reserved(19) + crc32(4)
    # ESP-IDF calculates header CRC over bytes [4:28]: seq_no + version + reserved.
    # The state field (bytes [0:4]) is intentionally excluded from the CRC.
    seq_ver_reserved = struct.pack('<IB', 0, _NVS_VER2) + b'\xFF' * 19  # 24 bytes
    header = struct.pack('<I', _PAGE_ACTIVE) + seq_ver_reserved + struct.pack('<I', _crc32(seq_ver_reserved))

    page = bytearray(header) + bytearray(bitmap) + bytearray(e0) + bytearray(e1)
    page += b'\xFF' * (_PAGE_SIZE - len(page))
    assert len(page) == _PAGE_SIZE

    # Pad to full partition size with 0xFF (erased flash). This overwrites any
    # pre-existing NVS pages with higher seq_no that ESP-IDF would otherwise prefer.
    return bytes(page) + b'\xFF' * (partition_size - _PAGE_SIZE)
