#include <string.h>
#include <unity.h>

#include "family_proto.h"

void setUp(void) {}
void tearDown(void) {}

// URL de n caracteres: "https://" + 'a' * (n - 8).
static void makeUrl(char *dst, size_t n) {
  memcpy(dst, "https://", 8);
  for (size_t i = 8; i < n; i++) dst[i] = 'a';
  dst[n] = '\0';
}

void test_build_with_url(void) {
  FamilyMsg m = {true, "https://incutwin.org/f/7KQ2M9"};
  char buf[160];
  TEST_ASSERT_GREATER_THAN(0, family_proto_build(buf, sizeof buf, &m));
  TEST_ASSERT_EQUAL_STRING("CTRL,FAMILY,1,https://incutwin.org/f/7KQ2M9\n", buf);
}

void test_build_without_url(void) {
  FamilyMsg m = {false, ""};
  char buf[32];
  family_proto_build(buf, sizeof buf, &m);
  TEST_ASSERT_EQUAL_STRING("CTRL,FAMILY,0,\n", buf);
}

void test_build_too_small_returns_0(void) {
  FamilyMsg m = {true, "https://incutwin.org/f/7KQ2M9"};
  char buf[10];
  TEST_ASSERT_EQUAL_INT(0, family_proto_build(buf, sizeof buf, &m));
  FamilyMsg bad = {true, "http://a.org/f/1"};
  char big[160];
  TEST_ASSERT_EQUAL_INT(0, family_proto_build(big, sizeof big, &bad));
}

void test_roundtrip(void) {
  FamilyMsg m = {true, "https://incutwin.org/f/7KQ2M9"};
  char buf[160];
  const int n = family_proto_build(buf, sizeof buf, &m);
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL_CHAR('\n', buf[n - 1]);
  buf[n - 1] = '\0';
  FamilyMsg out;
  memset(&out, 0xAA, sizeof out);
  TEST_ASSERT_TRUE(family_proto_parse(buf, &out));
  TEST_ASSERT_TRUE(out.notice);
  TEST_ASSERT_EQUAL_STRING(m.url, out.url);

  FamilyMsg e = {false, ""};
  const int n2 = family_proto_build(buf, sizeof buf, &e);
  buf[n2 - 1] = '\0';
  TEST_ASSERT_TRUE(family_proto_parse(buf, &out));
  TEST_ASSERT_FALSE(out.notice);
  TEST_ASSERT_EQUAL_STRING("", out.url);
}

static void assertRejected(const char *line) {
  FamilyMsg out;
  out.notice = true;
  strcpy(out.url, "junk");
  TEST_ASSERT_FALSE(family_proto_parse(line, &out));
  TEST_ASSERT_FALSE(out.notice);
  TEST_ASSERT_EQUAL_CHAR('\0', out.url[0]);
}

void test_parse_rejects_bad_flag(void) {
  assertRejected("CTRL,FAMILY,2,");
  assertRejected("CTRL,FAMILY,x,");
  assertRejected("CTRL,FAMILY,,");
  assertRejected("CTRL,FAMILY,10,");
}

void test_parse_rejects_missing_field(void) {
  assertRejected("CTRL,FAMILY,1");
  assertRejected("CTRL,FAMILY,");
  assertRejected("CTRL,SERVICE,1,");
}

void test_parse_rejects_extra_field(void) {
  assertRejected("CTRL,FAMILY,1,https://a.org/f/1,x");
  assertRejected("CTRL,FAMILY,1,http://a.org/f/1");
}

void test_url_rejects_http(void) {
  TEST_ASSERT_FALSE(family_url_valid("http://a.org/f/1"));
  TEST_ASSERT_FALSE(family_url_valid("HTTPS://a.org/f/1"));
  TEST_ASSERT_FALSE(family_url_valid(nullptr));
  TEST_ASSERT_TRUE(family_url_valid("https://a.org/f/1"));
}

void test_url_rejects_comma_space_ctrl(void) {
  TEST_ASSERT_FALSE(family_url_valid("https://a.org/f,1"));
  TEST_ASSERT_FALSE(family_url_valid("https://a.org/ f"));
  TEST_ASSERT_FALSE(family_url_valid("https://a.org/\t"));
  TEST_ASSERT_FALSE(family_url_valid("https://a.org/\x7F"));
  TEST_ASSERT_FALSE(family_url_valid("https://a.org/\xC3\xB1"));
}

void test_url_rejects_too_long(void) {
  char u[FAMILY_URL_MAX + 8];
  makeUrl(u, 97);
  TEST_ASSERT_FALSE(family_url_valid(u));
  makeUrl(u, 96);
  TEST_ASSERT_TRUE(family_url_valid(u));
  makeUrl(u, 9);
  TEST_ASSERT_TRUE(family_url_valid(u));
}

void test_url_rejects_bare_scheme(void) {
  TEST_ASSERT_FALSE(family_url_valid("https://"));
  TEST_ASSERT_FALSE(family_url_valid(""));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_build_with_url);
  RUN_TEST(test_build_without_url);
  RUN_TEST(test_build_too_small_returns_0);
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_parse_rejects_bad_flag);
  RUN_TEST(test_parse_rejects_missing_field);
  RUN_TEST(test_parse_rejects_extra_field);
  RUN_TEST(test_url_rejects_http);
  RUN_TEST(test_url_rejects_comma_space_ctrl);
  RUN_TEST(test_url_rejects_too_long);
  RUN_TEST(test_url_rejects_bare_scheme);
  return UNITY_END();
}
