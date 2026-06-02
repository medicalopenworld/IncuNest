#pragma once

void hmi_comm_module_init(void);
void hmi_comm_module_request_state(void);
void hmi_comm_module_send_boot_info(void);
void hmi_comm_module_send_wifi_credentials(const char *ssid, const char *pass);
