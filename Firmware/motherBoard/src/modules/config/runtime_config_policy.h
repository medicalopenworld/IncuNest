#pragma once
// Politica de la configuracion REMOTA (openspec: mb-remote-config, revision de
// seguridad del 2026-10-25).
//
// ---------------------------------------------------------------------------
//  RCFG_REMOTE_SAFETY_PARAMS  (0 = solo periodos GPRS; 1 = tambien seguridad)
// ---------------------------------------------------------------------------
// Con 0, lo UNICO que ThingsBoard puede cambiar son los tres periodos de
// publicacion GPRS (gprs_act, gprs_photo, gprs_stby). air_tmax, skin_tmax,
// heater_amps, fan_supply_pwm, fan_ctl_pwm y fan_pid_en solo se cambian en
// local (POST /config, consola USB). Un atributo compartido con una de esas
// claves se IGNORA, se deja un aviso en el log (tag CFG) y se publica
// cfg_rejected como atributo de cliente. La suscripcion y la peticion al
// conectar solo piden las claves de la lista remota efectiva.
//
// POR QUE ESTA A 0 (hallazgo C1 de la revision de seguridad).
// El MQTT de esta placa va EN CLARO: puerto 1883 con WiFiClient (WiFi) o
// TinyGsmClient (GPRS), sin TLS. Los atributos compartidos viajan por
// v1/devices/me/attributes, y en un canal sin cifrar ni verificacion de
// servidor cualquiera que este en el camino --la WiFi del hospital, el
// operador movil, un AP falso con el mismo SSID-- puede PUBLICAR en ese topic
// como si fuera el servidor. El token de acceso del dispositivo tambien va en
// claro, asi que ni siquiera hace falta suplantar nada: basta con haberlo
// visto pasar. Para los periodos GPRS lo peor que consigue un atacante es
// gastar datos de la SIM o dejar de ver la unidad (daño operativo,
// recuperable por la via local). Para el corte termico, el limite de corriente
// del calefactor o el ventilador es una orden sobre un dispositivo medico que
// calienta a un recien nacido. Eso no se puede dejar al alcance de un paquete
// sin autenticar, por muy bien que se valide el valor.
//
// QUE EXIGE PONERLO A 1:
//   1. TLS en el MQTT (8883) CON VERIFICACION DEL CERTIFICADO del servidor
//      (CA fijada en el firmware, no setInsecure()). Ver known_issues.md y la
//      nota sobre memoria: el handshake TLS en la MB sin PSRAM deja la SRAM
//      interna por debajo de 1 KB y ahoga la WiFi, asi que esto no es un
//      cambio de un flag: hay que resolver antes el presupuesto de memoria.
//   2. Una revision de riesgos del cambio remoto de limites de seguridad
//      (ISO 14971 / IEC 60601-1-8 / IEC 60601-2-19) hecha por quien firma el
//      expediente, y su traza en el fichero de gestion de riesgos.
//   3. Una verificacion en banco de los umbrales de abajo (RCFG_FAN_PWM_
//      REMOTE_MIN en particular).
//
// REGLAS EXTRA QUE SE APLICAN CUANDO ESTE A 1 (ya implementadas y probadas en
// host, compiladas pero INACTIVAS con 0; ver runtime_config_validate_remote()):
//   a. TECHO = VALOR DE FABRICA. Por un origen ThingsBoard el remoto solo
//      puede BAJAR o MANTENER un limite, nunca subirlo por encima de lo que la
//      unidad lleva compilado: AIR_THERMAL_CUTOUT_DEFAULT_C para el aire, el
//      de piel (SKIN_TEMPERATURE_SET_MAX acotado a ALARM_SKIN_CUTOUT_MAX_C) y
//      HEATER_MAX_POWER_AMPS segun el HW. Subir sigue siendo una accion local.
//   b. LOS CORTES FUERA DE 34..40 C SE RECHAZAN, no se recortan: un 45
//      tecleado por error o escrito por un atacante no se convierte en 40.
//      (El POST /config web si sigue recortando, como siempre; la consola USB
//      rechaza igual que ThingsBoard.)
//   c. UN CORTE DE AIRE (O DE PIEL) POR DEBAJO DE LA CONSIGNA ACTIVA CON EL
//      CONTROL EN CURSO SE RECHAZA: dispararia la alarma de corte (ALTA, corta
//      el calefactor) en una unidad que esta calentando a un bebe.
//   d. fan_supply_pwm, fan_ctl_pwm Y fan_pid_en SE RECHAZAN si la unidad no
//      tiene tacometro (in3.fanHasSpeedFeedback): sin realimentacion no hay
//      forma de saber que el ventilador sigue girando despues del cambio. Y se
//      rechazan mientras el calefactor este actuando.
//   e. El PWM remoto del ventilador tiene un MINIMO (RCFG_FAN_PWM_REMOTE_MIN,
//      abajo): un PWM bajo para el ventilador y el aire deja de circular.
//   f. LOS CAMBIOS REMOTOS DE SEGURIDAD CON TERAPIA ACTIVA (control o
//      fototerapia en marcha) NO SE APLICAN: quedan pendientes y se aplican al
//      llegar a standby (runtime_config_poll()).
//
// Las reglas comunes a CUALQUIER parametro remoto, con el flag a 0 o a 1:
//   - un cambio por clave cada RCFG_REMOTE_MIN_INTERVAL_MS como maximo; si
//     llega antes, queda pendiente y se aplica al cumplirse el plazo;
//   - solo fan_pid_en acepta un booleano; el resto exige un numero;
//   - cada cambio deja un evento de telemetria cfg_change y un aviso en el log;
//   - el reaplicado al reconectar solo aplica una escritura remota NUEVA y
//     nunca pisa un cambio local posterior (runtime_config.h).
#define RCFG_REMOTE_SAFETY_PARAMS 0

// PWM minimo que se acepta por ThingsBoard para fan_supply_pwm y fan_ctl_pwm
// (de 255). A MEDIR EN BANCO: 80 (~31 %) es un valor CONSERVADOR elegido sin
// medir, solo para tener un suelo mientras el flag este a 0. Hay que medir con
// el ventilador montado en la incubadora a que PWM deja de mantener el caudal
// y la lectura del tacometro dentro de lo esperado, y poner aqui ese valor mas
// un margen. No se ha medido nada.
#define RCFG_FAN_PWM_REMOTE_MIN 80

// Un cambio por clave desde ThingsBoard cada 10 s como maximo.
#define RCFG_REMOTE_MIN_INTERVAL_MS 10000UL

// Espera antes de reintentar un eco o un evento cfg_change que el broker no
// acepto, para no martillear un enlace GPRS que ya va mal.
#define RCFG_PUBLISH_RETRY_MS 5000UL
