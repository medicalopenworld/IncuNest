#include "sb_camera_sensor.h"
#include "sb_cam_builder.h"
#include "sb_env_i2c.h"
#include "sensorBoard_cmd_registry.h"
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_status.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "CAM";

/* Pinout DVP (OV2640/OV5640, autodetección por SCCB): docs/hardware.md.
 * d0..d7 = Y2..Y9. RESET no cableado. */
#define SB_CAM_PIN_PWDN 13
#define SB_CAM_PIN_RESET (-1)
#define SB_CAM_PIN_XCLK 16
#define SB_CAM_PIN_D0 8  /* Y2 */
#define SB_CAM_PIN_D1 10 /* Y3 */
#define SB_CAM_PIN_D2 11 /* Y4 */
#define SB_CAM_PIN_D3 9  /* Y5 */
#define SB_CAM_PIN_D4 6  /* Y6 */
#define SB_CAM_PIN_D5 7  /* Y7 */
#define SB_CAM_PIN_D6 17 /* Y8 */
#define SB_CAM_PIN_D7 15 /* Y9 */
#define SB_CAM_PIN_VSYNC 12
#define SB_CAM_PIN_HREF 18
#define SB_CAM_PIN_PCLK 21

/* SCCB: comparte el bus I2C principal (IO4/IO5) creado por env_sensors —
 * sccb-ng lo obtiene por número de puerto (I2C_NUM_1) */
#define SB_CAM_SCCB_PORT 1

/* Prio 3: por debajo de sensores (4) y transporte (5) — la captura es la
 * carga menos urgente del firmware */
#define SB_CAM_TASK_PRIO 3
#define SB_CAM_TASK_STACK 4096

/* Si una captura tarda más que esto, la cámara está colgada (DVP sin PCLK,
 * SCCB caído): el gate responde el fallo real en vez de "busy" eterno */
#define SB_CAM_STALL_MS 10000

static TaskHandle_t s_cam_task = NULL;
/* Escritos por el handler (contexto usb_rx, único productor); leídos/
 * limpiados por camera_task */
static volatile uint32_t s_pending_id = 0;
static volatile bool s_busy = false;
static volatile uint32_t s_busy_since_ms = 0;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* noblock: se invoca también desde el contexto usb_rx (handler) */
static void send_capture_err(uint32_t id, const char *msg)
{
    char buf[SB_PROTO_MAX_JSON_PAYLOAD];
    if (sb_cam_build_capture_err(buf, sizeof(buf), id, msg, now_ms()) > 0) {
        sensorBoard_comm_send_json_noblock(buf);
    }
}

/* Handler de "capture": contexto usb_rx — solo valida y notifica (contrato
 * de sensorBoard_cmd_registry.h) */
static void capture_cmd_handler(uint32_t id)
{
    uint32_t elapsed = s_busy ? (now_ms() - s_busy_since_ms) : 0;

    switch (sb_cam_gate(s_cam_task != NULL, s_busy, elapsed, SB_CAM_STALL_MS)) {
    case SB_CAM_GATE_NOT_READY:
        send_capture_err(id, "camera not ready");
        break;
    case SB_CAM_GATE_BUSY:
        send_capture_err(id, "busy");
        break;
    case SB_CAM_GATE_STALLED:
        /* Fallo real, no "busy" eterno: la cámara quedó colgada en fb_get */
        send_capture_err(id, "camera stalled");
        sensorBoard_status_set_sensor("cam", false);
        break;
    case SB_CAM_GATE_ACCEPT:
        s_busy = true;
        s_busy_since_ms = now_ms();
        s_pending_id = id;
        xTaskNotifyGive(s_cam_task);
        break;
    }
}

static void camera_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint32_t id = s_pending_id;

        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL || fb->len == 0) {
            if (fb != NULL) {
                esp_camera_fb_return(fb);
            }
            send_capture_err(id, "capture failed");
            sensorBoard_status_set_sensor("cam", false);
            s_busy = false;
            continue;
        }

        char buf[SB_PROTO_MAX_JSON_PAYLOAD];
        if (sb_cam_build_capture_ok(buf, sizeof(buf), id, fb->len, now_ms()) > 0) {
            sensorBoard_comm_send_json(buf);
        }

        esp_err_t err = sensorBoard_comm_send_binary(SB_PROTO_TYPE_JPEG, fb->buf, fb->len);
        /* send_binary copia el frame a PSRAM: el fb vuelve al driver ya.
         * ESP_ERR_TIMEOUT = binario anterior sin drenar (cota en vuelo=1). */
        esp_camera_fb_return(fb);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "jpeg frame dropped (%d)", (int)err);
        }
        sensorBoard_status_set_sensor("cam", err == ESP_OK);
        s_busy = false;
    }
}

/* Diagnóstico cuando el probe falla con NOT_SUPPORTED: algo ACKa en una
 * dirección de cámara pero su ID no coincide con ningún driver. Arranca el
 * XCLK (la SCCB de los OV no responde sin reloj), lee los registros de
 * chip-ID crudos de 0x30 (OV2640: banco 0xFF=1, regs 0x0A/0x0B ⇒ 0x26/0x4x)
 * y 0x3C (OV5640: regs 0x300A/0x300B ⇒ 0x56/0x40) y los loguea. */
static void cam_sccb_diag(void)
{
    i2c_master_bus_handle_t bus = sb_env_get_main_i2c_bus();
    if (bus == NULL) {
        return;
    }

    const ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CONFIG_SB_CAM_XCLK_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t ccfg = {
        .gpio_num = SB_CAM_PIN_XCLK,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1,
        .hpoint = 0,
    };
    if (ledc_timer_config(&tcfg) != ESP_OK || ledc_channel_config(&ccfg) != ESP_OK) {
        return;
    }
    /* PWDN bajo = sensor encendido (convención esp32-camera) */
    gpio_set_direction(SB_CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
    gpio_set_level(SB_CAM_PIN_PWDN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    const struct {
        uint8_t addr;
        const char *name;
    } cams[] = { { 0x30, "OV2640" }, { 0x3C, "OV5640" } };

    for (size_t i = 0; i < 2; i++) {
        if (i2c_master_probe(bus, cams[i].addr, 50) != ESP_OK) {
            ESP_LOGW(TAG, "diag: 0x%02X (%s) no ACKa", cams[i].addr, cams[i].name);
            continue;
        }
        i2c_device_config_t dcfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = cams[i].addr,
            .scl_speed_hz = 100000,
        };
        i2c_master_dev_handle_t dev = NULL;
        if (i2c_master_bus_add_device(bus, &dcfg, &dev) != ESP_OK) {
            continue;
        }
        uint8_t pid[2] = { 0xEE, 0xEE };
        if (cams[i].addr == 0x30) {
            const uint8_t bank[] = { 0xFF, 0x01 };
            i2c_master_transmit(dev, bank, 2, 100);
            const uint8_t r0a = 0x0A, r0b = 0x0B;
            i2c_master_transmit_receive(dev, &r0a, 1, &pid[0], 1, 100);
            i2c_master_transmit_receive(dev, &r0b, 1, &pid[1], 1, 100);
        } else {
            const uint8_t ra[] = { 0x30, 0x0A };
            const uint8_t rb[] = { 0x30, 0x0B };
            i2c_master_transmit_receive(dev, ra, 2, &pid[0], 1, 100);
            i2c_master_transmit_receive(dev, rb, 2, &pid[1], 1, 100);
        }
        ESP_LOGW(TAG, "diag: 0x%02X ACKa — chip-ID crudo: 0x%02X 0x%02X (%s esperaria %s)",
                 cams[i].addr, pid[0], pid[1], cams[i].name,
                 (cams[i].addr == 0x30) ? "0x26 0x4x" : "0x56 0x40");
        i2c_master_bus_rm_device(dev);
    }

    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
}

esp_err_t sb_camera_sensor_init(void)
{
    if (s_cam_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;

    /* El bus lo crea env_sensors: sin él no hay SCCB (y no debemos crear
     * un segundo bus en el mismo puerto) */
    if (sb_env_get_main_i2c_bus() == NULL) {
        ESP_LOGW(TAG, "main I2C bus not available — camera disabled");
        err = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    camera_config_t cfg = {
        .pin_pwdn = SB_CAM_PIN_PWDN,
        .pin_reset = SB_CAM_PIN_RESET,
        .pin_xclk = SB_CAM_PIN_XCLK,
        .pin_sccb_sda = -1, /* -1: usar sccb_i2c_port ya inicializado */
        .pin_sccb_scl = -1,
        .pin_d7 = SB_CAM_PIN_D7,
        .pin_d6 = SB_CAM_PIN_D6,
        .pin_d5 = SB_CAM_PIN_D5,
        .pin_d4 = SB_CAM_PIN_D4,
        .pin_d3 = SB_CAM_PIN_D3,
        .pin_d2 = SB_CAM_PIN_D2,
        .pin_d1 = SB_CAM_PIN_D1,
        .pin_d0 = SB_CAM_PIN_D0,
        .pin_vsync = SB_CAM_PIN_VSYNC,
        .pin_href = SB_CAM_PIN_HREF,
        .pin_pclk = SB_CAM_PIN_PCLK,
        .xclk_freq_hz = CONFIG_SB_CAM_XCLK_HZ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = CONFIG_SB_CAM_JPEG_QUALITY,
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .sccb_i2c_port = SB_CAM_SCCB_PORT,
    };

    err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_camera_init failed (%d)", (int)err);
        if (err == ESP_ERR_NOT_SUPPORTED) {
            cam_sccb_diag(); /* algo ACKa pero el ID no coincide: volcarlo */
        }
        goto fail;
    }

    if (xTaskCreate(camera_task, "camera", SB_CAM_TASK_STACK, NULL, SB_CAM_TASK_PRIO,
                    &s_cam_task) != pdPASS) {
        esp_camera_deinit();
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    err = sensorBoard_cmd_register("capture", capture_cmd_handler);
    if (err != ESP_OK) {
        vTaskDelete(s_cam_task);
        s_cam_task = NULL;
        esp_camera_deinit();
        goto fail;
    }

    sensorBoard_status_set_sensor("cam", true);

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL) {
        camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
        ESP_LOGI(TAG, "camera detected: %s (PID 0x%04X), QVGA JPEG q%d",
                 (info != NULL) ? info->name : "unknown", sensor->id.PID,
                 CONFIG_SB_CAM_JPEG_QUALITY);
    } else {
        ESP_LOGI(TAG, "camera up (QVGA JPEG q%d)", CONFIG_SB_CAM_JPEG_QUALITY);
    }
    return ESP_OK;

fail:
    sensorBoard_status_set_sensor("cam", false);
    return err;
}
