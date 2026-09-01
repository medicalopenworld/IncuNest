#include <unity.h>
#include <string.h>
#include <vector>

#include "modules/sensorboard_comm/sb_crc16.h"
#include "modules/sensorboard_comm/sb_frame_parser.h"
#include "modules/sensorboard_comm/sb_protocol.h"

void setUp(void) {}
void tearDown(void) {}

struct Capture {
  int complete_count = 0;
  int error_count = 0;
  uint8_t last_type = 0;
  uint32_t last_len = 0;
  uint8_t last_payload[512];
};

static void on_complete_cb(uint8_t type, const uint8_t *payload, uint32_t len,
                           void *ctx) {
  Capture *c = (Capture *)ctx;
  c->complete_count++;
  c->last_type = type;
  c->last_len = len;
  if (len > 0 && len <= sizeof(c->last_payload)) {
    memcpy(c->last_payload, payload, len);
  }
}

static void on_error_cb(void *ctx) {
  Capture *c = (Capture *)ctx;
  c->error_count++;
}

// Construye un frame valido completo: Magic+Type+Length(LE4)+Payload+CRC(BE2).
static std::vector<uint8_t> build_frame(uint8_t type,
                                        const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> frame;
  frame.push_back(SB_PROTO_MAGIC_0);
  frame.push_back(SB_PROTO_MAGIC_1);
  frame.push_back(type);
  uint32_t len = (uint32_t)payload.size();
  frame.push_back((uint8_t)(len & 0xFF));
  frame.push_back((uint8_t)((len >> 8) & 0xFF));
  frame.push_back((uint8_t)((len >> 16) & 0xFF));
  frame.push_back((uint8_t)((len >> 24) & 0xFF));
  for (uint8_t b : payload) frame.push_back(b);

  // CRC sobre Type+Length+Payload (todo menos el magic).
  std::vector<uint8_t> crc_input(frame.begin() + 2, frame.end());
  uint16_t crc = sb_crc16(crc_input.data(), crc_input.size());
  frame.push_back((uint8_t)((crc >> 8) & 0xFF));
  frame.push_back((uint8_t)(crc & 0xFF));
  return frame;
}

void test_whole_frame_in_one_feed(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  auto frame = build_frame(SB_PROTO_TYPE_JSON, {'{', '}'});
  sb_frame_parser_feed(&p, frame.data(), frame.size());

  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(0, cap.error_count);
  TEST_ASSERT_EQUAL_UINT8(SB_PROTO_TYPE_JSON, cap.last_type);
  TEST_ASSERT_EQUAL_UINT32(2, cap.last_len);
  TEST_ASSERT_EQUAL_UINT8('{', cap.last_payload[0]);
  TEST_ASSERT_EQUAL_UINT8('}', cap.last_payload[1]);
}

void test_frame_fed_one_byte_at_a_time(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  auto frame = build_frame(SB_PROTO_TYPE_JSON, {'h', 'i'});
  for (uint8_t b : frame) {
    sb_frame_parser_feed(&p, &b, 1);
  }

  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(0, cap.error_count);
}

void test_two_frames_back_to_back(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  auto f1 = build_frame(SB_PROTO_TYPE_JSON, {'a'});
  auto f2 = build_frame(SB_PROTO_TYPE_JSON, {'b', 'c'});
  sb_frame_parser_feed(&p, f1.data(), f1.size());
  sb_frame_parser_feed(&p, f2.data(), f2.size());

  TEST_ASSERT_EQUAL_INT(2, cap.complete_count);
  TEST_ASSERT_EQUAL_UINT32(2, cap.last_len);
  TEST_ASSERT_EQUAL_UINT8('c', cap.last_payload[1]);
}

void test_corrupt_crc_reports_error_and_recovers(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  auto bad = build_frame(SB_PROTO_TYPE_JSON, {'x'});
  bad[bad.size() - 1] ^= 0xFF;  // corrompe el ultimo byte del CRC
  auto good = build_frame(SB_PROTO_TYPE_JSON, {'y', 'z'});

  sb_frame_parser_feed(&p, bad.data(), bad.size());
  TEST_ASSERT_EQUAL_INT(0, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(1, cap.error_count);

  sb_frame_parser_feed(&p, good.data(), good.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(1, cap.error_count);
}

void test_oversized_length_reports_error_and_recovers(void) {
  Capture cap;
  uint8_t buf[8];  // capacidad deliberadamente pequena
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  std::vector<uint8_t> huge_payload(64, 0x41);
  auto bad = build_frame(SB_PROTO_TYPE_JSON, huge_payload);
  auto good = build_frame(SB_PROTO_TYPE_JSON, {'o', 'k'});

  sb_frame_parser_feed(&p, bad.data(), bad.size());
  TEST_ASSERT_EQUAL_INT(0, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(1, cap.error_count);

  sb_frame_parser_feed(&p, good.data(), good.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
}

void test_zero_length_payload(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  auto frame = build_frame(SB_PROTO_TYPE_JSON, {});
  sb_frame_parser_feed(&p, frame.data(), frame.size());

  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_UINT32(0, cap.last_len);
}

void test_garbage_before_magic_is_skipped(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  std::vector<uint8_t> stream = {0x00, 0xFF, 0xAB, 0x11};  // ruido + magic parcial falso
  auto frame = build_frame(SB_PROTO_TYPE_JSON, {'z'});
  for (uint8_t b : frame) stream.push_back(b);

  sb_frame_parser_feed(&p, stream.data(), stream.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(0, cap.error_count);
}

// Lo que codificamos para el SensorBoard tiene que poder parsearlo el mismo
// framing que usamos para leerle a el.
void test_encoded_frame_round_trips(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb,
                       &cap);

  const uint8_t payload[] = "{\"type\":\"cmd\",\"cmd\":\"capture\",\"id\":1}";
  uint8_t frame[128];
  size_t n = sb_frame_encode(SB_PROTO_TYPE_JSON, payload,
                             (uint32_t)(sizeof(payload) - 1), frame,
                             sizeof(frame));
  TEST_ASSERT_TRUE(n > 0);

  sb_frame_parser_feed(&p, frame, n);
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(0, cap.error_count);
  TEST_ASSERT_EQUAL_UINT32(sizeof(payload) - 1, cap.last_len);
  TEST_ASSERT_EQUAL_MEMORY(payload, cap.last_payload, sizeof(payload) - 1);
}

void test_encode_refuses_a_buffer_that_is_too_small(void) {
  const uint8_t payload[] = {1, 2, 3};
  uint8_t frame[8];  // 7 de cabecera + 3 + 2 de CRC no caben
  TEST_ASSERT_EQUAL_UINT32(
      0, sb_frame_encode(SB_PROTO_TYPE_JSON, payload, 3, frame, sizeof(frame)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_encoded_frame_round_trips);
  RUN_TEST(test_encode_refuses_a_buffer_that_is_too_small);
  RUN_TEST(test_whole_frame_in_one_feed);
  RUN_TEST(test_frame_fed_one_byte_at_a_time);
  RUN_TEST(test_two_frames_back_to_back);
  RUN_TEST(test_corrupt_crc_reports_error_and_recovers);
  RUN_TEST(test_oversized_length_reports_error_and_recovers);
  RUN_TEST(test_zero_length_payload);
  RUN_TEST(test_garbage_before_magic_is_skipped);
  return UNITY_END();
}
