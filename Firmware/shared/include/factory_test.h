#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tabla unica de identificadores, estados y protocolo del test de fabrica
// (design.md D1/D2, shared-factory-test). Vive en shared/ porque los IDs y el
// formato de linea los usan las DOS placas: la motherBoard los emite/recibe
// por el enlace serie y el display los consume para pintar la pantalla de
// fabrica. Que cada lado tuviera su propio enum y su propio sscanf es
// exactamente el patron que ya desincronizo `humidityMin` y `cause` en
// `PROFILE_*` — una sola fuente aqui evita repetirlo.
//
// Los IDs de motherBoard (0..FTEST_MB_COUNT-1) SI viajan por el cable, caben
// en una mascara de 32 bits (persistencia en NVS). Los IDs del display
// (>= FTEST_HMI_BASE) son solo internos a esa placa y NUNCA se codifican en
// una linea `CTRL,FTEST*`.
typedef enum {
  FTEST_MB_SYSINFO = 0,
  FTEST_MB_INA3221,
  FTEST_MB_STANDBY,
  FTEST_MB_CHARGER,
  FTEST_MB_POWER_SRC,
  FTEST_MB_SKIN_ADC,
  // Sensor ambiental por CUALQUIERA de los tres caminos que existen segun la
  // generacion: SHT40 del SensorBoard por USB, STS35/SHTC3 por I2C2 (equipo
  // antiguo) o SHT4x exterior en I2C1. Un equipo lleva SensorBoard O sensor
  // ambiental, no ambos; con uno que lea bien el test PASA. Antes eran tres
  // tests ("origen", "enlace", "exterior") y en banco daban FAIL con hardware
  // sano: ACK falso del sondeo I2C2 sobre las lineas USB, y SHT4x ausente en
  // las unidades con SensorBoard.
  FTEST_MB_ENV_SENSOR,
  FTEST_MB_SB_STATUS,
  FTEST_MB_SB_ENV,
  FTEST_MB_SB_DOOR,
  FTEST_MB_SB_LIGHT,
  FTEST_MB_SB_CAMERA,
  FTEST_MB_ACTUATORS,
  FTEST_MB_FAN_RPM,
  FTEST_MB_HUMID_USB,
  FTEST_MB_BUZZER,
  FTEST_MB_AFE_SPI,
  FTEST_MB_AFE_PROBE,
  FTEST_MB_HMI_LINK,
  FTEST_MB_GSM_AT,
  FTEST_MB_GSM_SIM,
  FTEST_MB_GSM_SIGNAL,
  FTEST_MB_GSM_NET,
  FTEST_MB_WIFI,
  FTEST_MB_TB_PROVISION,
  FTEST_MB_TIME,
  FTEST_MB_NVS,
  FTEST_MB_LITTLEFS,
  FTEST_MB_COUNT, // = 28
  FTEST_HMI_BASE = 64,
  FTEST_HMI_SYSINFO = FTEST_HMI_BASE,
  FTEST_HMI_I2C,
  FTEST_HMI_PANEL,
  FTEST_HMI_TOUCH,
  FTEST_HMI_BUZZER,
  FTEST_HMI_SPEAKER,
  FTEST_HMI_WIFI,
  FTEST_HMI_NVS,
  FTEST_HMI_LINK,
  FTEST_HMI_END, // = 73
  FTEST_ID_NONE = 255
} FtestId;

typedef enum {
  FTEST_RUNNING = 0,
  FTEST_PASS = 1,
  FTEST_FAIL = 2,
  FTEST_SKIP = 3,
  FTEST_WAIT = 4,
  FTEST_CONFIRM = 5,
  // Aviso: el test no pudo completarse por falta de entorno (sin cobertura,
  // sin AP, sin servidor, sin hora de red) dentro de su plazo. No es un fallo
  // de la placa y no cuenta como FAIL, pero tampoco se oculta como SKIP: el
  // operario debe verlo en ambar. Es un estado FINAL (cuenta en FTEST_DONE).
  FTEST_WARN = 6
} FtestStatus;

typedef enum {
  FTEST_REJECT_BUSY = 0,
  FTEST_REJECT_CONTROL_ACTIVE = 1,
  FTEST_REJECT_UNKNOWN_ID = 2
} FtestReject;

typedef enum {
  FTEST_CMD_START,
  FTEST_CMD_RUN,
  FTEST_CMD_ABORT,
  FTEST_CMD_CONFIRM
} FtestHmiCmdType;

#define FTEST_DETAIL_MAX 40
#define FTEST_TX_LINE_MAX 64
#define FTEST_TX_QUEUE_LEN 16
#define FTEST_MB_RESPONSE_TIMEOUT_MS 10000
#define FTEST_STIMULUS_TIMEOUT_MS 30000
#define FTEST_CONFIRM_TIMEOUT_MS 60000

typedef struct {
  uint8_t id;
  FtestStatus status;
  char detail[FTEST_DETAIL_MAX + 1];
} FtestResult;

typedef struct {
  FtestHmiCmdType type;
  uint8_t id;
  bool ok;
} FtestHmiCmd;

// Consultas de tabla. `id` es `unsigned` (no `FtestId`) a proposito: los
// valores que llegan por el cable son numeros sin garantia de pertenecer al
// enum, y el enum en C++ con valor fuera de rango es comportamiento poco
// definido para comparar con seguridad.
bool ftest_id_is_mb(unsigned id);       // 0..FTEST_MB_COUNT-1
bool ftest_id_is_hmi(unsigned id);      // FTEST_HMI_BASE..FTEST_HMI_END-1
bool ftest_id_is_optional(unsigned id); // true solo para GSM_NET, WIFI,
                                         // TB_PROVISION, TIME, AFE_PROBE
const char *
ftest_id_key(unsigned id); // clave ASCII corta (<= 12 chars, p.ej. "sb_env",
                            // "hmi_touch"); "?" si desconocido

// Codificadores. Escriben una linea completa terminada en "\n" en `buf`.
// Devuelven la longitud escrita (sin contar el terminador nulo) o -1 si el
// resultado no cabe en `n` bytes o si algun campo es invalido (id fuera de
// la tabla de motherBoard, status/reject fuera de rango).
//
// "CTRL,FTEST,<id>,<st>,<detail>\n". `detail` puede ser NULL (linea vacia).
// El saneado sustituye ',', '\r' y '\n' por ';' y trunca a FTEST_DETAIL_MAX;
// eso es lo que garantiza que la linea siempre cabe en FTEST_TX_LINE_MAX.
int ftest_format_result(char *buf, size_t n, unsigned id, FtestStatus st,
                         const char *detail);
// "CTRL,FTEST_DONE,p,f,s,w\n" (w = avisos, FTEST_WARN)
int ftest_format_done(char *buf, size_t n, unsigned pass, unsigned fail,
                       unsigned skip, unsigned warn);
// "CTRL,FTEST_REJECT,<r>\n"
int ftest_format_reject(char *buf, size_t n, FtestReject r);

// Parsers. Reciben lo que sigue al prefijo ya reconocido por el llamante
// ("CTRL,FTEST," / "CTRL,FTEST_DONE," / "CTRL,FTEST_REJECT," / "HMI,FTEST,"),
// con o sin "\r\n" final. Devuelven false para descartar la linea sin tocar
// `*out` (ni ningun campo individual de salida) — el llamante debe poder
// asumir que una respuesta false deja su estado intacto.
bool ftest_parse_result(
    const char *after_prefix,
    FtestResult *out); // solo ids ftest_id_is_mb; status 0..6; detail = resto
                        // de la linea (puede ser vacio)
// Acepta 3 campos (placa anterior, warn = 0) o 4. `warn` puede ser NULL.
bool ftest_parse_done(const char *after_prefix, unsigned *pass, unsigned *fail,
                      unsigned *skip, unsigned *warn);
bool ftest_parse_reject(const char *after_prefix, FtestReject *out);
bool ftest_parse_hmi_cmd(
    const char *after_prefix,
    FtestHmiCmd *out); // "START" | "RUN,<id mb>" | "ABORT" |
                        // "CONFIRM,<id mb>,<0|1>"; campos de mas/menos o no
                        // numericos -> false

#ifdef __cplusplus
}
#endif
