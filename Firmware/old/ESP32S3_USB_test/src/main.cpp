#include "Arduino.h"
#include <stdio.h>
#include <string.h>

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

static const char *TAG = "MASTER_USB";

// Handle to the CH340 device
static std::unique_ptr<CdcAcmDevice> vcp;

// RX buffer
static char rxBuffer[256];
static int rxIndex = 0;

// Disconnect semaphore
static SemaphoreHandle_t device_disconnected_sem;


// =====================================================
//                   PARSER (TEXT PROTOCOL)
// =====================================================
void parse_line(const char *line)
{
    ESP_LOGI(TAG, "RX LINE: %s", line);

    // -------------------------
    // CTRL,TEL
    // -------------------------
    if (strncmp(line, "CTRL,TEL", 8) == 0)
    {
        double air, skin;
        int hum;
        int r = sscanf(line, "CTRL,TEL,%lf,%lf,%d", &air, &skin, &hum);

        if (r == 3)
        {
            ESP_LOGI(TAG, "TEL OK: air=%.1f skin=%.1f hum=%d",
                     air, skin, hum);
        }
        else
        {
            ESP_LOGE(TAG, "ERROR parsing CTRL,TEL");
        }
        return;
    }

    // -------------------------
    // CTRL,ALM
    // -------------------------
    if (strncmp(line, "CTRL,ALM", 8) == 0)
    {
        int id, stateInt;
        char type[32];
        char desc[128];

        int r = sscanf(line, "CTRL,ALM,%d,%[^,],%[^,],%d",
                       &id, type, desc, &stateInt);

        if (r == 4)
        {
            ESP_LOGI(TAG, "ALARM OK: id=%d type=%s desc=%s state=%d",
                     id, type, desc, stateInt);
        }
        else
        {
            ESP_LOGE(TAG, "ERROR parsing CTRL,ALM");
        }
        return;
    }

    // -------------------------
    // HMI,<...>
    // -------------------------
    if (strncmp(line, "HMI,", 4) == 0)
    {
        int act, mode, photo, mute;
        double air, skin, hum;

        int r = sscanf(line, "HMI,%d,%d,%lf,%lf,%lf,%d,%d",
                       &act, &mode, &air, &skin, &hum, &photo, &mute);

        if (r == 7)
        {
            ESP_LOGI(TAG, "HMI CMD OK: act=%d mode=%d", act, mode);
        }
        else
        {
            ESP_LOGE(TAG, "ERROR parsing HMI");
        }
        return;
    }

    ESP_LOGW(TAG, "UNKNOWN MESSAGE: %s", line);
}



// =====================================================
//        USB RX CALLBACK (receives raw bytes)
// =====================================================
static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    for (size_t i = 0; i < data_len; i++)
    {
        char c = data[i];

        if (c == '\r')
            continue;

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



// =====================================================
//                  USB EVENT HANDLER
// =====================================================
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Device disconnected");
        xSemaphoreGive(device_disconnected_sem);
    }
}



// =====================================================
//              USB HOST BACKGROUND TASK
// =====================================================
static void usb_lib_task(void *arg)
{
    while (1)
    {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}



// =====================================================
//                      SETUP()
// =====================================================
void setup()
{
    ESP_LOGI(TAG, "Starting USB HOST + TEXT MODE");

    device_disconnected_sem = xSemaphoreCreateBinary();

    // --------------------------------------------
    // Install USB Host
    // --------------------------------------------
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Background task
    xTaskCreate(usb_lib_task, "usb_lib", 4096, NULL, 10, NULL);

    // CDC Host driver
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    // Register USB-UART drivers (CH340, CP210x, FTDI)
    VCP::register_driver<CH34x>();
    VCP::register_driver<CP210x>();
    VCP::register_driver<FT23x>();


    // --------------------------------------------
    // WAIT FOR HMI (CH340)
    // --------------------------------------------
    while (true)
    {
        const cdc_acm_host_device_config_t dev_config = {
            .connection_timeout_ms = 5000,
            .out_buffer_size = 512,
            .in_buffer_size = 512,
            .event_cb = handle_event,
            .data_cb = handle_rx,
            .user_arg = NULL,
        };

        ESP_LOGI(TAG, "Waiting for HMI...");
        vcp = std::unique_ptr<CdcAcmDevice>(VCP::open(&dev_config));

        if (!vcp)
        {
            ESP_LOGW(TAG, "No device detected, retrying...");
            continue;
        }

        ESP_LOGI(TAG, "USB device connected!");
        break;
    }


    // --------------------------------------------
    // CRITICAL: ENABLE DTR/RTS or CH340C WILL NOT SEND DATA
    // --------------------------------------------
    ESP_LOGI(TAG, "Enabling DTR/RTS...");
    vTaskDelay(20);
    vcp->set_control_line_state(true, true);  // <---- REQUIRED


    // --------------------------------------------
    // OPTIONAL BUT IMPORTANT: SET LINE CODING
    // --------------------------------------------
    cdc_acm_line_coding_t line_coding = {
        .dwDTERate = 115200,
        .bCharFormat = 0,  // 1 stop bit
        .bParityType = 0,  // no parity
        .bDataBits = 8
    };
    vcp->line_coding_set(&line_coding);

    ESP_LOGI(TAG, "CH340 ready to receive data");
}



// =====================================================
//                        LOOP()
// =====================================================
void loop()
{
    // Example: send one message per second
    static const char *msg = "CTRL,TEL,23.4,21.9,48\n";

    if (vcp)
    {
        vcp->tx_blocking(
            reinterpret_cast<uint8_t *>(const_cast<char *>(msg)),
            strlen(msg)
        );
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}
