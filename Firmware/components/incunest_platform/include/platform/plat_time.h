#pragma once

// Reloj monotono del sistema, sobre esp_timer (ESP-IDF puro).
//
// POR QUE SE CONSERVAN LOS NOMBRES millis()/micros():
// el firmware tiene 390 llamadas repartidas por las dos placas, y practicamente
// todas viven dentro del patron de resta sin signo
//
//     if ((uint32_t)(millis() - t0) >= PERIODO_MS) { ... }
//
// que es correcto SOLO si el tipo y el desbordamiento son exactamente los de
// antes. Renombrarlas no aportaria nada tecnico (la implementacion de abajo ya
// es nativa de IDF, no queda una linea de Arduino) y a cambio metaria 390
// ediciones a mano en los caminos de alarma y de control de un equipo medico.
// El nombre se queda; la implementacion es nuestra.
//
// Semantica que se preserva a proposito:
//   - millis() devuelve uint32_t y DA LA VUELTA a los 49,7 dias.
//   - micros() devuelve uint32_t y DA LA VUELTA a los 71,6 minutos.
// El codigo existente ya cuenta con eso. Para plazos largos usa millis64().

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Milisegundos desde el arranque. Desborda a los 49,7 dias (igual que antes).
uint32_t millis(void);

// Microsegundos desde el arranque. Desborda a los 71,6 minutos (igual que antes).
uint32_t micros(void);

// Sin desbordamiento practico. Para sellos de tiempo de larga duracion.
uint64_t millis64(void);
uint64_t micros64(void);

// Cede la CPU el tiempo indicado. Es vTaskDelay, NO la espera activa de
// Arduino: solo se puede llamar desde una tarea, nunca desde una ISR.
void delay_ms(uint32_t ms);

// Espera activa de microsegundos (esp_rom_delay_us). Solo para los pulsos
// cortos de los drivers de bus; no la uses para plazos de logica.
void delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif
