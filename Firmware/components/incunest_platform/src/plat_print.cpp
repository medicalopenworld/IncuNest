#include "platform/plat_print.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "platform/plat_time.h"

size_t Print::write(const uint8_t *buf, size_t len) {
  if (buf == nullptr) {
    return 0;
  }
  size_t n = 0;
  while (n < len) {
    if (write(buf[n]) != 1) {
      break;
    }
    n++;
  }
  return n;
}

size_t Print::write(const char *s) {
  if (s == nullptr) {
    return 0;
  }
  return write(reinterpret_cast<const uint8_t *>(s), strlen(s));
}

size_t Print::print(const char *s) { return write(s); }

// Arduino terminaba las lineas en "\r\n", y la motherBoard parsea el
// protocolo del HMI buscando exactamente eso. No cambiar a "\n".
size_t Print::println() { return write("\r\n"); }

size_t Print::printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const size_t n = vprintf(fmt, args);
  va_end(args);
  return n;
}

size_t Print::vprintf(const char *fmt, va_list args) {
  char stack_buf[128];
  va_list copy;
  va_copy(copy, args);
  int n = vsnprintf(stack_buf, sizeof(stack_buf), fmt, copy);
  va_end(copy);
  if (n < 0) {
    return 0;
  }
  if (static_cast<size_t>(n) < sizeof(stack_buf)) {
    return write(reinterpret_cast<const uint8_t *>(stack_buf),
                 static_cast<size_t>(n));
  }
  // Mensaje largo (las tramas de telemetria hacia el HMI pueden pasar de
  // 128 B): se formatea en el heap con el tamano exacto.
  std::vector<char> heap_buf(static_cast<size_t>(n) + 1);
  vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args);
  return write(reinterpret_cast<const uint8_t *>(heap_buf.data()),
               static_cast<size_t>(n));
}

size_t Stream::readBytes(char *buf, size_t len) {
  if (buf == nullptr) {
    return 0;
  }
  size_t n = 0;
  const uint32_t t0 = millis();
  while (n < len) {
    const int c = read();
    if (c >= 0) {
      buf[n++] = static_cast<char>(c);
      continue;
    }
    if (static_cast<uint32_t>(millis() - t0) >= timeout_ms_) {
      break;
    }
    delay_ms(1);
  }
  return n;
}

String Stream::readStringUntil(char terminator) {
  std::string acc;
  const uint32_t t0 = millis();
  for (;;) {
    const int c = read();
    if (c >= 0) {
      if (static_cast<char>(c) == terminator) {
        break;
      }
      acc.push_back(static_cast<char>(c));
      continue;
    }
    if (static_cast<uint32_t>(millis() - t0) >= timeout_ms_) {
      break;
    }
    delay_ms(1);
  }
  return String(acc);
}
