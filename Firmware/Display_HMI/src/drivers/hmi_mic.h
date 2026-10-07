#pragma once
#include <stdbool.h>
#include <stdint.h>

// Microfono PDM del display (LMD3526B261-OFA01): IO19 = MIC_CLK, IO20 =
// MIC_SD. Va siempre encendido desde setup(): hoy lo usa el test de fabrica
// (zumbador de la motherBoard) y esta pensado para la telemetria de ruido
// ambiente. HmiMic_Stop() existe pero nadie lo llama.
//
// IO19/IO20 son tambien D-/D+ del USB nativo del S3. El firmware no usa ese
// USB (el puerto serie es UART0 por el CH340), y HmiMic_Start() desconecta el
// pad USB para que su pull-up de D+ no pise la linea de datos del microfono.
// La placa ademas tiene que estar en el modo MIC+SPEAKER de su selector.
//
// Medida: ventanas de 100 ms a 16 kHz; de cada una se guarda el RMS de la
// componente alterna y su hora de fin. El nivel en dB NO esta calibrado
// (20*log10(rms/32768) + 120): sirve para comparar dos tramos, no como SPL.

bool HmiMic_Start(void);  // idempotente; false si el driver no arranca
void HmiMic_Stop(void);   // idempotente
bool HmiMic_Running(void);

// true si el microfono ha entregado senal viva en el ultimo segundo
// (pico a pico por encima del ruido de una linea pegada o flotante).
bool HmiMic_Alive(void);

// Nivel medio (promedio de energia, no de dB) de las ventanas que terminaron
// en (fromMs, toMs], en millis(). false si no hay ninguna ventana viva en ese
// tramo.
bool HmiMic_LevelBetween(uint32_t fromMs, uint32_t toMs, float *dbOut);

// Igual, pero solo con la energia en el tono del zumbador de la motherBoard
// (400 Hz y sus armonicos 3 y 5). El ruido de banda ancha de los ventiladores
// apenas cae ahi, asi que el zumbador destaca mucho mas que en el nivel total.
bool HmiMic_ToneBetween(uint32_t fromMs, uint32_t toMs, float *dbOut);