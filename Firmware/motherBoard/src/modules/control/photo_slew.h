#pragma once

// Siguiente PWM del lazo de intensidad de fototerapia, separado del hardware.
//
// Modelo proporcional (I ~ PWM): pwm_modelo = pwm * objetivo / corriente. El
// salto hacia el modelo se limita de forma ASIMETRICA: subir corriente es la
// direccion de riesgo clinico y va despacio (max_step_up); bajarla es siempre
// segura y va rapido (max_step_down). Ver known_issues.md, apartado 21.
//
// Filtro de ruido: una lectura espuria ALTA de corriente no debe tirar 20
// cuentas de golpe (luego costaria ~20 s recuperarlas a +1/s). Por eso el paso
// de bajada grande exige DOS evaluaciones consecutivas fuera de banda por
// encima del objetivo; la primera baja como mucho max_step_up (min con
// max_step_down). Entrar en banda, quedar por debajo del objetivo o una
// lectura invalida reinician la cuenta. El contador vive en PhotoSlew, con
// init explicito, sin estaticos ocultos.
//
// Excepcion intencionada: el clamp a min_pwm se aplica DESPUES del limite de
// paso, asi que si pwm_now < min_pwm (semilla por debajo del minimo) el
// resultado sube a min_pwm de una vez, mas de max_step_up. Se acepta: min_pwm
// es el suelo en el que la lampara sigue regulable y una semilla por debajo
// de el no es un punto de trabajo valido; no se rechaza para no dejar el lazo
// parado.
//
// Test de regresion: motherBoard/test/test_photo_slew.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int high_count;  // evaluaciones consecutivas fuera de banda por encima
} PhotoSlew;

void photo_slew_init(PhotoSlew *s);

// Devuelve el PWM a aplicar en este ciclo. Llamar UNA vez por ciclo de
// control, tambien dentro de banda (para reiniciar la cuenta).
//  - corriente no finita o <= 0: devuelve pwm_now y reinicia la cuenta.
//  - |corriente - objetivo| <= tol: devuelve pwm_now y reinicia la cuenta.
//  - Si no, va hacia el PWM del modelo (acotado en float a [0, max_pwm] y
//    redondeado al entero mas cercano) sin cruzarlo, con delta en
//    [-limite_bajada, +max_step_up], y el resultado recortado a
//    [min_pwm, max_pwm].
int photo_slew_step(PhotoSlew *s, int pwm_now, float current_a, float target_a,
                    float tol_a, int max_step_up, int max_step_down,
                    int min_pwm, int max_pwm);

#ifdef __cplusplus
}
#endif
