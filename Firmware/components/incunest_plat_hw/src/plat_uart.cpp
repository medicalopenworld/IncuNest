#include "platform/plat_uart.h"

#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "plat_uart";

HardwareSerial Serial(UART_NUM_0);
HardwareSerial Serial1(UART_NUM_1);
HardwareSerial Serial2(UART_NUM_2);

// Traduce el valor de configuracion de Arduino (SERIAL_8N1 = 0x800001c...) a
// los campos del driver. Los bits vienen del esp32-hal-uart.h de Arduino:
//   bits 2-3: datos (0=5,1=6,2=7,3=8)   bits 4-5: stop (1=1, 3=2)
//   bits 0-1: paridad (0=ninguna, 2=par, 3=impar)
static void decode_config(uint32_t config, uart_config_t *out) {
  switch ((config >> 2) & 0x3) {
  case 0: out->data_bits = UART_DATA_5_BITS; break;
  case 1: out->data_bits = UART_DATA_6_BITS; break;
  case 2: out->data_bits = UART_DATA_7_BITS; break;
  default: out->data_bits = UART_DATA_8_BITS; break;
  }
  out->stop_bits = ((config >> 4) & 0x3) == 3 ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
  switch (config & 0x3) {
  case 2: out->parity = UART_PARITY_EVEN; break;
  case 3: out->parity = UART_PARITY_ODD; break;
  default: out->parity = UART_PARITY_DISABLE; break;
  }
}

HardwareSerial::HardwareSerial(int uart_num) : uart_num_(uart_num) {}

HardwareSerial::~HardwareSerial() { end(); }

void HardwareSerial::begin(unsigned long baud, uint32_t config, int rxPin,
                           int txPin) {
  baud_ = baud;
  config_ = config;
  rx_pin_ = rxPin;
  tx_pin_ = txPin;
  reinstall();
}

void HardwareSerial::reinstall() {
  if (installed_) {
    uart_driver_delete(static_cast<uart_port_t>(uart_num_));
    installed_ = false;
  }

  uart_config_t cfg = {};
  cfg.baud_rate = static_cast<int>(baud_);
  cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;
  decode_config(config_, &cfg);

  const uart_port_t port = static_cast<uart_port_t>(uart_num_);

  // El driver exige que el anillo de RX sea mayor que el FIFO hardware
  // (128 B). Arduino redondeaba igual; aqui se hace explicito.
  size_t rx = rx_buf_ > UART_HW_FIFO_LEN(port) ? rx_buf_ : UART_HW_FIFO_LEN(port) + 1;
  size_t tx = tx_buf_;
  if (tx != 0 && tx <= UART_HW_FIFO_LEN(port)) {
    tx = UART_HW_FIFO_LEN(port) + 1;
  }

  esp_err_t err = uart_driver_install(port, static_cast<int>(rx),
                                      static_cast<int>(tx), 0, nullptr, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_driver_install(%d) -> %s", uart_num_, esp_err_to_name(err));
    return;
  }
  installed_ = true;

  err = uart_param_config(port, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_param_config(%d) -> %s", uart_num_, esp_err_to_name(err));
  }

  // Pines: -1 mantiene los de defecto del chip para ese UART, que es lo que
  // hacia Arduino con Serial.begin(baud) a secas.
  err = uart_set_pin(port, tx_pin_ >= 0 ? tx_pin_ : UART_PIN_NO_CHANGE,
                     rx_pin_ >= 0 ? rx_pin_ : UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uart_set_pin(%d, tx=%d, rx=%d) -> %s", uart_num_, tx_pin_,
             rx_pin_, esp_err_to_name(err));
  }

  if (uart_num_ == UART_NUM_0) {
    // La consola comparte UART0 con el protocolo (decision documentada en la
    // cabecera). Que stdout/ESP_LOG escriban a traves de este mismo driver
    // hace que cada escritura salga entera, sin mezclarse byte a byte con las
    // tramas del protocolo. Arduino hacia lo mismo al instalar su Serial.
    uart_vfs_dev_use_driver(UART_NUM_0);
  }
}

void HardwareSerial::end() {
  if (!installed_) {
    return;
  }
  if (uart_num_ == UART_NUM_0) {
    uart_vfs_dev_use_nonblocking(UART_NUM_0);
  }
  uart_driver_delete(static_cast<uart_port_t>(uart_num_));
  installed_ = false;
  peeked_ = -1;
}

void HardwareSerial::setRxBufferSize(size_t bytes) {
  rx_buf_ = bytes;
  if (installed_) {
    reinstall();
  }
}

void HardwareSerial::setTxBufferSize(size_t bytes) {
  tx_buf_ = bytes;
  if (installed_) {
    reinstall();
  }
}

int HardwareSerial::available() {
  if (!installed_) {
    return 0;
  }
  size_t n = 0;
  uart_get_buffered_data_len(static_cast<uart_port_t>(uart_num_), &n);
  return static_cast<int>(n) + (peeked_ >= 0 ? 1 : 0);
}

int HardwareSerial::read() {
  if (peeked_ >= 0) {
    const int c = peeked_;
    peeked_ = -1;
    return c;
  }
  if (!installed_) {
    return -1;
  }
  uint8_t b = 0;
  // Sin espera: igual que Serial.read() de Arduino, devuelve -1 si no hay
  // nada. Quien quiera esperar usa readBytes()/readStringUntil() de Stream.
  const int n = uart_read_bytes(static_cast<uart_port_t>(uart_num_), &b, 1, 0);
  return n == 1 ? b : -1;
}

int HardwareSerial::peek() {
  if (peeked_ < 0) {
    peeked_ = read();
  }
  return peeked_;
}

size_t HardwareSerial::write(uint8_t b) { return write(&b, 1); }

size_t HardwareSerial::write(const uint8_t *buf, size_t len) {
  if (!installed_ || buf == nullptr) {
    return 0;
  }
  const int n = uart_write_bytes(static_cast<uart_port_t>(uart_num_), buf, len);
  return n < 0 ? 0 : static_cast<size_t>(n);
}

void HardwareSerial::flush() {
  if (!installed_) {
    return;
  }
  uart_wait_tx_done(static_cast<uart_port_t>(uart_num_), pdMS_TO_TICKS(100));
}
