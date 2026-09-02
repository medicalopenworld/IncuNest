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

// El defecto que motiva el bound-check por byte: el dueno del parser encoge el
// buffer a mitad de un payload grande (una captura liberada al desconectar el
// USB) y, sin comprobacion, los bytes siguientes se escribian fuera del
// buffer nuevo -- decenas de KB sobre un array de 256 B en .bss.
void test_shrinking_the_buffer_mid_payload_does_not_overflow(void) {
  Capture cap;
  uint8_t big[4096];
  // Arena unica: los primeros 64 B hacen de buffer pequeno y los 64
  // siguientes de centinela. Dos arrays locales distintos no estan
  // garantizados adyacentes, asi que no detectarian el desbordamiento.
  uint8_t arena[128];
  uint8_t *small = arena;
  const size_t kSmallCap = 64;
  uint8_t *guard = arena + kSmallCap;
  const size_t kGuardLen = sizeof(arena) - kSmallCap;
  memset(arena, 0xA5, sizeof(arena));

  SbFrameParser p;
  sb_frame_parser_init(&p, big, sizeof(big), on_complete_cb, on_error_cb, &cap);

  // Frame binario grande a medias.
  std::vector<uint8_t> payload(2000, 0x5A);
  auto frame = build_frame(SB_PROTO_TYPE_JPEG, payload);
  sb_frame_parser_feed(&p, frame.data(), 500);  // cabecera + parte del payload

  // Se encoge el buffer, como al liberar la captura.
  sb_frame_parser_set_buffer(&p, small, kSmallCap);

  // El resto del frame viejo ya no debe escribirse en ningun sitio.
  sb_frame_parser_feed(&p, frame.data() + 500, frame.size() - 500);

  for (size_t i = 0; i < kGuardLen; i++) {
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5, guard[i], "escritura fuera de rango");
  }
  TEST_ASSERT_EQUAL_INT(0, cap.complete_count);

  // Y el parser sigue vivo: un frame nuevo se entrega con normalidad.
  auto good = build_frame(SB_PROTO_TYPE_JSON, {'o', 'k'});
  sb_frame_parser_feed(&p, good.data(), good.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
}

// Un Length corrupto en una cabecera JSON no puede aprovechar la capacidad
// grande de una captura en vuelo para tragarse 100 KB de stream.
void test_json_length_beyond_type_maximum_is_rejected(void) {
  Capture cap;
  uint8_t buf[4096];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb, &cap);

  std::vector<uint8_t> payload(1000, 0x42);  // > SB_PROTO_MAX_JSON_PAYLOAD
  auto frame = build_frame(SB_PROTO_TYPE_JSON, payload);
  sb_frame_parser_feed(&p, frame.data(), frame.size());

  TEST_ASSERT_EQUAL_INT(0, cap.complete_count);
  TEST_ASSERT_TRUE(cap.error_count >= 1);

  auto good = build_frame(SB_PROTO_TYPE_JSON, {'y'});
  sb_frame_parser_feed(&p, good.data(), good.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
}

void test_unknown_type_is_rejected_without_consuming_its_payload(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb, &cap);

  auto bad = build_frame(0x7Fu, {1, 2, 3});
  sb_frame_parser_feed(&p, bad.data(), bad.size());
  TEST_ASSERT_EQUAL_INT(0, cap.complete_count);
  TEST_ASSERT_TRUE(cap.error_count >= 1);

  auto good = build_frame(SB_PROTO_TYPE_JSON, {'z'});
  sb_frame_parser_feed(&p, good.data(), good.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
}

// Al perder el dispositivo hay que descartar el frame a medias: si no, el
// primer byte del stream nuevo continuaba el frame del anterior.
void test_reset_discards_a_half_received_frame(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb, &cap);

  auto frame = build_frame(SB_PROTO_TYPE_JSON, {'a', 'b', 'c', 'd'});
  sb_frame_parser_feed(&p, frame.data(), 9);  // cabecera + 2 bytes de payload
  sb_frame_parser_reset(&p);

  sb_frame_parser_feed(&p, frame.data() + 9, frame.size() - 9);
  TEST_ASSERT_EQUAL_INT(0, cap.complete_count);  // la cola no completa nada

  sb_frame_parser_feed(&p, frame.data(), frame.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
}

// Un frame valido cuyo payload contiene el propio magic: obligatorio, un JPEG
// de 30 KB lo contiene con alta probabilidad.
void test_magic_inside_payload_does_not_break_framing(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb, &cap);

  auto frame = build_frame(SB_PROTO_TYPE_JSON,
                           {SB_PROTO_MAGIC_0, SB_PROTO_MAGIC_1, 0x00, 0x10});
  sb_frame_parser_feed(&p, frame.data(), frame.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
  TEST_ASSERT_EQUAL_INT(0, cap.error_count);
}

void test_double_magic_prefix_resyncs(void) {
  Capture cap;
  uint8_t buf[512];
  SbFrameParser p;
  sb_frame_parser_init(&p, buf, sizeof(buf), on_complete_cb, on_error_cb, &cap);

  std::vector<uint8_t> stream = {SB_PROTO_MAGIC_0};  // magic a medias
  auto frame = build_frame(SB_PROTO_TYPE_JSON, {'q'});
  for (uint8_t b : frame) stream.push_back(b);

  sb_frame_parser_feed(&p, stream.data(), stream.size());
  TEST_ASSERT_EQUAL_INT(1, cap.complete_count);
}

void test_encode_rejects_an_impossible_length(void) {
  const uint8_t payload[] = {1, 2, 3};
  uint8_t frame[64];
  // Sin el rechazo previo, el total daba la vuelta y la guarda de capacidad
  // pasaba: el bucle de copia intentaba escribir 4 GB.
  TEST_ASSERT_EQUAL_UINT32(0, sb_frame_encode(SB_PROTO_TYPE_JSON, payload,
                                              0xFFFFFFFFu, frame,
                                              sizeof(frame)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_shrinking_the_buffer_mid_payload_does_not_overflow);
  RUN_TEST(test_json_length_beyond_type_maximum_is_rejected);
  RUN_TEST(test_unknown_type_is_rejected_without_consuming_its_payload);
  RUN_TEST(test_reset_discards_a_half_received_frame);
  RUN_TEST(test_magic_inside_payload_does_not_break_framing);
  RUN_TEST(test_double_magic_prefix_resyncs);
  RUN_TEST(test_encode_rejects_an_impossible_length);
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
