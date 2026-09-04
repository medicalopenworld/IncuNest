// Cuerpos de los 29 tests de la motherBoard (design.md D10, mb-factory-test).
// Una funcion estatica por id, tabla {id, fn} al final, ftest_hw_run() como
// unico punto de entrada. Fail-safe por diseno: ninguna lectura invalida
// levanta una excepcion, siempre se traduce a FAIL con detail.
//
// Sin entorno de test (hardware real, USB, I2C, PWM): verificacion manual en
// banco documentada en el commit de este cambio.
#include <Preferences.h>
#include <LittleFS.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <time.h>

#include "esp_mac.h"

#include "factory_test_hw.h"
#include "main.h"
#include "modules/sensorboard_comm/sensorboard_comm.h"
#include "modules/sensors/sensor_source.h"
#include "modules/sensors/sensors_module.h"
#include "modules/util/tz_source.h"
#include "system/hw_selftest.h"

extern IncuNest_parameters in3;
extern TwoWire *wire;
extern bool ambientSensorPresent;
extern bool digitalCurrentSensorPresent[2];
extern uint32_t g_lastHmiLineMs;
extern bool g_hmiEverSeen;
extern long lastSuccesfullSensorUpdate[SENSOR_TEMP_QTY];

#define D(...) snprintf(detail, FTEST_DETAIL_MAX + 1, __VA_ARGS__)

// Los unicos bits que ACTUATORS (id 14) debe mirar en HW_error: calefactor,
// fototerapia y ventilador (mb-factory-test, "Tests de actuadores y
// humidificador reutilizan el autotest"). El bit de humidificador que
// actuatorsTest() tambien puede levantar internamente NO cuenta aqui -- lo
// prueba HUMID_USB (id 16) por su cuenta, con su propio acceso a hardware.
static const long kFtestActuatorBitsMask =
    (1L << HEATER_CONSUMPTION_MIN_ERROR) | (1L << HEATER_CONSUMPTION_MAX_ERROR) |
    (1L << FAN_CONSUMPTION_MIN_ERROR) | (1L << FAN_CONSUMPTION_MAX_ERROR) |
    (1L << FAN_RPM_MIN_ERROR) | (1L << PHOTOTHERAPY_CONSUMPTION_MIN_ERROR) |
    (1L << PHOTOTHERAPY_CONSUMPTION_MAX_ERROR);

// ---------------------------------------------------------------------------
// 0: SYSINFO
static FtestStatus ftest_sysinfo(char *detail, FtestCascade *) {
  const uint32_t flashMB = (uint32_t)(ESP.getFlashChipSize() / (1024u * 1024u));
  const uint32_t heap = esp_get_free_heap_size();
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  uint32_t boots = 0;
  {
    Preferences p;
    p.begin("diag", true);
    boots = p.getUInt("boots", 0);
    p.end();
  }
  D("%uM h%uK r%d b%u %02X%02X%02X%02X%02X%02X", (unsigned)flashMB,
    (unsigned)(heap / 1024u), (int)esp_reset_reason(), (unsigned)boots,
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return (heap < FTEST_HEAP_MIN_BYTES) ? FTEST_FAIL : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 1: INA3221
static FtestStatus ftest_ina3221(char *detail, FtestCascade *) {
  const bool ok =
      digitalCurrentSensorPresent[MAIN] && digitalCurrentSensorPresent[SECUNDARY];
  D("main=%d sec=%d", digitalCurrentSensorPresent[MAIN],
    digitalCurrentSensorPresent[SECUNDARY]);
  return ok ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 2: STANDBY -- reusa testStandByCurrent(), cualquier bit nuevo en HW_error
// es FAIL (design.md D10: los unicos que puede levantar esta llamada son
// DEFECTIVE_CURRENT_SENSOR y STANDBY_CONSUMPTION_MAX_ERROR).
static FtestStatus ftest_standby(char *detail, FtestCascade *) {
  const long before = HW_error;
  testStandByCurrent();
  const bool newBit = (HW_error & ~before) != 0;
  D("i=%.3fA", in3.system_current_standby_test);
  return newBit ? FTEST_FAIL : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 3: CHARGER
static FtestStatus ftest_charger(char *detail, FtestCascade *) {
  // Bloqueante #5 del review de seguridad: `st` sin inicializar formateaba
  // memoria basura en el detail si charge_status() devolvia false (nunca
  // toca `st` en ese caso). `= {}` deja un valor conocido y, ademas, ya no
  // se formatea nada de `st` salvo que la llamada haya tenido exito.
  BQ25730_Status st = {};
  const bool ok = charge_status(&st);
  if (!ok) {
    D("sin respuesta");
    return FTEST_FAIL;
  }
  D("vb=%dmV vs=%dmV vu=%dmV i=%dmA", (int)st.vbat_mv, (int)st.vsys_mv,
    (int)st.vbus_mv, (int)st.ichg_ma);
  return FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 4: POWER_SRC -- siempre PASA, informativo (design.md D10).
static FtestStatus ftest_power_src(char *detail, FtestCascade *) {
  D("%s", (g_bq_status_valid && g_bq_status.ac_present) ? "ac" : "bat");
  return FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 5: SKIN_ADC
static FtestStatus ftest_skin_adc(char *detail, FtestCascade *) {
  wire->beginTransmission(ADS1110_I2C_ADDRESS);
  if (wire->endTransmission() != 0) {
    D("sin ack 0x48");
    return FTEST_FAIL;
  }
  measureSkinSensor();
  if (skinProbeLastReading() == SKIN_PROBE_READING_OPEN) {
    D("sin sonda");
    return FTEST_PASS;
  }
  const float t = in3.temperature[SKIN_SENSOR];
  D("t=%.1f", t);
  // Bloqueante #4 del review de seguridad: NaN no debe colar como PASS. Con
  // la comparacion en negativo original (t < 1 || t > 60) un NaN hace las dos
  // false y cae en PASS; en positivo, un NaN hace la conjuncion false y cae
  // en FAIL, que es lo correcto. isfinite() ademas rechaza +-inf.
  if (!std::isfinite(t)) return FTEST_FAIL;
  return (t >= 1.0f && t <= 60.0f) ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 6: EXT_SHT4X
static FtestStatus ftest_ext_sht4x(char *detail, FtestCascade *) {
  if (!ambientSensorPresent) {
    D("no detectado");
    return FTEST_FAIL;
  }
  if (!updateAmbientSensor()) {
    D("lectura fallo");
    return FTEST_FAIL;
  }
  const float t = in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR];
  const float h = in3.humidity[AMBIENT_DIGITAL_HUM_SENSOR];
  D("t=%.1f h=%.0f", t, h);
  // Bloqueante #4: mismo motivo que SKIN_ADC arriba -- rechazar NaN antes y
  // comparar el rango en positivo.
  if (!std::isfinite(t) || !std::isfinite(h)) return FTEST_FAIL;
  if (!(t >= -10.0f && t <= 60.0f)) return FTEST_FAIL;
  if (!(h >= 0.0f && h <= 100.0f)) return FTEST_FAIL;
  return FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 7: SENSORBOARD -- sensor de cabina por CUALQUIERA de los dos caminos: SHT40
// de la SensorBoard por USB o STS35/SHTC3 por I2C2 (equipo antiguo). Sustituye
// a los antiguos SENSOR_SRC + SB_LINK (design.md D10, shared-factory-test-
// bench): en banco, con una SensorBoard conectada, el sondeo I2C2 de
// clasificacion de generacion se hace sobre las mismas lineas que son D+/D-
// del USB y devuelve un ACK falso -- separar "origen" de "enlace" convertia
// esa peculiaridad del sondeo en un FAIL de fabrica con una SensorBoard
// enlazada y funcionando. Lo que importa es que la cabina tenga sensor, no
// por que bus llega.
//
// cascade->sb_usb se deja en OK solo si el camino que dio PASA fue USB: los
// tests sb_status/sb_env/sb_door/sb_light/sb_camera siguen exigiendo ese
// camino y SKIP en I2C (no tienen forma de leer nada por ese bus).
#define FTEST_SENSORBOARD_TIMEOUT_MS 10000u
#define FTEST_SENSORBOARD_ENV_FRESH_MS 3000u
#define FTEST_SENSORBOARD_I2C_FRESH_MS 5000u
// Mismo rango que DIG_TEMP_ROOM_MIN/MAX (initHardware.cpp): duplicado a
// proposito, igual que ya hacen SKIN_ADC/EXT_SHT4X con sus propios rangos --
// esos #define son locales a ese .cpp y no se exponen en ningun header.
#define FTEST_ROOM_TEMP_MIN_C 1.0f
#define FTEST_ROOM_TEMP_MAX_C 60.0f
static FtestStatus ftest_sensorboard(char *detail, FtestCascade *cascade) {
  const bool viaUsb = (sensorSourceGet() == SENSOR_SOURCE_SENSORBOARD);
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_SENSORBOARD_TIMEOUT_MS) {
    if (viaUsb) {
      SbSnapshot s;
      sensorboard_get_snapshot(&s);
      const uint32_t envAge = (uint32_t)(millis() - s.last_env_ms);
      const bool anyValid =
          s.temp.valid[0] || s.temp.valid[1] || s.temp.valid[2];
      if (sensorboard_comm_connected() && s.link_ok && s.env_seen &&
          envAge < FTEST_SENSORBOARD_ENV_FRESH_MS && anyValid) {
        cascade->sb_usb = FTEST_DEP_OK;
        D("usb");
        return FTEST_PASS;
      }
    } else {
      const uint32_t age = (uint32_t)(
          millis() - (uint32_t)lastSuccesfullSensorUpdate[ROOM_DIGITAL_TEMP_SENSOR]);
      const float t = in3.temperature[ROOM_DIGITAL_TEMP_SENSOR];
      if (roomSensorI2CDetected() && age < FTEST_SENSORBOARD_I2C_FRESH_MS &&
          std::isfinite(t) && t >= FTEST_ROOM_TEMP_MIN_C &&
          t <= FTEST_ROOM_TEMP_MAX_C) {
        // Paso por I2C, no por USB: los tests SB_* de abajo seguiran SKIP.
        cascade->sb_usb = FTEST_DEP_FAILED;
        D("i2c");
        return FTEST_PASS;
      }
    }
    if (ftest_abort_requested()) {
      cascade->sb_usb = FTEST_DEP_FAILED;
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  cascade->sb_usb = FTEST_DEP_FAILED;
  D("%s sin datos", viaUsb ? "usb" : "i2c");
  return FTEST_FAIL;
}

// Los tests sb_status/sb_env/sb_door/sb_light/sb_camera solo tienen datos que
// leer si SENSORBOARD (id 7) paso por el camino USB -- si paso por I2C, o si
// no llego a pasar (RUN de un solo test sin haber corrido antes el 7), no hay
// enlace con la SensorBoard del que tirar.
static bool sb_skip_if_no_usb(const FtestCascade *cascade, char *detail) {
  if (cascade->sb_usb != FTEST_DEP_OK) {
    D("sin usb");
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// 8: SB_STATUS
static FtestStatus ftest_sb_status(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_usb(cascade, detail)) return FTEST_SKIP;

  // Bloqueante #10 del review de seguridad: si la peticion ni siquiera se
  // pudo encolar, eso no es "falta un recurso concreto" (avail_*) sino
  // "la SensorBoard no respondio en absoluto" -- detail distinto para no
  // confundir al operario sobre que revisar.
  if (!sensorboard_status_request()) {
    D("sin respuesta");
    return FTEST_FAIL;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 5000) {
    SbSnapshot s;
    sensorboard_get_snapshot(&s);
    if (s.status_seen && s.avail_sht[0] && s.avail_sht[1] && s.avail_sht[2] &&
        s.avail_als && s.avail_door && s.avail_cam) {
      D("fw=%s sw=%d", s.sb_fw, s.usb_swap);
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  SbSnapshot s;
  sensorboard_get_snapshot(&s);
  if (!s.status_seen) {
    // La peticion se encolo bien pero no llego ninguna respuesta en el
    // plazo: distinto de "llego la respuesta pero falta un recurso".
    D("sin respuesta");
    return FTEST_FAIL;
  }
  static const char *const kNames[6] = {"sht0", "sht1", "sht2",
                                        "als",  "door", "cam"};
  const bool avail[6] = {s.avail_sht[0], s.avail_sht[1], s.avail_sht[2],
                        s.avail_als,    s.avail_door,   s.avail_cam};
  const char *missing = kNames[0];
  for (int i = 0; i < 6; i++) {
    if (!avail[i]) {
      missing = kNames[i];
      break;
    }
  }
  D("falta %s", missing);
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 9: SB_ENV
static FtestStatus ftest_sb_env(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_usb(cascade, detail)) return FTEST_SKIP;

  SbSnapshot s;
  sensorboard_get_snapshot(&s);
  if (!s.env_seen) {
    D("sin datos");
    return FTEST_FAIL;
  }
  if (!(s.temp.valid[0] && s.temp.valid[1] && s.temp.valid[2])) {
    D("posicion invalida");
    return FTEST_FAIL;
  }

  // Bloqueante #4 del review de seguridad: `valid[i]` solo dice que la
  // SensorBoard marco la posicion como leida, no que el valor sea finito.
  // Un NaN colado aqui volveria FTEST_FAIL/spread/diff tambien NaN, y las
  // comparaciones en negativo de abajo (`> MAX`) dan false con NaN -> PASS.
  bool allFinite = true;
  float mn = s.temp.value[0], mx = s.temp.value[0], sum = 0.0f;
  for (int i = 0; i < 3; i++) {
    if (!std::isfinite(s.temp.value[i])) allFinite = false;
    if (s.temp.value[i] < mn) mn = s.temp.value[i];
    if (s.temp.value[i] > mx) mx = s.temp.value[i];
    sum += s.temp.value[i];
  }
  if (!allFinite) {
    D("valor no finito");
    return FTEST_FAIL;
  }
  const float spread = mx - mn;
  const float mean = sum / 3.0f;

  bool haveExt = false;
  float diff = 0.0f;
  if (ambientSensorPresent && updateAmbientSensor()) {
    const float ext = in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR];
    if (std::isfinite(ext)) {
      haveExt = true;
      diff = fabsf(mean - ext);
    }
  }

  D("sp=%.1f df=%.1f", spread, diff);
  // Comparaciones en positivo (bloqueante #4): con allFinite ya garantizado
  // arriba esto es cinturon y tirantes, no defensa unica.
  if (!(spread <= FTEST_SB_SPREAD_MAX_C)) return FTEST_FAIL;
  if (haveExt && !(diff <= FTEST_SB_VS_EXT_MAX_C)) return FTEST_FAIL;
  return FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 10: SB_DOOR (WAIT)
static FtestStatus ftest_sb_door(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_usb(cascade, detail)) return FTEST_SKIP;

  ftest_emit(FTEST_MB_SB_DOOR, FTEST_WAIT, "abra y cierre la puerta");

  bool sawOpen = false;
  bool sawClose = false;
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_STIMULUS_TIMEOUT_MS) {
    SbSnapshot s;
    sensorboard_get_snapshot(&s);
    if (s.door_known) {
      if (s.door_open) {
        sawOpen = true;
      } else if (sawOpen) {
        sawClose = true;
      }
    }
    if (sawOpen && sawClose) {
      D("ok");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 11: SB_LIGHT (WAIT)
static FtestStatus ftest_sb_light(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_usb(cascade, detail)) return FTEST_SKIP;

  float base = -1.0f;
  {
    const uint32_t start = millis();
    while ((uint32_t)(millis() - start) < 2000) {
      SbSnapshot s;
      sensorboard_get_snapshot(&s);
      if (s.lux_valid) base = s.lux;
      if (ftest_abort_requested()) {
        D("%s", ftest_abort_reason());
        return FTEST_SKIP;
      }
      ftest_wdt_feed();
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }
  if (base < 0.0f) {
    D("sin lux");
    return FTEST_FAIL;
  }
  if (base < 20.0f) {
    D("poca luz");
    return FTEST_SKIP;
  }

  ftest_emit(FTEST_MB_SB_LIGHT, FTEST_WAIT, "tape el sensor de luz");
  const float threshold = base * 0.5f;
  const uint32_t start2 = millis();
  while ((uint32_t)(millis() - start2) < 20000) {
    SbSnapshot s;
    sensorboard_get_snapshot(&s);
    if (s.lux_valid && s.lux < threshold) {
      D("base=%.0f", base);
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout b=%.0f", base);
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 12: SB_CAMERA
static FtestStatus ftest_sb_camera(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_usb(cascade, detail)) return FTEST_SKIP;

  if (!sensorboard_capture_request()) {
    D("no se pudo pedir");
    return FTEST_FAIL;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 10000) {
    uint8_t *jpeg = nullptr;
    size_t len = 0;
    if (sensorboard_capture_take(&jpeg, &len)) {
      const bool ok = len >= 1000;
      D("len=%u", (unsigned)len);
      sensorboard_capture_free(jpeg);
      return ok ? FTEST_PASS : FTEST_FAIL;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 13: ACTUATORS -- reusa actuatorsTest() tal cual (design.md, Non-Goals).
static FtestStatus ftest_actuators(char *detail, FtestCascade *cascade) {
  const long before = HW_error;
  const bool critical = actuatorsTest();
  const bool newActuatorBit = (HW_error & ~before & kFtestActuatorBitsMask) != 0;
  const bool failed = critical || newActuatorBit;
  cascade->actuators = failed ? FTEST_DEP_FAILED : FTEST_DEP_OK;
  D("h=%.2fA p=%.2fA f=%.2fA", in3.heater_current_test,
    in3.phototherapy_current_test, in3.fan_current_test);
  return failed ? FTEST_FAIL : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 14: FAN_RPM -- depende del resultado de ACTUATORS (cascada).
static FtestStatus ftest_fan_rpm(char *detail, FtestCascade *cascade) {
  if (cascade->actuators == FTEST_DEP_FAILED) {
    D("actuators fallo");
    return FTEST_SKIP;
  }
  if (!in3.fanHasSpeedFeedback) {
    D("sin feedback");
    return FTEST_FAIL;
  }
  D("rpm=%.0f", in3.fan_rpm);
  // Bloqueante #4: comparacion en negativo original (< MIN -> FAIL) deja
  // pasar un NaN (ninguna comparacion con NaN es true); en positivo un NaN
  // cae en FAIL.
  if (!std::isfinite(in3.fan_rpm)) return FTEST_FAIL;
  return (in3.fan_rpm >= FAN_MIN_RPM) ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 15: HUMID_USB -- acceso directo a USB_EN/USB_FAULT, no reusa
// actuatorsTest()/in3_hum (design.md D10, texto literal del requisito).
#define FTEST_USB_FAULT_SETTLE_MS 100
static FtestStatus ftest_humid_usb(char *detail, FtestCascade *) {
  digitalWrite(USB_EN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(FTEST_USB_FAULT_SETTLE_MS));
  const bool faultOk = GPIORead(USB_FAULT);
  const float mA = measureMeanConsumption(SECUNDARY, USB_SHUNT_CHANNEL) * 1000.0f;
  digitalWrite(USB_EN, LOW);

  D("fault=%d i=%.1fmA", faultOk, mA);
  if (!faultOk) return FTEST_FAIL;
  // Bloqueante #4: idem FAN_RPM -- positivo, y NaN explicito antes.
  if (!std::isfinite(mA)) return FTEST_FAIL;
  return (mA > FTEST_HUMID_MIN_MA) ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 16: BUZZER -- con microfono (SensorBoard) o CONFIRM del operario.
static FtestStatus ftest_buzzer(char *detail, FtestCascade *) {
  SbSnapshot before;
  sensorboard_get_snapshot(&before);

  if (before.sound_seen) {
    const float base = before.dba;
    const uint32_t baseMs = before.last_sound_ms;
    ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
    float peak = base;
    bool gotNew = false;
    const uint32_t start = millis();
    while ((uint32_t)(millis() - start) < 7000) {
      SbSnapshot s;
      sensorboard_get_snapshot(&s);
      if (s.sound_seen && s.last_sound_ms != baseMs) {
        peak = s.dba;
        gotNew = true;
        break;
      }
      if (ftest_abort_requested()) break;
      ftest_wdt_feed();
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    ledcWrite(BUZZER_PWM_CHANNEL, 0);
    D("base=%.0f pico=%.0f", base, peak);
    if (!gotNew) return FTEST_FAIL;
    return ((peak - base) >= FTEST_BUZZER_DBA_DELTA) ? FTEST_PASS : FTEST_FAIL;
  }

  // Sin microfono: arma el CONFIRM ANTES de encolar la linea (bloqueante #8
  // del review de seguridad, "carrera del CONFIRM"): si el operario
  // contestara entre encolar CTRL,FTEST,16,5 y que ftest_wait_confirm()
  // fijara el id esperado, ese "give" se habria perdido.
  ftest_arm_confirm(FTEST_MB_BUZZER);
  ftest_emit(FTEST_MB_BUZZER, FTEST_CONFIRM, "sonido del zumbador?");
  ledcWrite(BUZZER_PWM_CHANNEL, BUZZER_HALF_PWM);
  vTaskDelay(pdMS_TO_TICKS(500));
  ledcWrite(BUZZER_PWM_CHANNEL, 0);
  const int confirmResult =
      ftest_wait_confirm(FTEST_MB_BUZZER, FTEST_CONFIRM_TIMEOUT_MS);
  if (confirmResult < 0) {
    D("timeout");
    return FTEST_FAIL;
  }
  D("confirm=%d", confirmResult);
  return confirmResult ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 17: AFE_SPI
static FtestStatus ftest_afe_spi(char *detail, FtestCascade *) {
  const AFE4490TimingConfig tc = afe.getTimingConfig();
  const uint32_t diag = afe.runAfeDiagnostics();
  D("t2=%u diag=0x%lX", (unsigned)tc.t2, (unsigned long)diag);
  const bool ok = (tc.t1 < tc.t2) && (tc.t2 != 0) && (tc.t2 != 0xFFFF);
  return ok ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 18: AFE_PROBE (opcional)
static FtestStatus ftest_afe_probe(char *detail, FtestCascade *) {
  const ProbeState st = g_spo2_data.probe_state;
  D("state=%d", (int)st);
  return (st == ProbeState::PROBE_DISCONNECTED) ? FTEST_SKIP : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 19: HMI_LINK
static FtestStatus ftest_hmi_link(char *detail, FtestCascade *) {
  const uint32_t age = (uint32_t)(millis() - g_lastHmiLineMs);
  D("seen=%d age=%ums", g_hmiEverSeen, (unsigned)age);
  return (g_hmiEverSeen && age < 5000) ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 20-23: GSM, pasivos sobre el estado de GPRS_Task (design.md D7).
//
// gsm_at PASA en cuanto el modem ha dado CUALQUIER senal de vida por AT
// (GPRS.modemResponded/simReady, puestos en GPRSPowerUp()) o ya esta mas
// adelante en la secuencia (connect/post). `GPRS.powerUp` NO basta por si
// solo: solo es true DURANTE la secuencia de arranque del modem, asi que en
// banco fallaba si el modem ya habia arrancado antes de pulsar el boton de
// fabrica.
static FtestStatus ftest_gsm_at(char *detail, FtestCascade *) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 45000) {
    if (GPRS.modemResponded || GPRS.simReady || GPRS.connect || GPRS.post) {
      D("ok");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

// gsm_sim PASA con "+CPIN: READY" (GPRS.simReady); ya no exige el CCID (leer
// la ICCID es un paso posterior de la secuencia que puede tardar mas y no es
// lo que este test dice comprobar). Si el CCID ya esta disponible se informa
// en el detail, sin condicionar el resultado.
static FtestStatus ftest_gsm_sim(char *detail, FtestCascade *cascade) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 15000) {
    if (GPRS.simReady) {
      cascade->gsm_sim = FTEST_DEP_OK;
      if (GPRS.CCID.length() > 0) {
        D("ready ccid=%s", GPRS.CCID.c_str());
      } else {
        D("ready");
      }
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      cascade->gsm_sim = FTEST_DEP_FAILED;
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  cascade->gsm_sim = FTEST_DEP_FAILED;
  D("sin sim");
  return FTEST_FAIL;
}

static FtestStatus ftest_gsm_signal(char *detail, FtestCascade *cascade) {
  if (cascade->gsm_sim == FTEST_DEP_FAILED) {
    D("sin sim");
    return FTEST_SKIP;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 15000) {
    if (GPRS.CSQ >= 1 && GPRS.CSQ <= 31) {
      D("csq=%d", GPRS.CSQ);
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("csq=%d", GPRS.CSQ);
  return FTEST_FAIL;
}

// gsm_net SKIP en cascada si no hay SIM (no tiene sentido esperar adjunto de
// red sin SIM); si hay SIM pero se agota el plazo, es un AVISO -- fabrica
// puede no tener cobertura celular en la nave, y eso no es un fallo de la
// placa (shared-factory-test-bench, "Estado nuevo WARN").
static FtestStatus ftest_gsm_net(char *detail, FtestCascade *cascade) {
  if (cascade->gsm_sim == FTEST_DEP_FAILED) {
    D("sin sim");
    return FTEST_SKIP;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_CONN_TIMEOUT_MS) {
    if (GPRS.post) {
      D("ok");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("sin red");
  return FTEST_WARN;
}

// ---------------------------------------------------------------------------
// 24: WIFI (opcional) -- sin AP por defecto en fabrica es un AVISO, no un
// FAIL ni un SKIP silencioso: el operario debe verlo en ambar.
static FtestStatus ftest_wifi(char *detail, FtestCascade *) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_CONN_TIMEOUT_MS) {
    if (WIFIIsConnected()) {
      D("rssi=%d", (int)WiFi.RSSI());
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("sin AP");
  return FTEST_WARN;
}

// ---------------------------------------------------------------------------
// 25: TB_PROVISION (opcional, design.md D8)
static FtestStatus ftest_tb_provision(char *detail, FtestCascade *) {
  if (in3.serialNumber == 0) {
    D("sin serie");
    return FTEST_WARN;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_CONN_TIMEOUT_MS) {
    if (WIFIIsConnectedToServer()) {
      D("wifi");
      return FTEST_PASS;
    }
    if (GPRSIsConnectedToServer()) {
      D("gprs");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("sin servidor");
  return FTEST_WARN;
}

// ---------------------------------------------------------------------------
// 26: TIME (opcional) -- espera hasta FTEST_CONN_TIMEOUT_MS a que el reloj
// se ponga en hora (NTP por WiFi o cellular, ver GPRSEnsureTimeSynced()); sin
// eso no hay forma de distinguir "todavia no le ha dado tiempo" de "no va a
// llegar", asi que antes de shared-factory-test-bench este test decidia con
// una unica lectura instantanea que casi siempre caia en SKIP.
static FtestStatus ftest_time(char *detail, FtestCascade *) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_CONN_TIMEOUT_MS) {
    if (time(nullptr) >= 1609459200) {
      const char *src = "none";
      switch (tz_source_origin()) {
        case TZ_SOURCE_NITZ: src = "nitz"; break;
        case TZ_SOURCE_IP: src = "ip"; break;
        case TZ_SOURCE_MANUAL: src = "manual"; break;
        default: src = "none"; break;
      }
      D("src=%s", src);
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("%s", ftest_abort_reason());
      return FTEST_SKIP;
    }
    ftest_wdt_feed();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("sin hora");
  return FTEST_WARN;
}

// ---------------------------------------------------------------------------
// 27: NVS
static FtestStatus ftest_nvs(char *detail, FtestCascade *) {
  Preferences p;
  p.begin(NS_FTEST, false);
  const uint32_t val = (uint32_t)millis();
  p.putUInt(KEY_FTEST_PROBE, val);
  const uint32_t back = p.getUInt(KEY_FTEST_PROBE, 0);
  p.end();
  D("probe=%u", (unsigned)back);
  return (back == val) ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 28: LITTLEFS
static FtestStatus ftest_littlefs(char *detail, FtestCascade *) {
  // Bloqueante #6 del review de seguridad: `begin(true)` formatea la
  // particion si el montaje falla, lo que destruiria cualquier dato ya
  // guardado ahi. Un test de fabrica solo debe VERIFICAR que la particion
  // existente monta, nunca reformatearla.
  const bool mounted = LittleFS.begin(false);
  if (!mounted) {
    D("no monta");
    return FTEST_FAIL;
  }
  const size_t total = LittleFS.totalBytes();
  D("total=%uB", (unsigned)total);
  return (total > 0) ? FTEST_PASS : FTEST_FAIL;
}

#undef D

// ---------------------------------------------------------------------------
typedef FtestStatus (*FtestBodyFn)(char *detail, FtestCascade *cascade);

struct FtestEntry {
  unsigned id;
  FtestBodyFn fn;
};

static const FtestEntry kFtestTable[] = {
    {FTEST_MB_SYSINFO, ftest_sysinfo},
    {FTEST_MB_INA3221, ftest_ina3221},
    {FTEST_MB_STANDBY, ftest_standby},
    {FTEST_MB_CHARGER, ftest_charger},
    {FTEST_MB_POWER_SRC, ftest_power_src},
    {FTEST_MB_SKIN_ADC, ftest_skin_adc},
    {FTEST_MB_EXT_SHT4X, ftest_ext_sht4x},
    {FTEST_MB_SENSORBOARD, ftest_sensorboard},
    {FTEST_MB_SB_STATUS, ftest_sb_status},
    {FTEST_MB_SB_ENV, ftest_sb_env},
    {FTEST_MB_SB_DOOR, ftest_sb_door},
    {FTEST_MB_SB_LIGHT, ftest_sb_light},
    {FTEST_MB_SB_CAMERA, ftest_sb_camera},
    {FTEST_MB_ACTUATORS, ftest_actuators},
    {FTEST_MB_FAN_RPM, ftest_fan_rpm},
    {FTEST_MB_HUMID_USB, ftest_humid_usb},
    {FTEST_MB_BUZZER, ftest_buzzer},
    {FTEST_MB_AFE_SPI, ftest_afe_spi},
    {FTEST_MB_AFE_PROBE, ftest_afe_probe},
    {FTEST_MB_HMI_LINK, ftest_hmi_link},
    {FTEST_MB_GSM_AT, ftest_gsm_at},
    {FTEST_MB_GSM_SIM, ftest_gsm_sim},
    {FTEST_MB_GSM_SIGNAL, ftest_gsm_signal},
    {FTEST_MB_GSM_NET, ftest_gsm_net},
    {FTEST_MB_WIFI, ftest_wifi},
    {FTEST_MB_TB_PROVISION, ftest_tb_provision},
    {FTEST_MB_TIME, ftest_time},
    {FTEST_MB_NVS, ftest_nvs},
    {FTEST_MB_LITTLEFS, ftest_littlefs},
};

FtestStatus ftest_hw_run(unsigned id, char detail[FTEST_DETAIL_MAX + 1],
                         FtestCascade *cascade) {
  detail[0] = '\0';
  for (const FtestEntry &e : kFtestTable) {
    if (e.id == id) {
      return e.fn(detail, cascade);
    }
  }
  snprintf(detail, FTEST_DETAIL_MAX + 1, "id");
  return FTEST_FAIL;
}
