#pragma once
#ifdef USE_IDF_FRAMEWORK

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "driver/uart.h"
#include <cstring>
#include <cstdio>
#include <string>

static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
static inline uint32_t micros() {
    return (uint32_t)(esp_timer_get_time());
}
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static inline bool psramFound() {
    return esp_psram_is_initialized();
}

// PROGMEM is a no-op on IDF (all data is in regular flash or RAM)
#ifndef PROGMEM
#define PROGMEM
#endif

// ---- Serial-like UART wrapper for CommTask ----
class UARTSerialClass {
    static const uart_port_t PORT = UART_NUM_0;
    bool _begun = false;
public:
    void begin(uint32_t baud) {
        if (_begun) return;
        _begun = true;
        uart_config_t cfg = {};
        cfg.baud_rate  = (int)baud;
        cfg.data_bits  = UART_DATA_8_BITS;
        cfg.parity     = UART_PARITY_DISABLE;
        cfg.stop_bits  = UART_STOP_BITS_1;
        cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
        uart_param_config(PORT, &cfg);
        uart_set_pin(PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(PORT, 512, 0, 0, NULL, 0);
    }
    void print(const char *s)  { if (s) uart_write_bytes(PORT, s, strlen(s)); }
    void print(int n)           { char b[20]; snprintf(b, sizeof(b), "%d", n); print(b); }
    void println(const char *s) { print(s); print("\r\n"); }
    void println(int n)         { char b[20]; snprintf(b, sizeof(b), "%d\r\n", n); print(b); }
    template<typename... Args>
    void printf(const char *fmt, Args... args) {
        char buf[256];
        snprintf(buf, sizeof(buf), fmt, args...);
        uart_write_bytes(PORT, buf, strlen(buf));
    }
    int available() {
        size_t len = 0;
        uart_get_buffered_data_len(PORT, &len);
        return (int)len;
    }
    int read() {
        uint8_t c = 0;
        return (uart_read_bytes(PORT, &c, 1, pdMS_TO_TICKS(1)) > 0) ? c : -1;
    }
};

// Single instance — CommTask uses this as COMM_SERIAL
extern UARTSerialClass Serial;

// ---- Math shims (not in IDF without explicit include) ----
#include <cmath>  // round(), roundf(), isnan()

// constrain — Arduino macro, not in IDF
template<typename T>
static inline T constrain(T x, T lo, T hi) {
    return (x < lo) ? lo : (x > hi ? hi : x);
}

// ---- ESP object shim ----
class ESPClass {
public:
    void     restart()      const { esp_restart(); }
    uint32_t getFreeHeap()  const { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
    uint32_t getPsramSize() const { return (uint32_t)esp_psram_get_size(); }
};
extern ESPClass ESP;

#endif // USE_IDF_FRAMEWORK
