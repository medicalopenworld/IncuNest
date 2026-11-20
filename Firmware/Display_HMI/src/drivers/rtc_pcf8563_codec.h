#pragma once
// Codificacion de los registros de tiempo del PCF8563, sin I2C.
//
// Separado del driver a proposito: esto es lo unico del RTC que se puede
// probar sin el chip delante, y es justo donde se esconden los errores caros
// —BCD mal leido, el bit de siglo, el flag de pila baja ignorado—. El driver
// de al lado solo mueve bytes por el bus.
//
// El chip guarda siete registros consecutivos desde 0x02:
//
//   0x02 VL_seconds   bit7 = VL, bits6-0 = segundos BCD
//   0x03 minutes      bits6-0 = minutos BCD
//   0x04 hours        bits5-0 = horas BCD
//   0x05 days         bits5-0 = dia del mes BCD
//   0x06 weekdays     bits2-0 = dia de la semana (0 = domingo)
//   0x07 century_months  bit7 = siglo, bits4-0 = mes BCD
//   0x08 years        año BCD de dos digitos
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Numero de registros de tiempo, y el registro por el que empiezan.
#define PCF8563_TIME_REG_FIRST 0x02
#define PCF8563_TIME_REG_COUNT 7

// Direccion del chip en el bus. Fija por hardware, no configurable.
#define PCF8563_I2C_ADDR 0x51

// Convierte los siete registros en un epoch UTC.
//
// Devuelve false, sin tocar `outEpoch`, cuando el contenido no es creible:
//
//  - VL=1: el oscilador se paro o la tension de pila cayo por debajo del
//    umbral. El chip sigue contando y devuelve digitos con buena pinta, pero
//    no significan nada. Es la unica senal que distingue "hora conservada" de
//    "hora inventada", y saltarsela es como se acaba sellando el historial de
//    alarmas con una fecha falsa.
//  - Nibbles BCD fuera de rango decimal (p.ej. 0x1A), o campos civiles
//    imposibles. Un bus con ruido devuelve 0xFF, que pasa cualquier
//    comprobacion de rango hecha a ojo.
//  - Epoch resultante fuera de [2021-01-01, 2100-01-01).
//
// El bit de siglo se ignora al LEER y el año se interpreta como 2000+YY, que
// es la contrapartida de escribirlo siempre a 0 (ver abajo).
bool pcf8563_decode_time(const uint8_t regs[PCF8563_TIME_REG_COUNT],
                         uint32_t *outEpoch);

// Convierte un epoch UTC en los siete registros.
//
// Escribe SIEMPRE el bit de siglo a 0 e interpreta el año como 2000+YY. El
// chip guarda solo dos digitos de año, asi que hace falta una convencion; esta
// cubre 2000-2099, que contiene entera la ventana valida del firmware.
//
// Devuelve false, sin tocar `regs`, para un epoch fuera de esa ventana — 2100
// en adelante no es representable con esta convencion y hay que rechazarlo
// aqui, antes de que llegue al chip, y no descubrirlo al releerlo.
//
// El bit VL del registro de segundos se escribe a 0: escribir la hora es
// justamente lo que la declara valida.
bool pcf8563_encode_time(uint32_t epoch, uint8_t regs[PCF8563_TIME_REG_COUNT]);

#ifdef __cplusplus
}
#endif
