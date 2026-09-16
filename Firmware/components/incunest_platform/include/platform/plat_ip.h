#pragma once

// Direccion IPv4 con la forma de la clase IPAddress de Arduino.
//
// Es un valor de 4 bytes con conversion a/desde uint32_t y a texto; no hay
// nada de red aqui dentro. Se conserva el nombre y la forma porque aparece en
// estructuras de estado del firmware (GPRS.h: `IPAddress IP;`) y en
// comparaciones del tipo `WiFi.localIP() != IPAddress(0,0,0,0)`. Cuando se
// porte la capa de red a esp_wifi/esp_netif, esta clase es la que hara de
// puente con esp_ip4_addr_t sin tocar esos sitios.

#include <cstdint>
#include <cstdio>

#include "platform/plat_string.h"

class IPAddress {
public:
  IPAddress() : addr_{0, 0, 0, 0} {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : addr_{a, b, c, d} {}

  // Igual que Arduino: el uint32_t va en orden de red (a.b.c.d = a en el byte
  // menos significativo), que es como lo entrega lwIP.
  explicit IPAddress(uint32_t raw) {
    addr_[0] = static_cast<uint8_t>(raw & 0xFF);
    addr_[1] = static_cast<uint8_t>((raw >> 8) & 0xFF);
    addr_[2] = static_cast<uint8_t>((raw >> 16) & 0xFF);
    addr_[3] = static_cast<uint8_t>((raw >> 24) & 0xFF);
  }

  uint8_t operator[](int i) const { return addr_[i & 3]; }
  uint8_t &operator[](int i) { return addr_[i & 3]; }

  operator uint32_t() const {
    return static_cast<uint32_t>(addr_[0]) |
           (static_cast<uint32_t>(addr_[1]) << 8) |
           (static_cast<uint32_t>(addr_[2]) << 16) |
           (static_cast<uint32_t>(addr_[3]) << 24);
  }

  bool operator==(const IPAddress &o) const {
    return addr_[0] == o.addr_[0] && addr_[1] == o.addr_[1] &&
           addr_[2] == o.addr_[2] && addr_[3] == o.addr_[3];
  }
  bool operator!=(const IPAddress &o) const { return !(*this == o); }

  String toString() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", addr_[0], addr_[1], addr_[2],
             addr_[3]);
    return String(buf);
  }

private:
  uint8_t addr_[4];
};
