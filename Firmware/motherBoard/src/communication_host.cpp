#include "communication_host.h"

#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "usb/vcp.hpp"
#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"

using namespace esp_usb;

static const char *TAG = "COMM_HOST";

// ======================================================
//  GLOBAL DATA
// ======================================================
TelemetryMessage ctrl_tel_msg = {0,0,0};
HMI_CommandMessage hmi_cmd_msg = {0,0,0,0,0,0,0,false};

static std::unique_ptr<CdcAcmDevice> vcp;
static char rxBuffer[256];
static int rxIndex = 0;

static SemaphoreHandle_t device_disconnected_sem;


// ======================================================
//  PARSER
// ======================================================
void parse_line(const char *line)
{
    ESP_LOGI(TAG, "RX: %s", line);

    // -----------------------------
    // CTRL,TEL
    // -----------------------------
    if (strncmp(line, "CTRL,TEL", 8) == 0)
    {
        double air, skin;
        int hum;

        if (sscanf(line, "CTRL,TEL,%lf,%lf,%d", &air, &skin, &hum) == 3)
        {
            ctrl_tel_msg.detectedAirTemperature = air;
            ctrl_tel_msg.detectedSkinTemperature = skin;
            ctrl_tel_msg.detectedHumidity = hum;

            ESP_LOGI(TAG, "TEL OK air=%.1f skin=%.1f hum=%d", air, skin, hum);
        }
        else ESP_LOGE(TAG, "TEL parse error");
        return;
    }

    // -----------------------------
    // CTRL,ALM
    // -----------------------------
    if (strncmp(line, "CTRL,ALM", 8) == 0)
    {
        ESP_LOGW(TAG, "ALARM: %s", line);
        return;
    }

    // -----------------------------
    // HMI COMMAND
    // -----------------------------
    if (strncmp(line, "HMI,", 4) == 0)
    {
        int act, mode, photo, mute;
        double air, skin, hum;

        if (sscanf(line, "HMI,%d,%d,%lf,%lf,%lf,%d,%d",
                   &act, &mode, &air, &skin, &hum, &photo, &mute) == 7)
        {
            hmi_cmd_msg.actuation = act;
            hmi_cmd_msg.controlMode = mode;
            hmi_cmd_msg.desiredAirTemperature = air;
            hmi_cmd_msg.desiredSkinTemperature = skin;
            hmi_cmd_msg.desiredHumidity = hum;
            hmi_cmd_msg.phototherapyMode = photo;
            hmi_cmd_msg.muteAlarm = mute;
            hmi_cmd_msg.newCommand = true;

            ESP_LOGI(TAG, "HMI CMD stored successfully");
        }
        else ESP_LOGE(TAG, "HMI parse error");

        return;
    }

    ESP_LOGW(TAG, "Unknown line: %s", line);
}


// ======================================================
//  USB RX CALLBACK
// ======================================================
static bool handle_rx(const uint8_t *data, size_t len, void *arg)
{
    for (size_t i = 0; i < len; i++)
    {
        char c = data[i];
        if (c == '\r') continue;

        if (c == '\n')
        {
            rxBuffer[rxIndex] = 0;
            parse_line(rxBuffer);
            rxIndex = 0;
            continue;
        }

        if (rxIndex < sizeof(rxBuffer)-1)
            rxBuffer[rxIndex++] = c;
    }
    return true;
}


// ======================================================
//  USB EVENT (disconnect)
// ======================================================
static void handle_event(const cdc_acm_host_dev_event_data_t *event,
                         void *user_ctx)
{
    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED)
    {
        ESP_LOGW(TAG, "HMI disconnected");
        xSemaphoreGive(device_disconnected_sem);
    }
}


// ======================================================
//  USB host service task
// ======================================================
static void usb_lib_task(void *arg)
{
    while (1)
    {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
    }
}


// ======================================================
//  SEND DATA TO HMI
// ======================================================
void CommunicationHost_Send(const char *msg)
{
    if (!vcp) return;

    size_t len = strlen(msg);
    static uint8_t buf[256];

    if (len >= sizeof(buf))
    {
        ESP_LOGE(TAG, "TX too long");
        return;
    }

    memcpy(buf, msg, len);
    vcp->tx_blocking(buf, len);
}


// ======================================================
//  INITIALIZATION (CALL ONCE IN setup())
// ======================================================
void CommunicationHost_Init()
{
    device_disconnected_sem = xSemaphoreCreateBinary();

    const usb_host_config_t cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&cfg));

    xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);

    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    VCP::register_driver<CH34x>();
    VCP::register_driver<CP210x>();
    VCP::register_driver<FT23x>();
}


// ======================================================
//  COMMUNICATION TASK
// ======================================================
void Communication_Task(void *pvParameters)
{
    while (true)
    {
        const cdc_acm_host_device_config_t dev = {
            .connection_timeout_ms = 4000,
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .event_cb = handle_event,
            .data_cb = handle_rx,
            .user_arg = NULL,
        };

        ESP_LOGI(TAG, "Waiting for HMI...");
        vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev));

        if (!vcp)
        {
            ESP_LOGW(TAG, "HMI not found");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        ESP_LOGI(TAG, "HMI connected!");

        // Enable DTR/RTS (CH340C requirement)
        vcp->set_control_line_state(true, true);
        vTaskDelay(20);

        // Line coding
        cdc_acm_line_coding_t line = {
            .dwDTERate = 115200,
            .bCharFormat = 0,
            .bParityType = 0,
            .bDataBits = 8
        };
        vcp->line_coding_set(&line);

        // COMM LOOP
        while (vcp)
        {
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "CTRL,TEL,%.1f,%.1f,%d\n",
                     ctrl_tel_msg.detectedAirTemperature,
                     ctrl_tel_msg.detectedSkinTemperature,
                     (int)ctrl_tel_msg.detectedHumidity);

            vcp->tx_blocking((uint8_t *)msg, strlen(msg));

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
