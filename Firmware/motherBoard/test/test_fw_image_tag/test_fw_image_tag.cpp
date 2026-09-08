#include <unity.h>

#include <cstring>
#include <string>
#include <vector>

#include "fw_image_tag.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

const char *const kHmiTag = FW_TAG_PREFIX FW_BOARD_ID_DISPLAY_HMI;
const char *const kMbTag  = FW_TAG_PREFIX FW_BOARD_ID_MOTHERBOARD;

// Alimenta el matcher en trozos de chunk bytes, como hace HTTPUpload.
void feedInChunks(FwStreamMatcher &m, const std::string &data, size_t chunk) {
  for (size_t off = 0; off < data.size(); off += chunk) {
    const size_t n = (data.size() - off < chunk) ? data.size() - off : chunk;
    m.feed(reinterpret_cast<const uint8_t *>(data.data()) + off, n);
  }
}

// Binario de mentira: relleno + la marca que se le pida + mas relleno.
std::string fakeImage(const char *tag, size_t padBefore, size_t padAfter) {
  std::string s(padBefore, '\xE9');
  if (tag != nullptr) {
    s += tag;
  }
  s += std::string(padAfter, '\x00');
  return s;
}

} // namespace

// --- El matcher encuentra la marca este donde este ---

void test_marca_en_un_solo_trozo(void) {
  FwStreamMatcher m(kHmiTag);
  const std::string img = fakeImage(kHmiTag, 100, 100);
  m.feed(reinterpret_cast<const uint8_t *>(img.data()), img.size());
  TEST_ASSERT_TRUE(m.found());
}

void test_marca_partida_entre_dos_trozos(void) {
  // El caso que importa: la marca cae justo en la frontera de dos callbacks.
  const std::string img = fakeImage(kHmiTag, 10, 10);
  const size_t split = 10 + strlen(kHmiTag) / 2;
  FwStreamMatcher m(kHmiTag);
  m.feed(reinterpret_cast<const uint8_t *>(img.data()), split);
  m.feed(reinterpret_cast<const uint8_t *>(img.data()) + split, img.size() - split);
  TEST_ASSERT_TRUE(m.found());
}

void test_marca_byte_a_byte(void) {
  // Peor caso de troceado: un byte por llamada.
  FwStreamMatcher m(kHmiTag);
  feedInChunks(m, fakeImage(kHmiTag, 7, 7), 1);
  TEST_ASSERT_TRUE(m.found());
}

void test_marca_con_trozos_realistas(void) {
  FwStreamMatcher m(kHmiTag);
  feedInChunks(m, fakeImage(kHmiTag, 4096, 4096), 1436);
  TEST_ASSERT_TRUE(m.found());
}

void test_binario_sin_marca_no_coincide(void) {
  FwStreamMatcher m(kHmiTag);
  feedInChunks(m, fakeImage(nullptr, 2048, 2048), 512);
  TEST_ASSERT_FALSE(m.found());
}

void test_marca_de_otra_placa_no_coincide(void) {
  FwStreamMatcher m(kHmiTag);
  feedInChunks(m, fakeImage(kMbTag, 512, 512), 64);
  TEST_ASSERT_FALSE(m.found());
}

void test_prefijo_solo_no_basta_para_la_marca_completa(void) {
  // "IncuNestFW:" suelto (la constante del prefijo dentro del binario) no
  // puede hacerse pasar por la marca de esta placa.
  FwStreamMatcher m(kHmiTag);
  feedInChunks(m, fakeImage(FW_TAG_PREFIX, 32, 32), 8);
  TEST_ASSERT_FALSE(m.found());
}

void test_reset_olvida_lo_visto(void) {
  FwStreamMatcher m(kHmiTag);
  feedInChunks(m, fakeImage(kHmiTag, 16, 16), 16);
  TEST_ASSERT_TRUE(m.found());
  m.reset();
  TEST_ASSERT_FALSE(m.found());
  feedInChunks(m, fakeImage(nullptr, 64, 64), 16);
  TEST_ASSERT_FALSE(m.found());
}

void test_entradas_degeneradas_no_rompen(void) {
  FwStreamMatcher m(kHmiTag);
  m.feed(nullptr, 100);
  const uint8_t byte = 'I';
  m.feed(&byte, 0);
  TEST_ASSERT_FALSE(m.found());

  FwStreamMatcher vacio("");
  feedInChunks(vacio, fakeImage(kHmiTag, 8, 8), 4);
  TEST_ASSERT_FALSE(vacio.found()); // patron invalido: nunca dice que si

  FwStreamMatcher largo("IncuNestFW:esta-cadena-pasa-de-los-32-bytes-de-tope");
  feedInChunks(largo, fakeImage(kHmiTag, 8, 8), 4);
  TEST_ASSERT_FALSE(largo.found());
}

// --- La regla de decision ---

void test_binario_propio_se_acepta(void) {
  TEST_ASSERT_FALSE(fw_image_is_foreign(true, true));
}

void test_binario_de_otra_placa_se_rechaza(void) {
  TEST_ASSERT_TRUE(fw_image_is_foreign(false, true));
}

void test_binario_sin_marca_se_acepta(void) {
  // Build anterior a este cambio: se deja pasar para no bloquear el rollback.
  TEST_ASSERT_FALSE(fw_image_is_foreign(false, false));
}

// --- El escenario del incidente, de punta a punta ---

void test_hmi_rechaza_el_binario_de_la_motherboard(void) {
  // Lo que paso el 2026-09-08: el flasher subio motherboard/firmware.bin a un
  // Display HMI. El binario de la MB lleva su marca y el prefijo, nunca la
  // marca del HMI.
  const std::string mbImage = fakeImage(kMbTag, 4096, 4096);
  FwStreamMatcher self(kHmiTag);
  FwStreamMatcher any(FW_TAG_PREFIX);
  feedInChunks(self, mbImage, 1436);
  feedInChunks(any, mbImage, 1436);
  TEST_ASSERT_TRUE(fw_image_is_foreign(self.found(), any.found()));
}

void test_hmi_acepta_su_propio_binario(void) {
  // El binario del HMI lleva su marca completa y, suelto en .rodata, tambien
  // el prefijo: el prefijo por si solo no debe hacer que se rechace.
  std::string hmiImage = fakeImage(kHmiTag, 4096, 512);
  hmiImage += FW_TAG_PREFIX;
  hmiImage += std::string(1024, '\x00');

  FwStreamMatcher self(kHmiTag);
  FwStreamMatcher any(FW_TAG_PREFIX);
  feedInChunks(self, hmiImage, 1436);
  feedInChunks(any, hmiImage, 1436);
  TEST_ASSERT_FALSE(fw_image_is_foreign(self.found(), any.found()));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_marca_en_un_solo_trozo);
  RUN_TEST(test_marca_partida_entre_dos_trozos);
  RUN_TEST(test_marca_byte_a_byte);
  RUN_TEST(test_marca_con_trozos_realistas);
  RUN_TEST(test_binario_sin_marca_no_coincide);
  RUN_TEST(test_marca_de_otra_placa_no_coincide);
  RUN_TEST(test_prefijo_solo_no_basta_para_la_marca_completa);
  RUN_TEST(test_reset_olvida_lo_visto);
  RUN_TEST(test_entradas_degeneradas_no_rompen);
  RUN_TEST(test_binario_propio_se_acepta);
  RUN_TEST(test_binario_de_otra_placa_se_rechaza);
  RUN_TEST(test_binario_sin_marca_se_acepta);
  RUN_TEST(test_hmi_rechaza_el_binario_de_la_motherboard);
  RUN_TEST(test_hmi_acepta_su_propio_binario);
  return UNITY_END();
}
