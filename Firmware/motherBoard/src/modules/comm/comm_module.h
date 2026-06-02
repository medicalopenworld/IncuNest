#pragma once
#include <stdbool.h>

// GPRS
void comm_module_gprs_init(void);
void comm_module_gprs_handler(void);
bool comm_module_gprs_connected(void);

// WiFi / OTA
void comm_module_wifi_init(void);
void comm_module_wifi_handler(void);
bool comm_module_wifi_connected(void);

// MB↔HMI serial link
void comm_module_hmi_init(void);
void comm_module_hmi_task(void *pv);
void comm_module_hmi_send(const char *msg);
