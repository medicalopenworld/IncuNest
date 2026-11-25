#pragma once
// Sexo del bebe al nacer: fila de tres botones grandes (NINO / NINA / NO CONSTA)
// compartida por el asistente de activacion (BabyWizard) y el registro desde la
// pantalla Bebes (BabyHistory). Un toque elige y avanza: no hay CONTINUAR.
//
// Codigos ISO/IEC 5218, los mismos que guarda la placa y publica como baby_sex:
// 0 no consta, 1 nino, 2 nina. "NO CONSTA" es una respuesta valida: el dato es
// opcional (IncuTwin elige el dibujo por su cuenta si no llega).
#include <lvgl.h>
#include <stdint.h>

typedef void (*SexPickerCb)(uint8_t sex);

// Crea la fila dentro de `parent` y la devuelve ya dimensionada (600x110); el
// llamador la alinea. Solo puede haber una en pantalla a la vez (el callback se
// guarda en un estatico, como el resto de pantallas del asistente).
lv_obj_t *SexPicker_Create(lv_obj_t *parent, SexPickerCb onPick);
