#include "platform/plat_net_client.h"

#include <cerrno>
#include <cstring>

#include "esp_log.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "platform/plat_time.h"

static const char *TAG = "plat_net";

// ---------------------------------------------------------------- WiFiClient

WiFiClient::~WiFiClient() { WiFiClient::stop(); }

void WiFiClient::setTimeout(uint32_t seconds) {
  timeout_ms_ = seconds * 1000U;
  Stream::setTimeout(timeout_ms_);
  if (fd_ >= 0) {
    struct timeval tv = {static_cast<time_t>(seconds), 0};
    setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }
}

int WiFiClient::openSocket(const char *host, uint16_t port) {
  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", port);
  if (getaddrinfo(host, port_str, &hints, &res) != 0 || res == nullptr) {
    ESP_LOGW(TAG, "no se resuelve '%s'", host);
    return -1;
  }
  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd < 0) {
    freeaddrinfo(res);
    return -1;
  }
  struct timeval tv = {static_cast<time_t>(timeout_ms_ / 1000), 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
    ESP_LOGW(TAG, "connect(%s:%u) -> errno %d", host, port, errno);
    close(fd);
    freeaddrinfo(res);
    return -1;
  }
  freeaddrinfo(res);
  return fd;
}

int WiFiClient::connect(const char *host, uint16_t port) {
  stop();
  fd_ = openSocket(host, port);
  return fd_ >= 0 ? 1 : 0;
}

int WiFiClient::connect(IPAddress ip, uint16_t port) {
  return connect(ip.toString().c_str(), port);
}

uint8_t WiFiClient::connected() {
  if (fd_ < 0) {
    return 0;
  }
  // Como Arduino: sigue "conectado" mientras el socket este abierto o quede
  // algo por leer; un recv de 0 bytes (FIN) lo cierra.
  if (peeked_ >= 0) {
    return 1;
  }
  uint8_t b;
  const int n = recv(fd_, &b, 1, MSG_PEEK | MSG_DONTWAIT);
  if (n == 0) {
    stop();
    return 0;
  }
  if (n < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
    stop();
    return 0;
  }
  return 1;
}

void WiFiClient::stop() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
  peeked_ = -1;
}

int WiFiClient::rawAvailable() {
  if (fd_ < 0) {
    return 0;
  }
  int n = 0;
  if (ioctl(fd_, FIONREAD, &n) != 0) {
    return 0;
  }
  return n;
}

int WiFiClient::rawRead(uint8_t *buf, size_t len) {
  if (fd_ < 0) {
    return -1;
  }
  const int n = recv(fd_, buf, len, MSG_DONTWAIT);
  if (n == 0) {
    stop();
    return -1;
  }
  if (n < 0) {
    return -1;
  }
  return n;
}

int WiFiClient::rawWrite(const uint8_t *buf, size_t len) {
  if (fd_ < 0) {
    return -1;
  }
  return send(fd_, buf, len, 0);
}

int WiFiClient::available() {
  return rawAvailable() + (peeked_ >= 0 ? 1 : 0);
}

int WiFiClient::read() {
  if (peeked_ >= 0) {
    const int c = peeked_;
    peeked_ = -1;
    return c;
  }
  uint8_t b;
  return rawRead(&b, 1) == 1 ? b : -1;
}

int WiFiClient::peek() {
  if (peeked_ < 0) {
    uint8_t b;
    if (rawRead(&b, 1) == 1) {
      peeked_ = b;
    }
  }
  return peeked_;
}

size_t WiFiClient::write(uint8_t b) { return write(&b, 1); }

size_t WiFiClient::write(const uint8_t *buf, size_t len) {
  if (buf == nullptr) {
    return 0;
  }
  size_t sent = 0;
  while (sent < len) {
    const int n = rawWrite(buf + sent, len - sent);
    if (n <= 0) {
      break;
    }
    sent += static_cast<size_t>(n);
  }
  return sent;
}

// ---------------------------------------------------------- WiFiClientSecure

WiFiClientSecure::~WiFiClientSecure() { WiFiClientSecure::stop(); }

int WiFiClientSecure::connect(const char *host, uint16_t port) {
  stop();
  tls_ = esp_tls_init();
  if (tls_ == nullptr) {
    return 0;
  }
  esp_tls_cfg_t cfg = {};
  cfg.timeout_ms = static_cast<int>(timeout_ms_);
  if (insecure_) {
    // Sin CA y sin comprobar el nombre: esp-tls lo acepta solo con
    // CONFIG_ESP_TLS_INSECURE + SKIP_SERVER_CERT_VERIFY, que el sdkconfig de la
    // motherBoard activa por esto mismo.
    cfg.skip_common_name = true;
  } else if (ca_pem_ != nullptr) {
    cfg.cacert_buf = reinterpret_cast<const unsigned char *>(ca_pem_);
    cfg.cacert_bytes = strlen(ca_pem_) + 1;
  }
  const int r = esp_tls_conn_new_sync(host, static_cast<int>(strlen(host)), port, &cfg, tls_);
  if (r != 1) {
    ESP_LOGW(TAG, "TLS a %s:%u fallo (%d)", host, port, r);
    esp_tls_conn_destroy(tls_);
    tls_ = nullptr;
    return 0;
  }
  esp_tls_get_conn_sockfd(tls_, &fd_);
  return 1;
}

int WiFiClientSecure::connect(IPAddress ip, uint16_t port) {
  return connect(ip.toString().c_str(), port);
}

uint8_t WiFiClientSecure::connected() {
  if (tls_ == nullptr) {
    return 0;
  }
  if (peeked_ >= 0 || rawAvailable() > 0) {
    return 1;
  }
  // Sin datos pendientes: mirar si el socket sigue vivo sin consumir nada.
  uint8_t b;
  const int n = recv(fd_, &b, 1, MSG_PEEK | MSG_DONTWAIT);
  if (n == 0 || (n < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
    stop();
    return 0;
  }
  return 1;
}

void WiFiClientSecure::stop() {
  if (tls_ != nullptr) {
    esp_tls_conn_destroy(tls_); // cierra tambien el socket
    tls_ = nullptr;
  }
  fd_ = -1;
  peeked_ = -1;
}

int WiFiClientSecure::rawAvailable() {
  if (tls_ == nullptr) {
    return 0;
  }
  // Bytes ya descifrados y a la espera en mbedtls...
  const ssize_t buffered = esp_tls_get_bytes_avail(tls_);
  if (buffered > 0) {
    return static_cast<int>(buffered);
  }
  // ...o cifrados esperando en el socket. Cuenta como "hay algo" aunque no
  // sepamos cuantos bytes saldran tras descifrar.
  int n = 0;
  if (fd_ >= 0 && ioctl(fd_, FIONREAD, &n) == 0 && n > 0) {
    return 1;
  }
  return 0;
}

int WiFiClientSecure::rawRead(uint8_t *buf, size_t len) {
  if (tls_ == nullptr) {
    return -1;
  }
  if (rawAvailable() <= 0) {
    return -1; // no bloquear: el codigo sondea con available()
  }
  const ssize_t n = esp_tls_conn_read(tls_, buf, len);
  if (n == 0) {
    stop();
    return -1;
  }
  if (n < 0) {
    if (n == ESP_TLS_ERR_SSL_WANT_READ || n == ESP_TLS_ERR_SSL_WANT_WRITE) {
      return -1;
    }
    stop();
    return -1;
  }
  return static_cast<int>(n);
}

int WiFiClientSecure::rawWrite(const uint8_t *buf, size_t len) {
  if (tls_ == nullptr) {
    return -1;
  }
  const ssize_t n = esp_tls_conn_write(tls_, buf, len);
  return n < 0 ? -1 : static_cast<int>(n);
}
