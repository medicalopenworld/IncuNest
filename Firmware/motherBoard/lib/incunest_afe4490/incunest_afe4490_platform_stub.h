#pragma once
// incunest_afe4490_platform_stub.h — Arduino + FreeRTOS type stubs for native (offline) builds
// Library version: v0.18 — native/offline (no hardware)
// Spec: incunest_afe4490_spec.md §9
// Author: Medical Open World — http://medicalopenworld.org — <contact@medicalopenworld.org>

#include <cstdint>
#include <cmath>    // cosf, sinf, sqrtf, expf, fabsf, fmaxf, fminf, roundf, tanf
#include <cstring>  // memset

// ── Integer types (Arduino aliases) ──────────────────────────────────────────
using uint8_t  = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using uint64_t = std::uint64_t;
using int8_t   = std::int8_t;
using int16_t  = std::int16_t;
using int32_t  = std::int32_t;
using int64_t  = std::int64_t;

// ── FreeRTOS types ────────────────────────────────────────────────────────────
using SemaphoreHandle_t = void*;
using QueueHandle_t     = void*;
using TaskHandle_t      = void*;
using TickType_t        = uint32_t;
using BaseType_t        = int;
using UBaseType_t       = unsigned int;

#define portMAX_DELAY   ((TickType_t)0xFFFFFFFFUL)
#define pdTRUE          ((BaseType_t)1)
#define pdFALSE         ((BaseType_t)0)

// FreeRTOS stub functions — never called (all paths guarded by _initialized == false)
inline SemaphoreHandle_t xSemaphoreCreateMutex()                                         { return nullptr; }
inline SemaphoreHandle_t xSemaphoreCreateBinary()                                        { return nullptr; }
inline BaseType_t        xSemaphoreTake(SemaphoreHandle_t, TickType_t)                   { return pdTRUE;  }
inline BaseType_t        xSemaphoreGive(SemaphoreHandle_t)                               { return pdTRUE;  }
inline void              vSemaphoreDelete(SemaphoreHandle_t)                             {}
inline QueueHandle_t     xQueueCreate(unsigned, unsigned)                                { return nullptr; }
inline BaseType_t        xQueueSend(QueueHandle_t, const void*, TickType_t)              { return pdFALSE; }
inline BaseType_t        xQueueReceive(QueueHandle_t, void*, TickType_t)                 { return pdFALSE; }
inline void              vQueueDelete(QueueHandle_t)                                     {}
inline void              vTaskDelete(TaskHandle_t)                                       {}
inline uint32_t          uxTaskGetStackHighWaterMark(TaskHandle_t)                       { return 0; }

// ── Arduino helpers ───────────────────────────────────────────────────────────
#define IRAM_ATTR

template<typename T>
inline T constrain(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

// roundf is already in <cmath> as std::roundf; bring it to global scope
using std::roundf;
