/* Runner de Unity para el test_app de plat_fs.
 *
 * Los TEST_CASE viven en test_plat_fs.cpp; aqui solo esta el app_main que los
 * lanza. Es el mismo reparto que usan los test_apps de SensorBoard_v2.
 */
#include "unity.h"
#include <stdio.h>

void app_main(void)
{
    printf("\n=== plat_fs: contrato de la capa de compatibilidad ===\n");
    printf("Se prueba sobre el LittleFS REAL de la particion 'spiffs'.\n\n");
    unity_run_menu();
}
