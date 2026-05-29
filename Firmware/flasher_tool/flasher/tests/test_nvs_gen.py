import struct
import zlib
import pytest

from nvs_gen import generate_serial_nvs, _crc32

_PAGE_SIZE   = 4096
_HDR_SIZE    = 32
_BITMAP_SIZE = 32
_ENTRY_SIZE  = 32
_ENTRIES_OFFSET = _HDR_SIZE + _BITMAP_SIZE  # 64


def _esp_crc32(data: bytes) -> int:
    """ESP-IDF crc32_le(0xFFFFFFFF, data) — table loop, no final XOR."""
    return (~zlib.crc32(data)) & 0xFFFFFFFF


# ------------------------------------------------------------------ #
# _crc32 helper
# ------------------------------------------------------------------ #

def test_crc32_matches_esp_idf_no_final_xor():
    # ESP-IDF omits the final XOR that zlib applies; verify our helper matches.
    data = b'\x00\x01\x02\x03'
    assert _crc32(data) == _esp_crc32(data)


def test_crc32_differs_from_plain_zlib():
    data = b'\xDE\xAD\xBE\xEF'
    assert _crc32(data) != (zlib.crc32(data) & 0xFFFFFFFF)


# ------------------------------------------------------------------ #
# Page structure
# ------------------------------------------------------------------ #

_PARTITION_SIZE = 0x6000  # 24 KB default


@pytest.fixture
def page():
    return generate_serial_nvs(42, _PARTITION_SIZE)


def test_image_is_full_partition_size(page):
    assert len(page) == _PARTITION_SIZE


def test_remainder_is_erased_flash(page):
    # Bytes beyond the first page must be 0xFF so old NVS pages are overwritten.
    assert page[_PAGE_SIZE:] == b'\xFF' * (_PARTITION_SIZE - _PAGE_SIZE)


def test_page_state_is_active(page):
    state, = struct.unpack_from('<I', page, 0)
    assert state == 0xFFFFFFFE


def test_page_header_crc_is_valid(page):
    # ESP-IDF computes CRC over bytes [4:28]: seq_no + version + reserved.
    crc_data = page[4:28]
    expected = _esp_crc32(crc_data)
    actual, = struct.unpack_from('<I', page, 28)
    assert actual == expected, f"header CRC {actual:#010x} != expected {expected:#010x}"


def test_page_version_is_v2(page):
    assert page[8] == 0xFE


# ------------------------------------------------------------------ #
# Entry 0 — namespace 'mb_cfg'
# ------------------------------------------------------------------ #

def test_namespace_entry_fields(page):
    e = page[_ENTRIES_OFFSET: _ENTRIES_OFFSET + _ENTRY_SIZE]
    assert e[0] == 0x00   # ns_index = 0 (root)
    assert e[1] == 0x01   # type = U8
    assert e[2] == 0x01   # span = 1
    assert e[3] == 0xFF   # chunk_index = ANY
    assert e[8:14] == b'mb_cfg'
    assert e[24] == 0x01  # namespace assigned index = 1


def test_namespace_entry_crc_is_valid(page):
    e = page[_ENTRIES_OFFSET: _ENTRIES_OFFSET + _ENTRY_SIZE]
    expected = _esp_crc32(e[0:4] + e[8:32])
    actual, = struct.unpack_from('<I', e, 4)
    assert actual == expected, f"ns entry CRC {actual:#010x} != expected {expected:#010x}"


# ------------------------------------------------------------------ #
# Entry 1 — int32 'serial'  (matches Preferences.putInt / nvs_get_i32)
# ------------------------------------------------------------------ #

def test_data_entry_fields(page):
    offset = _ENTRIES_OFFSET + _ENTRY_SIZE
    e = page[offset: offset + _ENTRY_SIZE]
    assert e[0] == 0x01   # ns_index = 1 (mb_cfg)
    assert e[1] == 0x14   # type = I32 (matches getInt / nvs_get_i32)
    assert e[2] == 0x01   # span = 1
    assert e[3] == 0xFF   # chunk_index = ANY
    assert e[8:14] == b'serial'
    value, = struct.unpack_from('<i', e, 24)
    assert value == 42


def test_data_entry_crc_is_valid(page):
    offset = _ENTRIES_OFFSET + _ENTRY_SIZE
    e = page[offset: offset + _ENTRY_SIZE]
    expected = _esp_crc32(e[0:4] + e[8:32])
    actual, = struct.unpack_from('<I', e, 4)
    assert actual == expected, f"data entry CRC {actual:#010x} != expected {expected:#010x}"


# ------------------------------------------------------------------ #
# Bitmap
# ------------------------------------------------------------------ #

def test_bitmap_marks_first_two_entries_written(page):
    # Entries 0 and 1 → Written (0b10 each), rest → Empty (0b11).
    # Word 0 LSB-first: bits[1:0]=10, bits[3:2]=10, rest=1 → 0xFFFFFFFA
    word0, = struct.unpack_from('<I', page, _HDR_SIZE)
    assert word0 == 0xFFFFFFFA
    # Remaining bitmap words all empty
    for i in range(1, _BITMAP_SIZE // 4):
        w, = struct.unpack_from('<I', page, _HDR_SIZE + i * 4)
        assert w == 0xFFFFFFFF


# ------------------------------------------------------------------ #
# Boundary values
# ------------------------------------------------------------------ #

@pytest.mark.parametrize('serial', [0, 1, 9999])
def test_serial_round_trips(serial):
    page = generate_serial_nvs(serial, _PARTITION_SIZE)
    offset = _ENTRIES_OFFSET + _ENTRY_SIZE
    e = page[offset: offset + _ENTRY_SIZE]
    value, = struct.unpack_from('<i', e, 24)
    assert value == serial


def test_invalid_serial_raises():
    with pytest.raises(ValueError):
        generate_serial_nvs(10000, _PARTITION_SIZE)
    with pytest.raises(ValueError):
        generate_serial_nvs(-1, _PARTITION_SIZE)


def test_invalid_partition_size_raises():
    with pytest.raises(ValueError):
        generate_serial_nvs(1, 1000)  # not a multiple of 4096
