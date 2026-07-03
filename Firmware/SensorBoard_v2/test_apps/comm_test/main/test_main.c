/* Tests Unity de usb_comm — cada TEST_CASE mapea un Scenario del spec
 * openspec/changes/sb-phase1-usb-cdc/specs/usb-transport/spec.md */
#include "unity.h"
#include "sensorBoard_crc16.h"
#include "sensorBoard_frame.h"
#include "sensorBoard_comm_protocol.h"
#include <string.h>

/* ── CRC16-CCITT FALSE ─────────────────────────────────────── */

TEST_CASE("CRC16 known vector: '123456789' == 0x29B1", "[crc16]")
{
    const uint8_t data[] = "123456789";
    TEST_ASSERT_EQUAL_HEX16(0x29B1, sb_crc16(data, 9));
}

TEST_CASE("CRC16 empty data returns 0xFFFF", "[crc16]")
{
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, sb_crc16(NULL, 0));
}

TEST_CASE("CRC16 single byte 0x00", "[crc16]")
{
    /* CCITT FALSE (poly 0x1021, init 0xFFFF) de 0x00 = 0xE1F0, verificado a
     * mano y consistente con el vector "123456789"->0x29B1. El plan original
     * decía 0x84C0 (erróneo) — detectado en hardware real el 2026-07-03. */
    const uint8_t data[] = { 0x00 };
    TEST_ASSERT_EQUAL_HEX16(0xE1F0, sb_crc16(data, 1));
}

TEST_CASE("CRC16 incremental equals batch", "[crc16]")
{
    const uint8_t data[] = { 0x01, 0x02, 0x03, 0x04 };
    uint16_t batch = sb_crc16(data, 4);
    uint16_t inc = 0xFFFF;
    for (int i = 0; i < 4; i++) {
        inc = sb_crc16_byte(inc, data[i]);
    }
    TEST_ASSERT_EQUAL_HEX16(batch, inc);
}

/* ── Frame encoder ─────────────────────────────────────────── */

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
    sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)"A", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_MAGIC_0, out[0]);
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_MAGIC_1, out[1]);
}

TEST_CASE("frame_encode: type byte at position 2", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)"A", 1, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(SB_PROTO_TYPE_JSON, out[2]);
}

TEST_CASE("frame_encode: length LE at bytes 3-6", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)"ABCDE", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8(5, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0, out[5]);
    TEST_ASSERT_EQUAL_HEX8(0, out[6]);
}

TEST_CASE("frame_encode: payload copied at offset 7", "[frame]")
{
    uint8_t out[32];
    sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)"HI", 2, out, sizeof(out));
    TEST_ASSERT_EQUAL_HEX8('H', out[7]);
    TEST_ASSERT_EQUAL_HEX8('I', out[8]);
}

TEST_CASE("frame_encode: returns 0 when buffer too small", "[frame]")
{
    uint8_t out[5];
    size_t len = sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)"hello", 5, out, sizeof(out));
    TEST_ASSERT_EQUAL(0, len);
}

/* ── Frame decoder ─────────────────────────────────────────── */

static uint8_t s_cb_type;
static uint8_t s_cb_payload[64];
static size_t s_cb_len;
static int s_cb_calls;

static void test_frame_cb(uint8_t type, const uint8_t *payload, size_t len, void *ctx)
{
    (void)ctx;
    s_cb_type = type;
    s_cb_len = len < sizeof(s_cb_payload) ? len : sizeof(s_cb_payload);
    memcpy(s_cb_payload, payload, s_cb_len);
    s_cb_calls++;
}

static void feed_frame(sb_frame_dec_t *dec, const uint8_t *payload, size_t len, uint8_t type)
{
    uint8_t frame[SB_PROTO_MAX_JSON_FRAME];
    size_t flen = sb_frame_encode(type, payload, len, frame, sizeof(frame));
    for (size_t i = 0; i < flen; i++) {
        sb_frame_dec_feed(dec, frame[i], test_frame_cb, NULL);
    }
}

TEST_CASE("decoder: round-trip JSON payload", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    const uint8_t payload[] = "hello world";
    feed_frame(&dec, payload, 11, SB_PROTO_TYPE_JSON);

    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(SB_PROTO_TYPE_JSON, s_cb_type);
    TEST_ASSERT_EQUAL(11, s_cb_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, s_cb_payload, 11);
}

TEST_CASE("decoder: bad CRC discarded silently", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    uint8_t frame[32];
    size_t flen = sb_frame_encode(SB_PROTO_TYPE_JSON, (const uint8_t *)"X", 1, frame, sizeof(frame));
    TEST_ASSERT_GREATER_THAN(0, flen);
    frame[flen - 1] ^= 0xFF; /* corrupt CRC low byte */

    for (size_t i = 0; i < flen; i++) {
        sb_frame_dec_feed(&dec, frame[i], test_frame_cb, NULL);
    }
    TEST_ASSERT_EQUAL(0, s_cb_calls);
}

TEST_CASE("decoder: resync after garbage bytes", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    const uint8_t garbage[] = { 0x00, 0xFF, 0x12, 0x34 };
    for (int i = 0; i < 4; i++) {
        sb_frame_dec_feed(&dec, garbage[i], test_frame_cb, NULL);
    }
    feed_frame(&dec, (const uint8_t *)"OK", 2, SB_PROTO_TYPE_JSON);

    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(2, s_cb_len);
}

TEST_CASE("decoder: empty payload frame", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    feed_frame(&dec, NULL, 0, SB_PROTO_TYPE_JSON);
    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(0, s_cb_len);
}

TEST_CASE("decoder: oversize length discarded, then resyncs", "[decoder]")
{
    uint8_t dec_buf[16]; /* buffer pequeño a propósito */
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    /* Header con Length=1024 > 16: el decoder debe descartar sin escribir payload */
    const uint8_t oversized_header[] = { 0xAB, 0xCD, 0x00, 0x00, 0x04, 0x00, 0x00 };
    for (size_t i = 0; i < sizeof(oversized_header); i++) {
        sb_frame_dec_feed(&dec, oversized_header[i], test_frame_cb, NULL);
    }
    TEST_ASSERT_EQUAL(0, s_cb_calls);

    /* Tras el descarte, un frame válido pequeño debe decodificarse */
    feed_frame(&dec, (const uint8_t *)"ok", 2, SB_PROTO_TYPE_JSON);
    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(2, s_cb_len);
}

TEST_CASE("decoder: round-trip at exact SB_PROTO_MAX_JSON_PAYLOAD", "[decoder]")
{
    static uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    static uint8_t payload[SB_PROTO_MAX_JSON_PAYLOAD];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    static uint8_t frame[SB_PROTO_MAX_JSON_FRAME];
    size_t flen = sb_frame_encode(SB_PROTO_TYPE_JSON, payload, sizeof(payload), frame, sizeof(frame));
    TEST_ASSERT_EQUAL(SB_PROTO_MAX_JSON_PAYLOAD + SB_PROTO_FRAME_OVERHEAD, flen);
    for (size_t i = 0; i < flen; i++) {
        sb_frame_dec_feed(&dec, frame[i], test_frame_cb, NULL);
    }

    TEST_ASSERT_EQUAL(1, s_cb_calls);
}

TEST_CASE("decoder: resync on 0xAB 0xAB 0xCD pattern", "[decoder]")
{
    uint8_t dec_buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;

    /* Un 0xAB extra antes de un frame válido: el segundo 0xAB debe
     * reconocerse como posible inicio del frame real, sin perderlo */
    sb_frame_dec_feed(&dec, 0xAB, test_frame_cb, NULL);
    feed_frame(&dec, (const uint8_t *)"OK", 2, SB_PROTO_TYPE_JSON);

    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(2, s_cb_len);
}

/* ── Builders de respuesta (puros: verifican contenido exacto) ── */

#include "sensorBoard_cmd_builder.h"
#include "sensorBoard_status.h"

TEST_CASE("build_status: exact JSON content (no sensors registered)", "[cmd]")
{
    sb_status_reset();
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    size_t n = sb_cmd_build_status(buf, sizeof(buf), 7, 1234);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"resp\",\"cmd\":\"status\",\"id\":7,"
                             "\"status\":\"ok\",\"device\":\"SensorBoard\","
                             "\"fw\":\"1.0.0\",\"uptime\":1234}",
                             buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("build_error: exact JSON content for unknown cmd", "[cmd]")
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    size_t n = sb_cmd_build_error(buf, sizeof(buf), "foo", 2, "cmd not found", 55);
    TEST_ASSERT_EQUAL_STRING("{\"type\":\"resp\",\"cmd\":\"foo\",\"id\":2,"
                             "\"status\":\"error\",\"msg\":\"cmd not found\",\"ts\":55}",
                             buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);
}

TEST_CASE("build_error: escapes quotes and backslashes in cmd echo", "[cmd]")
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    size_t n = sb_cmd_build_error(buf, sizeof(buf), "a\"b\\c", 9, "cmd not found", 1);
    TEST_ASSERT_GREATER_THAN(0, n);
    /* El eco debe llegar escapado: a\"b\\c */
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"cmd\":\"a\\\"b\\\\c\""));
}

TEST_CASE("build_error: oversized cmd truncated, JSON stays closed", "[cmd]")
{
    char long_cmd[300];
    memset(long_cmd, 'A', sizeof(long_cmd) - 1);
    long_cmd[sizeof(long_cmd) - 1] = '\0';

    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    size_t n = sb_cmd_build_error(buf, sizeof(buf), long_cmd, 1, "cmd not found", 1);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL('}', buf[n - 1]); /* JSON completo, no truncado a mitad */
}

/* ── Registro de sensores en status (Fase 2) ───────────────── */

TEST_CASE("status: registered sensors appear in sensors{}", "[status]")
{
    sb_status_reset();
    TEST_ASSERT_EQUAL(ESP_OK, sensorBoard_status_set_sensor("sht0", true));
    TEST_ASSERT_EQUAL(ESP_OK, sensorBoard_status_set_sensor("als", false));

    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    size_t n = sb_cmd_build_status(buf, sizeof(buf), 1, 2);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"sensors\":{\"sht0\":true,\"als\":false}"));
    TEST_ASSERT_EQUAL('}', buf[n - 1]);
}

TEST_CASE("status: re-registering a name updates in place", "[status]")
{
    sb_status_reset();
    sensorBoard_status_set_sensor("sht0", true);
    sensorBoard_status_set_sensor("sht0", false);

    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    sb_cmd_build_status(buf, sizeof(buf), 1, 2);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"sht0\":false"));
    /* una sola aparición */
    char *first = strstr(buf, "sht0");
    TEST_ASSERT_NULL(strstr(first + 1, "sht0"));
}

TEST_CASE("status: table full rejects ninth sensor", "[status]")
{
    sb_status_reset();
    const char *names[] = { "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7" };
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, sensorBoard_status_set_sensor(names[i], true));
    }
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, sensorBoard_status_set_sensor("s8", true));
    sb_status_reset();
}

TEST_CASE("status: 8 max-length names overflow fails closed, not garbled", "[status]")
{
    /* Techo documentado: 8 nombres de 11 chars no caben en los 256B del
     * payload — build_status debe devolver 0 (fail-closed), nunca JSON roto */
    sb_status_reset();
    for (int i = 0; i < 8; i++) {
        char name[SB_STATUS_NAME_MAX];
        snprintf(name, sizeof(name), "sensor_%04d", i);
        TEST_ASSERT_EQUAL(ESP_OK, sensorBoard_status_set_sensor(name, true));
    }
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    size_t n = sb_cmd_build_status(buf, sizeof(buf), 1, 2);
    TEST_ASSERT_EQUAL(0, n);
    sb_status_reset();
}

TEST_CASE("status: invalid names rejected", "[status]")
{
    sb_status_reset();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sensorBoard_status_set_sensor(NULL, true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sensorBoard_status_set_sensor("", true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      sensorBoard_status_set_sensor("nombre-demasiado-largo", true));
}

/* ── Registro de comandos (Fase 5) ─────────────────────────── */

#include "sensorBoard_cmd_registry.h"

static uint32_t s_reg_last_id;
static int s_reg_calls;
static void reg_test_handler(uint32_t id)
{
    s_reg_last_id = id;
    s_reg_calls++;
}
static void reg_test_handler2(uint32_t id)
{
    (void)id;
    s_reg_calls += 100;
}

TEST_CASE("cmd_register: registered handler is found and callable", "[cmdreg]")
{
    sb_cmd_registry_reset();
    TEST_ASSERT_EQUAL(ESP_OK, sensorBoard_cmd_register("capture", reg_test_handler));

    sb_cmd_handler_t h = sb_cmd_registry_find("capture");
    TEST_ASSERT_NOT_NULL(h);
    s_reg_calls = 0;
    h(7);
    TEST_ASSERT_EQUAL(1, s_reg_calls);
    TEST_ASSERT_EQUAL(7, s_reg_last_id);
}

TEST_CASE("cmd_register: unknown cmd finds NULL", "[cmdreg]")
{
    sb_cmd_registry_reset();
    TEST_ASSERT_NULL(sb_cmd_registry_find("nope"));
}

TEST_CASE("cmd_register: re-register replaces handler", "[cmdreg]")
{
    sb_cmd_registry_reset();
    sensorBoard_cmd_register("capture", reg_test_handler);
    sensorBoard_cmd_register("capture", reg_test_handler2);
    s_reg_calls = 0;
    sb_cmd_registry_find("capture")(1);
    TEST_ASSERT_EQUAL(100, s_reg_calls);
}

TEST_CASE("cmd_register: table full and invalid args rejected", "[cmdreg]")
{
    sb_cmd_registry_reset();
    /* Deriva del límite real para que un cambio de SB_CMD_REG_MAX no deje
     * este test midiendo otra cosa */
    for (unsigned i = 0; i < SB_CMD_REG_MAX; i++) {
        char name[SB_CMD_REG_NAME_MAX];
        snprintf(name, sizeof(name), "c%u", i);
        TEST_ASSERT_EQUAL(ESP_OK, sensorBoard_cmd_register(name, reg_test_handler));
    }
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, sensorBoard_cmd_register("extra", reg_test_handler));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sensorBoard_cmd_register(NULL, reg_test_handler));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sensorBoard_cmd_register("x", NULL));
    sb_cmd_registry_reset();
}

/* ── API pública (sin init: no requieren host USB) ─────────── */

#include "sensorBoard_comm.h"

TEST_CASE("send_binary: invalid args rejected before any allocation", "[comm]")
{
    uint8_t small[4] = { 0 };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sensorBoard_comm_send_binary(0x01, NULL, 4));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sensorBoard_comm_send_binary(0x01, small, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      sensorBoard_comm_send_binary(0x01, small, SB_PROTO_MAX_BINARY_PAYLOAD + 1));
}

TEST_CASE("send_binary: without init returns ESP_ERR_INVALID_STATE", "[comm]")
{
    uint8_t buf[4] = { 0 };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, sensorBoard_comm_send_binary(0x01, buf, sizeof(buf)));
}

TEST_CASE("frame: binary TYPE=0x01 round-trip with 4KB payload", "[frame]")
{
    static uint8_t payload[4096];
    static uint8_t frame[4096 + SB_PROTO_FRAME_OVERHEAD];
    static uint8_t dec_buf[4096];
    for (size_t i = 0; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i * 7);
    }

    size_t flen = sb_frame_encode(SB_PROTO_TYPE_JPEG, payload, sizeof(payload), frame, sizeof(frame));
    TEST_ASSERT_EQUAL(sizeof(payload) + SB_PROTO_FRAME_OVERHEAD, flen);

    sb_frame_dec_t dec;
    sb_frame_dec_init(&dec, dec_buf, sizeof(dec_buf));
    s_cb_calls = 0;
    for (size_t i = 0; i < flen; i++) {
        sb_frame_dec_feed(&dec, frame[i], test_frame_cb, NULL);
    }
    TEST_ASSERT_EQUAL(1, s_cb_calls);
    TEST_ASSERT_EQUAL(SB_PROTO_TYPE_JPEG, s_cb_type);
}

TEST_CASE("send_json without init returns ESP_ERR_INVALID_STATE", "[comm]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, sensorBoard_comm_send_json("{}"));
}

TEST_CASE("cmd_handle: malformed JSON does not crash", "[cmd]")
{
    const uint8_t garbage[] = "{not json!!";
    sensorBoard_cmd_handle(garbage, sizeof(garbage) - 1);
    /* Sin crash == pasa; la respuesta (si la hubiera) exige host USB */
    TEST_PASS();
}

TEST_CASE("cmd_handle: NULL/oversize payload ignored", "[cmd]")
{
    sensorBoard_cmd_handle(NULL, 10);
    static const uint8_t big[SB_PROTO_MAX_JSON_PAYLOAD + 1] = { '{' };
    sensorBoard_cmd_handle(big, sizeof(big));
    TEST_PASS();
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
