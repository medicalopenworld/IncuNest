/* Runner de Unity para el test_app de plat_nvs.
 *
 * OJO AL nvs_flash_init(): la capa NvsPrefs NO inicializa la particion, solo
 * la abre. En el firmware de la placa eso lo hace el arranque (main.cpp); aqui
 * hay que hacerlo a mano o TODOS los begin() devuelven false. Sin esto los
 * tests fallan por una razon que no tiene nada que ver con lo que prueban.
 */
#include "unity.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include <stdio.h>

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Misma recuperacion que hace el firmware: borrar y reintentar.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    printf("\n=== plat_nvs: contrato de la capa de compatibilidad ===\n");
    printf("Se prueba sobre la NVS REAL de la placa.\n\n");
    unity_run_menu();
}
