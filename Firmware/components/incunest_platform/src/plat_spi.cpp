#include "platform/plat_spi.h"

#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // portMAX_DELAY

static const char *TAG = "plat_spi";

SpiBus SPI;

bool SpiBus::begin(int sck, int miso, int mosi, int ss) {
  if (bus_ready_) {
    return true;
  }
  spi_bus_config_t cfg = {};
  cfg.mosi_io_num = mosi;
  cfg.miso_io_num = miso;
  cfg.sclk_io_num = sck;
  cfg.quadwp_io_num = -1;
  cfg.quadhd_io_num = -1;
  // El AFE4490 se lee byte a byte; no hace falta un bufer DMA grande.
  cfg.max_transfer_sz = 64;

  // Sin DMA a proposito (SPI_DMA_DISABLED): las transferencias son de 1-2
  // bytes y montar un descriptor de DMA por byte costaria mas que moverlo.
  esp_err_t err = spi_bus_initialize(host_, &cfg, SPI_DMA_DISABLED);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_initialize -> %s", esp_err_to_name(err));
    return false;
  }
  bus_ready_ = true;
  (void)ss; // el CS lo gobierna el driver del dispositivo, como antes
  return true;
}

bool SpiBus::begin() { return bus_ready_; }

void SpiBus::end() {
  if (dev_ != nullptr) {
    spi_bus_remove_device(dev_);
    dev_ = nullptr;
  }
  if (bus_ready_) {
    spi_bus_free(host_);
    bus_ready_ = false;
  }
}

bool SpiBus::ensureDevice(const SPISettings &settings) {
  if (dev_ != nullptr && current_ == settings) {
    return true;
  }
  if (dev_ != nullptr) {
    spi_bus_remove_device(dev_);
    dev_ = nullptr;
  }
  spi_device_interface_config_t cfg = {};
  cfg.clock_speed_hz = static_cast<int>(settings.clock());
  cfg.mode = settings.dataMode();
  cfg.spics_io_num = -1; // CS por GPIO desde el driver del AFE, como antes
  cfg.queue_size = 1;
  if (settings.bitOrder() == LSBFIRST) {
    cfg.flags = SPI_DEVICE_BIT_LSBFIRST;
  }

  esp_err_t err = spi_bus_add_device(host_, &cfg, &dev_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_add_device -> %s", esp_err_to_name(err));
    dev_ = nullptr;
    return false;
  }
  current_ = settings;
  return true;
}

void SpiBus::beginTransaction(const SPISettings &settings) {
  if (!bus_ready_ || !ensureDevice(settings)) {
    return;
  }
  // spi_device_acquire_bus() reserva el bus para la secuencia entera, que es
  // lo que hacia beginTransaction() de Arduino: impide que otra tarea meta
  // una transferencia en medio de un registro del AFE.
  if (spi_device_acquire_bus(dev_, portMAX_DELAY) == ESP_OK) {
    in_transaction_ = true;
  }
}

void SpiBus::endTransaction() {
  if (in_transaction_ && dev_ != nullptr) {
    spi_device_release_bus(dev_);
    in_transaction_ = false;
  }
}

uint8_t SpiBus::transfer(uint8_t data) {
  if (dev_ == nullptr) {
    return 0;
  }
  spi_transaction_t t = {};
  t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
  t.length = 8;
  t.tx_data[0] = data;
  // Por sondeo, sin interrupciones: es el equivalente de IDF al
  // spiTransferByteNL() que usaba Arduino.
  if (spi_device_polling_transmit(dev_, &t) != ESP_OK) {
    return 0;
  }
  return t.rx_data[0];
}

uint16_t SpiBus::transfer16(uint16_t data) {
  const uint8_t hi = transfer(static_cast<uint8_t>(data >> 8));
  const uint8_t lo = transfer(static_cast<uint8_t>(data & 0xFF));
  return static_cast<uint16_t>((hi << 8) | lo);
}

void SpiBus::transfer(void *buf, size_t count) {
  if (dev_ == nullptr || buf == nullptr || count == 0) {
    return;
  }
  uint8_t *p = static_cast<uint8_t *>(buf);
  for (size_t i = 0; i < count; i++) {
    p[i] = transfer(p[i]);
  }
}
