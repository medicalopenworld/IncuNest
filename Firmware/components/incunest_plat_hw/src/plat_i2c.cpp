#include "platform/plat_i2c.h"

#include "esp_log.h"

static const char *TAG = "plat_i2c";

// Solo protege la creacion perezosa del mutex de cada bus, no las
// transacciones. Se mantiene microscopica a proposito.
static portMUX_TYPE s_lock_init_spin = portMUX_INITIALIZER_UNLOCKED;

I2cBus Wire;
I2cBus Wire1;

// Creacion perezosa del cerrojo. Los buses son globales y begin() se llama
// desde initHardware(), pero alguna libreria vendorizada puede tocar el bus
// antes; crearlo aqui hace que la primera llamada, sea cual sea, lo tenga.
// Solo la primera pasada entra en la seccion critica.
void I2cBus::lock() {
  if (lock_ == nullptr) {
    SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
    portENTER_CRITICAL(&s_lock_init_spin);
    if (lock_ == nullptr) {
      lock_ = m;
      m = nullptr;
    }
    portEXIT_CRITICAL(&s_lock_init_spin);
    if (m != nullptr) {
      vSemaphoreDelete(m); // otra tarea gano la carrera
    }
    if (lock_ == nullptr) {
      ESP_LOGE(TAG, "sin memoria para el mutex del bus");
      return;
    }
  }
  xSemaphoreTakeRecursive(lock_, portMAX_DELAY);
}

void I2cBus::unlock() {
  if (lock_ != nullptr) {
    xSemaphoreGiveRecursive(lock_);
  }
}

I2cBus::~I2cBus() { end(); }

bool I2cBus::begin(int sda, int scl, uint32_t freq_hz, int port) {
  Guard g(*this);
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
  // OJO: begin() llama a end() con el cerrojo ya tomado. Por eso es recursivo.
  Guard g(*this);
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
  Guard g(*this);
  if (bus_ == nullptr) {
    return false;
  }
  return i2c_master_probe(bus_, addr, timeout_ms) == ESP_OK;
}

bool I2cBus::write(uint8_t addr, const uint8_t *data, size_t len,
                   int timeout_ms) {
  Guard g(*this);
  i2c_master_dev_handle_t dev = deviceFor(addr);
  if (dev == nullptr) {
    return false;
  }
  return i2c_master_transmit(dev, data, len, timeout_ms) == ESP_OK;
}

bool I2cBus::read(uint8_t addr, uint8_t *buf, size_t len, int timeout_ms) {
  Guard g(*this);
  i2c_master_dev_handle_t dev = deviceFor(addr);
  if (dev == nullptr) {
    return false;
  }
  return i2c_master_receive(dev, buf, len, timeout_ms) == ESP_OK;
}

bool I2cBus::writeRead(uint8_t addr, const uint8_t *out, size_t out_len,
                       uint8_t *in, size_t in_len, int timeout_ms) {
  Guard g(*this);
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
  lock(); // se suelta en endTransmission(true) o en requestFrom()
  TaskHandle_t self = xTaskGetCurrentTaskHandle();
  if (compat_owner_ == self) {
    // Esta misma tarea dejo una transaccion sin cerrar (ver compat_owner_ en
    // la cabecera). La toma huerfana es suya: se devuelve ahora, no se filtra.
    pending_restart_ = false;
    unlock();
  }
  compat_owner_ = self;
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
    // Sin beginTransmission() previo NO tenemos el cerrojo, asi que no se
    // puede soltar aqui: seria devolver la toma de otra tarea.
    return 4; // "otro error", igual que Wire
  }
  tx_open_ = false;

  if (!sendStop) {
    // START repetido: NO se envia nada todavia. Lo resolvera el requestFrom()
    // siguiente en una sola transaccion escritura+lectura, que es exactamente
    // lo que veia el bus con Arduino. EL CERROJO SE QUEDA TOMADO hasta
    // entonces: entre la escritura y la lectura no puede colarse otra tarea,
    // que es precisamente lo que significa un START repetido.
    pending_restart_ = true;
    return 0; // compat_owner_ sigue siendo esta tarea: el cerrojo no se suelta
  }

  pending_restart_ = false;
  compat_owner_ = nullptr;
  uint8_t rc;
  if (tx_len_ == 0) {
    // endTransmission() sin datos era el sondeo de presencia de Arduino.
    //
    // Aqui SI se aplica setTimeOut(). El unico punto del firmware que lo
    // llama (initRoomSensor) lo hace por el coste de sondear una direccion
    // que NO contesta, no por el de una lectura; con el sensor de aire
    // desconectado ese sondeo se repite en cada ciclo de reconexion. Las
    // TRANSFERENCIAS DE DATOS se quedan con el plazo por defecto del driver a
    // proposito: bajarlas a 10 ms convertiria una conversion lenta del STS35
    // en una lectura de aire perdida, que es justo lo que no puede fallar.
    rc = probe(tx_addr_, timeout_ms_) ? 0 : 2; // 2 = NACK a la direccion
  } else {
    rc = write(tx_addr_, tx_buf_, tx_len_) ? 0 : 2;
    tx_len_ = 0;
  }
  unlock();
  return rc;
}

uint8_t I2cBus::requestFrom(uint8_t addr, uint8_t len, bool sendStop) {
  (void)sendStop;
  // Si veniamos de un endTransmission(false) YA tenemos el cerrojo; si no, se
  // toma aqui. En los dos casos se sale de esta funcion sin tenerlo.
  const bool held = (compat_owner_ == xTaskGetCurrentTaskHandle());
  if (!held) {
    lock();
  }
  compat_owner_ = nullptr;

  rx_len_ = 0;
  rx_pos_ = 0;
  if (len == 0 || len > kBufSize) {
    pending_restart_ = false;
    unlock();
    return 0;
  }

  bool ok;
  if (pending_restart_ && tx_len_ > 0 && addr == tx_addr_) {
    ok = writeRead(addr, tx_buf_, tx_len_, rx_buf_, len);
    tx_len_ = 0;
    pending_restart_ = false;
  } else {
    pending_restart_ = false;
    ok = read(addr, rx_buf_, len);
  }
  rx_owner_ = ok ? xTaskGetCurrentTaskHandle() : nullptr;
  unlock();
  if (!ok) {
    return 0;
  }
  rx_len_ = len;
  return len;
}

// available()/read()/peek() solo sirven bytes a la tarea que los pidio; ver
// rx_owner_ en la cabecera.
int I2cBus::available() {
  if (rx_owner_ != xTaskGetCurrentTaskHandle()) {
    return 0;
  }
  return static_cast<int>(rx_len_ - rx_pos_);
}

int I2cBus::read() {
  if (rx_owner_ != xTaskGetCurrentTaskHandle() || rx_pos_ >= rx_len_) {
    return -1;
  }
  return rx_buf_[rx_pos_++];
}

int I2cBus::peek() {
  if (rx_owner_ != xTaskGetCurrentTaskHandle() || rx_pos_ >= rx_len_) {
    return -1;
  }
  return rx_buf_[rx_pos_];
}
