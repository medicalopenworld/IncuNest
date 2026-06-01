#include "comm_module.h"
#include "main.h"
#include "GPRS.h"
#include "Wifi_OTA.h"
#include "CommTask.h"

// ── GPRS ─────────────────────────────────────────────────────────────────────
void comm_module_gprs_init(void)    { initGPRS(); GPRS_TB_Init(); }
void comm_module_gprs_handler(void) { GPRS_Handler(); }
bool comm_module_gprs_connected(void) { return GPRSIsConnectedToServer(); }

// ── WiFi / OTA ────────────────────────────────────────────────────────────────
void comm_module_wifi_init(void)    { WIFI_TB_Init(); }
void comm_module_wifi_handler(void) { WifiOTAHandler(); }
bool comm_module_wifi_connected(void) { return WIFIIsConnectedToServer(); }

// ── MB↔HMI serial link ───────────────────────────────────────────────────────
void comm_module_hmi_init(void)            { CommunicationHost_Init(); }
void comm_module_hmi_task(void *pv)        { Communication_Task(pv); }
void comm_module_hmi_send(const char *msg) { CommunicationHost_Send(msg); }
