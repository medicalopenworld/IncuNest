#pragma once

// Siguiente PWM del lazo de intensidad de fototerapia, separado del hardware.
//
// Modelo proporcional (I ~ PWM): pwm_modelo = pwm * objetivo / corriente. El
// salto hacia el modelo se limita de forma ASIMETRICA: subir corriente es la
// direccion de riesgo clinico y va despacio (max_step_up); bajarla es siempre
// segura y va rapido (max_step_down). Ver known_issues.md, apartado 21.
//
// Test de regresion: motherBoard/test/test_photo_slew.

#ifdef __cplusplus
extern "C" {
#endif

// Devuelve el PWM a aplicar en este ciclo.
//  - |corriente - objetivo| <= tol, o corriente <= 0: devuelve pwm_now.
//  - Si no, va hacia el PWM del modelo (redondeo al entero mas cercano) sin
//    cruzarlo, con delta en [-max_step_down, +max_step_up], y el resultado
//    recortado a [min_pwm, max_pwm].
int photo_slew_next_pwm(int pwm_now, float current_a, float target_a,
                        float tol_a, int max_step_up, int max_step_down,
                        int min_pwm, int max_pwm);

#ifdef __cplusplus
}
#endif
