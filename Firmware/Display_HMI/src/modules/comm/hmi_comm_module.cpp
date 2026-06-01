#include "hmi_comm_module.h"
#include "CommTask.h"

void hmi_comm_module_init(void)                    { CreateCommTask(); }
void hmi_comm_module_request_state(void)           { Communication_RequestState(); }
void hmi_comm_module_send_boot_info(void)          { Communication_SendBootInfo(); }
void hmi_comm_module_send_wifi_credentials(const char *ssid, const char *pass) {
  Communication_SendWiFiCredentials(ssid, pass);
}
