#include "sb_env_sensors.h"
#include "sb_env_convert.h"
#include "sb_env_i2c.h"
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_status.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>

static const char *TAG = "ENV";

/* Pinout: docs/hardware.md (fuente de verdad) */
#define SB_PIN_I2C_TEMP_SDA 41
#define SB_PIN_I2C_TEMP_SCL 42
#define SB_PIN_I2C_MAIN_SDA 4
#define SB_PIN_I2C_MAIN_SCL 5
#define SB_ALS_ADC_UNIT ADC_UNIT_1
#define SB_ALS_ADC_CHANNEL ADC_CHANNEL_0 /* IO1 */

/* SHT4x: direcciones por variante (AD1B=0x44, CD1B=0x46) */
#define SHT40_ADDR_AD1B 0x44
#define SHT40_ADDR_CD1B 0x46
#define SHT4X_CMD_MEASURE_HP 0xFD
#define SHT4X_MEASURE_DELAY_MS 10
#define SB_I2C_SPEED_HZ 100000
#define SB_I2C_TIMEOUT_MS 100

/* Gate de plausibilidad (rules/security.md: fuera de rango físico ⇒ sensor
 * no disponible). Rango operativo del SHT4x: -40..+125 °C — atrapa además
 * patrones bus-stuck (raw 0x0000→-45, 0xFFFF→+130) que colisionen en CRC-8. */
#define SHT4X_TEMP_MIN_C (-40.0f)
#define SHT4X_TEMP_MAX_C (125.0f)

/* Fallback si no hay calibración eFuse: fondo de escala aprox. a 12 dB
 * y máximo raw de 12 bits — mantener coherentes con ADC_ATTEN_DB_12 y
 * ADC_BITWIDTH_DEFAULT usados abajo. */
#define SB_ADC_FALLBACK_FULLSCALE_MV 3100
#define SB_ADC_MAX_RAW_12BIT 4095

/* Prio 4: por debajo del transporte usb_comm (5) — presupuesto de
 * prioridades comentado en sensorBoard_comm.c */
#define SB_ENV_TASK_PRIO 4
#define SB_ENV_TASK_STACK 4096

typedef struct {
    i2c_master_dev_handle_t dev; /* NULL si el bus/dispositivo no inició */
    const char *name;
} sb_sht_t;

/* Índice = posición en los arrays del evento (ADR-0002):
 * 0 = bus temp 0x44, 1 = bus temp 0x46, 2 = bus principal 0x44 */
static sb_sht_t s_sht[SB_ENV_SHT_COUNT] = { { NULL, "sht0" }, { NULL, "sht1" }, { NULL, "sht2" } };
static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_adc_cali = NULL;
/* Bus principal (IO4/IO5): lo comparte la cámara (SCCB) en Fase 5 */
static i2c_master_bus_handle_t s_i2c_main_bus = NULL;

i2c_master_bus_handle_t sb_env_get_main_i2c_bus(void)
{
    return s_i2c_main_bus;
}

static esp_err_t sht40_read(i2c_master_dev_handle_t dev, float *temp, float *rh)
{
    uint8_t cmd = SHT4X_CMD_MEASURE_HP;
    esp_err_t err = i2c_master_transmit(dev, &cmd, 1, SB_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(SHT4X_MEASURE_DELAY_MS));

    uint8_t rx[6];
    err = i2c_master_receive(dev, rx, sizeof(rx), SB_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    if (sht4x_crc8(&rx[0], 2) != rx[2] || sht4x_crc8(&rx[3], 2) != rx[5]) {
        return ESP_ERR_INVALID_CRC;
    }
    float t = sht4x_convert_temp((uint16_t)((rx[0] << 8) | rx[1]));
    if (t < SHT4X_TEMP_MIN_C || t > SHT4X_TEMP_MAX_C) {
        return ESP_ERR_INVALID_RESPONSE; /* dato CRC-válido pero no plausible */
    }
    *temp = t;
    *rh = sht4x_convert_rh((uint16_t)((rx[3] << 8) | rx[4]));
    return ESP_OK;
}

static float als_read(bool *valid)
{
    *valid = false;
    if (s_adc == NULL) {
        return 0.0f;
    }
    int raw = 0;
    if (adc_oneshot_read(s_adc, SB_ALS_ADC_CHANNEL, &raw) != ESP_OK) {
        return 0.0f;
    }
    int mv;
    if (s_adc_cali == NULL || adc_cali_raw_to_voltage(s_adc_cali, raw, &mv) != ESP_OK) {
        mv = raw * SB_ADC_FALLBACK_FULLSCALE_MV / SB_ADC_MAX_RAW_12BIT;
    }
    *valid = true;
    return sb_als_mv_to_lux(mv, CONFIG_SB_ALS_UV_PER_LUX);
}

static void sensor_task(void *arg)
{
    (void)arg;
    for (;;) {
        sb_env_readings_t r = { 0 };

        for (int i = 0; i < SB_ENV_SHT_COUNT; i++) {
            r.valid[i] = (s_sht[i].dev != NULL) &&
                         (sht40_read(s_sht[i].dev, &r.temp[i], &r.hum[i]) == ESP_OK);
            sensorBoard_status_set_sensor(s_sht[i].name, r.valid[i]);
        }
        r.lux = als_read(&r.lux_valid);
        sensorBoard_status_set_sensor("als", r.lux_valid);

        char buf[SB_PROTO_MAX_JSON_PAYLOAD];
        uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);
        if (sb_env_build_event(buf, sizeof(buf), &r, ts) > 0) {
            sensorBoard_comm_send_json(buf);
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_SB_ENV_POLL_PERIOD_S * 1000));
    }
}

/* Fallo de un bus/sensor/ADC = ese sensor queda no-disponible (sensors.x
 * false); solo el fallo de la tarea es fatal. Roadmap Fase 2: "reportar
 * false en lugar de bloquear la tarea o crashear". */
static i2c_master_bus_handle_t init_bus_devices(i2c_port_num_t port, int sda, int scl,
                                                sb_sht_t **devs, const uint8_t *addrs, int count)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus %d init failed", (int)port);
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addrs[i],
            .scl_speed_hz = SB_I2C_SPEED_HZ,
        };
        if (i2c_master_bus_add_device(bus, &dev_cfg, &devs[i]->dev) != ESP_OK) {
            devs[i]->dev = NULL;
            ESP_LOGW(TAG, "SHT40 add failed (bus %d addr 0x%02X)", (int)port, addrs[i]);
        }
    }
    return bus;
}

static void init_als(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = SB_ALS_ADC_UNIT };
    if (adc_oneshot_new_unit(&unit_cfg, &s_adc) != ESP_OK) {
        ESP_LOGW(TAG, "ADC init failed — ALS no disponible");
        s_adc = NULL;
        return;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_config_channel(s_adc, SB_ALS_ADC_CHANNEL, &chan_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "ADC channel config failed — ALS no disponible");
        adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
        return;
    }
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = SB_ALS_ADC_UNIT,
        .chan = SB_ALS_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali) != ESP_OK) {
        s_adc_cali = NULL; /* seguimos con conversión aproximada */
    }
}

esp_err_t sb_env_sensors_init(void)
{
    /* Bus de temperatura (IO41/IO42): sht0=0x44, sht1=0x46 */
    sb_sht_t *temp_bus_devs[] = { &s_sht[0], &s_sht[1] };
    const uint8_t temp_bus_addrs[] = { SHT40_ADDR_AD1B, SHT40_ADDR_CD1B };
    init_bus_devices(I2C_NUM_0, SB_PIN_I2C_TEMP_SDA, SB_PIN_I2C_TEMP_SCL, temp_bus_devs,
                     temp_bus_addrs, 2);

    /* Bus principal (IO4/IO5, compartido con SCCB de cámara en Fase 5): sht2 */
    sb_sht_t *main_bus_devs[] = { &s_sht[2] };
    const uint8_t main_bus_addrs[] = { SHT40_ADDR_AD1B };
    s_i2c_main_bus = init_bus_devices(I2C_NUM_1, SB_PIN_I2C_MAIN_SDA, SB_PIN_I2C_MAIN_SCL,
                                      main_bus_devs, main_bus_addrs, 1);

    init_als();

    /* Visibles en status desde el arranque; el primer poll fija el valor real */
    for (int i = 0; i < SB_ENV_SHT_COUNT; i++) {
        sensorBoard_status_set_sensor(s_sht[i].name, false);
    }
    sensorBoard_status_set_sensor("als", false);

    if (xTaskCreate(sensor_task, "env_sensors", SB_ENV_TASK_STACK, NULL, SB_ENV_TASK_PRIO, NULL) !=
        pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "env_sensors up (poll %ds)", CONFIG_SB_ENV_POLL_PERIOD_S);
    return ESP_OK;
}
