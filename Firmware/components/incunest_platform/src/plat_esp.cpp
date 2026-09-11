#include "platform/plat_esp.h"

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"

EspClass ESP;

uint32_t EspClass::getFreeHeap() const { return esp_get_free_heap_size(); }

uint32_t EspClass::getMinFreeHeap() const {
  return esp_get_minimum_free_heap_size();
}

uint32_t EspClass::getHeapSize() const {
  return (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
}

uint32_t EspClass::getPsramSize() const {
  return (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

uint32_t EspClass::getFreePsram() const {
  return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

uint32_t EspClass::getFlashChipSize() const {
  // Arduino leia el tamano REAL del chip, no el declarado en el sdkconfig.
  // spi_flash_get_chip_size() se retiro en IDF 5; el equivalente vigente es
  // el tamano de la particion mapeada por el bootloader.
  uint32_t size = 0;
  if (esp_flash_get_size(NULL, &size) != ESP_OK) {
    return 0;
  }
  return size;
}

void EspClass::restart() const { esp_restart(); }
