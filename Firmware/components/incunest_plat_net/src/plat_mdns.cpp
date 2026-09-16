#include "platform/plat_mdns.h"

#include <cstring>
#include <string>

#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "plat_mdns";

MDNSResponder MDNS;

bool MDNSResponder::begin(const char *hostname) {
  if (!started_) {
    const esp_err_t err = mdns_init();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "mdns_init -> %s", esp_err_to_name(err));
      return false;
    }
    started_ = true;
  }
  return mdns_hostname_set(hostname) == ESP_OK;
}

void MDNSResponder::end() {
  if (started_) {
    mdns_free();
    started_ = false;
  }
}

bool MDNSResponder::addService(const char *service, const char *proto, uint16_t port) {
  if (!started_ || service == nullptr || proto == nullptr) {
    return false;
  }
  std::string s = service[0] == '_' ? service : std::string("_") + service;
  std::string p = proto[0] == '_' ? proto : std::string("_") + proto;
  return mdns_service_add(nullptr, s.c_str(), p.c_str(), port, nullptr, 0) == ESP_OK;
}
