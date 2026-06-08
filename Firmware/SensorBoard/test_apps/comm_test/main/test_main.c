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

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
