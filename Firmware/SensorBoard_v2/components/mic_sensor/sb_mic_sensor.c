#include "sb_mic_sensor.h"
#include "sb_audio_dsp.h"
#include "sensorBoard_comm.h"
#include "sensorBoard_comm_protocol.h"
#include "sensorBoard_status.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "MIC";

/* Pinout: docs/hardware.md — ICS-41350 es PDM, no I2S estándar */
#define SB_PIN_MIC_CLK 40
#define SB_PIN_MIC_DATA 39

/* Prio 4: productor, por debajo del transporte (5) */
#define SB_MIC_TASK_PRIO 4
#define SB_MIC_TASK_STACK 4096

#define SB_MIC_READ_TIMEOUT_MS 1000

/* Ventana en muestras (int16). A 16 kHz y 1000 ms son 16000 muestras (32 KB):
 * el buffer vive en la tarea como static para no reventar el stack. */
#define SB_MIC_WINDOW_SAMPLES (CONFIG_SB_MIC_SAMPLE_RATE_HZ * CONFIG_SB_MIC_WINDOW_MS / 1000)

static i2s_chan_handle_t s_rx_chan = NULL;

static void audio_task(void *arg)
{
    (void)arg;
    static int16_t window[SB_MIC_WINDOW_SAMPLES];
    const float offset_db = (float)CONFIG_SB_MIC_DB_OFFSET_TENTHS / 10.0f;

    for (;;) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx_chan, window, sizeof(window), &bytes_read,
                                         pdMS_TO_TICKS(SB_MIC_READ_TIMEOUT_MS));
        bool ok = (err == ESP_OK) && (bytes_read >= sizeof(int16_t));

        float db = 0.0f;
        if (ok) {
            db = sb_audio_rms_to_db(sb_audio_rms(window, bytes_read / sizeof(int16_t)),
                                    offset_db);
            ok = sb_audio_level_plausible(db);
        }
        sensorBoard_status_set_sensor("mic", ok);

        if (ok) {
            char buf[96];
            uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);
            if (sb_audio_build_event(buf, sizeof(buf), db, ts) > 0) {
                sensorBoard_comm_send_json(buf);
            }
        }

        /* La captura DMA sigue corriendo; se descartan ventanas intermedias.
         * Publicación cada PUBLISH_PERIOD_S (>= duración de la ventana). */
        int idle_ms = CONFIG_SB_MIC_PUBLISH_PERIOD_S * 1000 - CONFIG_SB_MIC_WINDOW_MS;
        if (idle_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(idle_ms));
        }
    }
}

esp_err_t sb_mic_sensor_init(void)
{
    if (s_rx_chan != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    err = i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);
    if (err != ESP_OK) {
        goto fail;
    }

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_SB_MIC_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = SB_PIN_MIC_CLK,
            .din = SB_PIN_MIC_DATA,
            .invert_flags = { .clk_inv = false },
        },
    };
    err = i2s_channel_init_pdm_rx_mode(s_rx_chan, &pdm_cfg);
    if (err != ESP_OK) {
        goto fail_chan;
    }
    err = i2s_channel_enable(s_rx_chan);
    if (err != ESP_OK) {
        goto fail_chan;
    }

    if (xTaskCreate(audio_task, "mic", SB_MIC_TASK_STACK, NULL, SB_MIC_TASK_PRIO, NULL) !=
        pdPASS) {
        i2s_channel_disable(s_rx_chan);
        err = ESP_ERR_NO_MEM;
        goto fail_chan;
    }

    sensorBoard_status_set_sensor("mic", true);
    ESP_LOGI(TAG, "mic up (%d Hz, window %dms, publish %ds)", CONFIG_SB_MIC_SAMPLE_RATE_HZ,
             CONFIG_SB_MIC_WINDOW_MS, CONFIG_SB_MIC_PUBLISH_PERIOD_S);
    return ESP_OK;

fail_chan:
    i2s_del_channel(s_rx_chan);
    s_rx_chan = NULL;
fail:
    /* El componente es dueño de su nombre en status */
    sensorBoard_status_set_sensor("mic", false);
    return err;
}
