#pragma once

// mDNS con la forma del objeto MDNS de Arduino (ESPmDNS), sobre el componente
// espressif/mdns. El firmware solo usa begin(hostname) y
// addService("http", "tcp", 80) para anunciar la pagina de flasheo.

#include <cstdint>

class MDNSResponder {
public:
  bool begin(const char *hostname);
  void end();
  // Arduino admitia "http"/"tcp" sin el guion bajo y lo anadia; aqui igual.
  bool addService(const char *service, const char *proto, uint16_t port);

private:
  bool started_ = false;
};

extern MDNSResponder MDNS;
