#include "unity.h"
#include "sensorBoard_crc16.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_comm_protocol.h"
#include <string.h>

/* ── CRC16-CCITT tests ─────────────────────────────────────── */

TEST_CASE("CRC16 known vector: '123456789' == 0x29B1", "[crc16]")
{
    const uint8_t data[] = "123456789";
    uint16_t crc = sb_crc16(data, 9);
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc);
}

TEST_CASE("CRC16 empty data returns 0xFFFF", "[crc16]")
{
    uint16_t crc = sb_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

TEST_CASE("CRC16 single byte 0x00", "[crc16]")
{
    const uint8_t data[] = {0x00};
    uint16_t crc = sb_crc16(data, 1);
    /* poly=0x1021, init=0xFFFF: result = 0x84C0 */
    TEST_ASSERT_EQUAL_HEX16(0x84C0, crc);
}

TEST_CASE("CRC16 incremental equals batch", "[crc16]")
{
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t batch = sb_crc16(data, 4);

    uint16_t inc = 0xFFFF;
    for (int i = 0; i < 4; i++) {
        inc = sb_crc16_byte(inc, data[i]);
    }
    TEST_ASSERT_EQUAL_HEX16(batch, inc);
}

/* ── Frame encoder tests ───────────────────────────────────── */

TEST_CASE("frame_encode: output size = payload + overhead", "[frame]")
{
    const uint8_t payload[] = "hello";
    uint8_t out[SB_PROTO_MAX_JSON_FRAME];
    size_t len = sb_frame_encode(SB_PROTO_TYPE_JSON, payload, 5, out, sizeof(out));
    TEST_ASSERT_EQUAL(5 + SB_PROTO_FRAME_OVERHEAD, len);
}

TEST_CASE("frame_encode: magic bytes correct", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"A", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_MAGIC_0, out[0]);
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_MAGIC_1, out[1]);
}

TEST_CASE("frame_encode: type byte at position 2", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"A", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_TYPE_JSON, out[2]);
}

TEST_CASE("frame_encode: length LE at bytes 3-6", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"ABCDE", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(5, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0, out[5]);
    TEST_ASSERT_EQUAL_HEX8(0, out[6]);
}

TEST_CASE("frame_encode: payload copied at offset 7", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"HI", 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8('H', out[7]);
    TEST_ASSERT_EQUAL_HEX8('I', out[8]);
}

TEST_CASE("frame_encode: returns 0 when buffer too small", "[frame]")
{
    uint8_t out[5];
    size_t len = sb_frame_encode(SB_PROTO_TYPE_JSON, (uint8_t *)"hello", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL(0, len);
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
