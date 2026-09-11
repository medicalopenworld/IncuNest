#include "platform/plat_i2c.h"

#include "esp_log.h"

static const char *TAG = "plat_i2c";

I2cBus Wire;
I2cBus Wire1;

I2cBus::~I2cBus() { end(); }

bool I2cBus::begin(int sda, int scl, uint32_t freq_hz, int port) {
  if (bus_ != nullptr) {
    end();
  }
  i2c_master_bus_config_t cfg = {};
  cfg.i2c_port = port;
  cfg.sda_io_num = static_cast<gpio_num_t>(sda);
  cfg.scl_io_num = static_cast<gpio_num_t>(scl);
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  // Las pull-up internas del ESP32 son de ~45 kOhm: valen para arrancar, pero
  // las placas llevan sus propias resistencias externas. Se dejan puestas por
  // el mismo motivo que las ponia Wire.begin(): si una placa de banco viene
  // sin ellas, el bus no se queda muerto sin explicacion.
  cfg.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&cfg, &bus_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2c_new_master_bus(sda=%d scl=%d) -> %s", sda, scl,
             esp_err_to_name(err));
    bus_ = nullptr;
    return false;
  }
  freq_hz_ = freq_hz;
  device_count_ = 0;
  return true;
}

void I2cBus::end() {
  if (bus_ == nullptr) {
    return;
  }
  for (int i = 0; i < device_count_; i++) {
    i2c_master_bus_rm_device(devices_[i].handle);
  }
  device_count_ = 0;
  i2c_del_master_bus(bus_);
  bus_ = nullptr;
  freq_hz_ = 0;
}

i2c_master_dev_handle_t I2cBus::deviceFor(uint8_t addr) {
  for (int i = 0; i < device_count_; i++) {
    if (devices_[i].addr == addr) {
      return devices_[i].handle;
    }
  }
  if (bus_ == nullptr || device_count_ >= kMaxDevices) {
    ESP_LOGE(TAG, "sin hueco para el dispositivo 0x%02X", addr);
    return nullptr;
  }
  i2c_device_config_t cfg = {};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = addr;
  cfg.scl_speed_hz = freq_hz_;

  i2c_master_dev_handle_t handle = nullptr;
  esp_err_t err = i2c_master_bus_add_device(bus_, &cfg, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "add_device(0x%02X) -> %s", addr, esp_err_to_name(err));
    return nullptr;
  }
  devices_[device_count_].addr = addr;
  devices_[device_count_].handle = handle;
  device_count_++;
  return handle;
}

bool I2cBus::probe(uint8_t addr, int timeout_ms) {
  if (bus_ == nullptr) {
    return false;
  }
  return i2c_master_probe(bus_, addr, timeout_ms) == ESP_OK;
}

bool I2cBus::write(uint8_t addr, const uint8_t *data, size_t len,
                   int timeout_ms) {
  i2c_master_dev_handle_t dev = deviceFor(addr);
  if (dev == nullptr) {
    return false;
  }
  return i2c_master_transmit(dev, data, len, timeout_ms) == ESP_OK;
}

bool I2cBus::read(uint8_t addr, uint8_t *buf, size_t len, int timeout_ms) {
  i2c_master_dev_handle_t dev = deviceFor(addr);
  if (dev == nullptr) {
    return false;
  }
  return i2c_master_receive(dev, buf, len, timeout_ms) == ESP_OK;
}

bool I2cBus::writeRead(uint8_t addr, const uint8_t *out, size_t out_len,
                       uint8_t *in, size_t in_len, int timeout_ms) {
  i2c_master_dev_handle_t dev = deviceFor(addr);
  if (dev == nullptr) {
    return false;
  }
  return i2c_master_transmit_receive(dev, out, out_len, in, in_len,
                                     timeout_ms) == ESP_OK;
}

bool I2cBus::writeReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  const uint8_t payload[2] = {reg, value};
  return write(addr, payload, sizeof(payload));
}

bool I2cBus::readReg(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
  return writeRead(addr, &reg, 1, buf, len);
}

// ---------------------------------------------------------------------------
// Compatibilidad con Wire. Ver la advertencia de plat_i2c.h: esto es SOLO
// para las librerias de sensor vendorizadas, no para el codigo de la app.
// ---------------------------------------------------------------------------

void I2cBus::beginTransmission(uint8_t addr) {
  tx_addr_ = addr;
  tx_len_ = 0;
  tx_open_ = true;
}

size_t I2cBus::write(uint8_t data) {
  if (!tx_open_ || tx_len_ >= kBufSize) {
    return 0;
  }
  tx_buf_[tx_len_++] = data;
  return 1;
}

size_t I2cBus::write(const uint8_t *data, size_t len) {
  if (!tx_open_ || data == nullptr) {
    return 0;
  }
  size_t n = 0;
  while (n < len && tx_len_ < kBufSize) {
    tx_buf_[tx_len_++] = data[n++];
  }
  return n;
}

uint8_t I2cBus::endTransmission(bool sendStop) {
  if (!tx_open_) {
    return 4; // "otro error", igual que Wire
  }
  tx_open_ = false;

  if (!sendStop) {
    // START repetido: NO se envia nada todavia. Lo resolvera el requestFrom()
    // siguiente en una sola transaccion escritura+lectura, que es exactamente
    // lo que veia el bus con Arduino.
    pending_restart_ = true;
    return 0;
  }

  pending_restart_ = false;
  if (tx_len_ == 0) {
    // endTransmission() sin datos era el sondeo de presencia de Arduino.
    return probe(tx_addr_) ? 0 : 2; // 2 = NACK a la direccion
  }
  const bool ok = write(tx_addr_, tx_buf_, tx_len_);
  tx_len_ = 0;
  return ok ? 0 : 2;
}

uint8_t I2cBus::requestFrom(uint8_t addr, uint8_t len, bool sendStop) {
  (void)sendStop;
  rx_len_ = 0;
  rx_pos_ = 0;
  if (len == 0 || len > kBufSize) {
    return 0;
  }

  bool ok;
  if (pending_restart_ && tx_len_ > 0 && addr == tx_addr_) {
    ok = writeRead(addr, tx_buf_, tx_len_, rx_buf_, len);
    tx_len_ = 0;
    pending_restart_ = false;
  } else {
    ok = read(addr, rx_buf_, len);
  }
  if (!ok) {
    return 0;
  }
  rx_len_ = len;
  return len;
}

int I2cBus::available() { return static_cast<int>(rx_len_ - rx_pos_); }

int I2cBus::read() {
  if (rx_pos_ >= rx_len_) {
    return -1;
  }
  return rx_buf_[rx_pos_++];
}

int I2cBus::peek() {
  if (rx_pos_ >= rx_len_) {
    return -1;
  }
  return rx_buf_[rx_pos_];
}
