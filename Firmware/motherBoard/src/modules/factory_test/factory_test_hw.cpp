// Cuerpos de los 30 tests de la motherBoard (design.md D10, mb-factory-test).
// Una funcion estatica por id, tabla {id, fn} al final, ftest_hw_run() como
// unico punto de entrada. Fail-safe por diseno: ninguna lectura invalida
// levanta una excepcion, siempre se traduce a FAIL con detail.
//
// Sin entorno de test (hardware real, USB, I2C, PWM): verificacion manual en
// banco documentada en el commit de este cambio.
#include <Preferences.h>
#include <LittleFS.h>
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
  BQ25730_Status st;
  const bool ok = charge_status(&st);
  D("vb=%dmV vs=%dmV vu=%dmV i=%dmA", (int)st.vbat_mv, (int)st.vsys_mv,
    (int)st.vbus_mv, (int)st.ichg_ma);
  return ok ? FTEST_PASS : FTEST_FAIL;
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
  return (t < 1.0f || t > 60.0f) ? FTEST_FAIL : FTEST_PASS;
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
  if (t < -10.0f || t > 60.0f) return FTEST_FAIL;
  if (h < 0.0f || h > 100.0f) return FTEST_FAIL;
  return FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 7: SENSOR_SRC -- decide la cascada SKIP de los SB_* (design.md D10).
static FtestStatus ftest_sensor_src(char *detail, FtestCascade *cascade) {
  const bool ok = (sensorSourceGet() == SENSOR_SOURCE_SENSORBOARD);
  cascade->sensor_src = ok ? FTEST_DEP_OK : FTEST_DEP_FAILED;
  if (!ok) {
    D("i2c2 responde");
    return FTEST_FAIL;
  }
  detail[0] = '\0';
  return FTEST_PASS;
}

static bool sb_skip_if_no_sensorboard(const FtestCascade *cascade, char *detail) {
  if (cascade->sensor_src == FTEST_DEP_FAILED) {
    D("sin sensorboard");
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// 8: SB_LINK
static FtestStatus ftest_sb_link(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_sensorboard(cascade, detail)) return FTEST_SKIP;

  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < FTEST_MB_RESPONSE_TIMEOUT_MS) {
    SbSnapshot s;
    sensorboard_get_snapshot(&s);
    if (sensorboard_comm_connected() && s.link_ok) {
      D("ok");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 9: SB_STATUS
static FtestStatus ftest_sb_status(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_sensorboard(cascade, detail)) return FTEST_SKIP;

  sensorboard_status_request();
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
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  SbSnapshot s;
  sensorboard_get_snapshot(&s);
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
// 10: SB_ENV
static FtestStatus ftest_sb_env(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_sensorboard(cascade, detail)) return FTEST_SKIP;

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

  float mn = s.temp.value[0], mx = s.temp.value[0], sum = 0.0f;
  for (int i = 0; i < 3; i++) {
    if (s.temp.value[i] < mn) mn = s.temp.value[i];
    if (s.temp.value[i] > mx) mx = s.temp.value[i];
    sum += s.temp.value[i];
  }
  const float spread = mx - mn;
  const float mean = sum / 3.0f;

  bool haveExt = false;
  float diff = 0.0f;
  if (ambientSensorPresent && updateAmbientSensor()) {
    haveExt = true;
    diff = fabsf(mean - in3.temperature[AMBIENT_DIGITAL_TEMP_SENSOR]);
  }

  D("sp=%.1f df=%.1f", spread, diff);
  if (spread > FTEST_SB_SPREAD_MAX_C) return FTEST_FAIL;
  if (haveExt && diff > FTEST_SB_VS_EXT_MAX_C) return FTEST_FAIL;
  return FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 11: SB_DOOR (WAIT)
static FtestStatus ftest_sb_door(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_sensorboard(cascade, detail)) return FTEST_SKIP;

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
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 12: SB_LIGHT (WAIT)
static FtestStatus ftest_sb_light(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_sensorboard(cascade, detail)) return FTEST_SKIP;

  float base = -1.0f;
  {
    const uint32_t start = millis();
    while ((uint32_t)(millis() - start) < 2000) {
      SbSnapshot s;
      sensorboard_get_snapshot(&s);
      if (s.lux_valid) base = s.lux;
      if (ftest_abort_requested()) {
        D("abort");
        return FTEST_SKIP;
      }
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
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout b=%.0f", base);
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 13: SB_CAMERA
static FtestStatus ftest_sb_camera(char *detail, FtestCascade *cascade) {
  if (sb_skip_if_no_sensorboard(cascade, detail)) return FTEST_SKIP;

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
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 14: ACTUATORS -- reusa actuatorsTest() tal cual (design.md, Non-Goals).
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
// 15: FAN_RPM -- depende del resultado de ACTUATORS (cascada).
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
  return (in3.fan_rpm < FAN_MIN_RPM) ? FTEST_FAIL : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 16: HUMID_USB -- acceso directo a USB_EN/USB_FAULT, no reusa
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
  return (mA <= FTEST_HUMID_MIN_MA) ? FTEST_FAIL : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 17: BUZZER -- con microfono (SensorBoard) o CONFIRM del operario.
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
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    ledcWrite(BUZZER_PWM_CHANNEL, 0);
    D("base=%.0f pico=%.0f", base, peak);
    if (!gotNew) return FTEST_FAIL;
    return ((peak - base) >= FTEST_BUZZER_DBA_DELTA) ? FTEST_PASS : FTEST_FAIL;
  }

  // Sin microfono: zumba y pregunta al operario.
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
// 18: AFE_SPI
static FtestStatus ftest_afe_spi(char *detail, FtestCascade *) {
  const AFE4490TimingConfig tc = afe.getTimingConfig();
  const uint32_t diag = afe.runAfeDiagnostics();
  D("t2=%u diag=0x%lX", (unsigned)tc.t2, (unsigned long)diag);
  const bool ok = (tc.t1 < tc.t2) && (tc.t2 != 0) && (tc.t2 != 0xFFFF);
  return ok ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 19: AFE_PROBE (opcional)
static FtestStatus ftest_afe_probe(char *detail, FtestCascade *) {
  const ProbeState st = g_spo2_data.probe_state;
  D("state=%d", (int)st);
  return (st == ProbeState::PROBE_DISCONNECTED) ? FTEST_SKIP : FTEST_PASS;
}

// ---------------------------------------------------------------------------
// 20: HMI_LINK
static FtestStatus ftest_hmi_link(char *detail, FtestCascade *) {
  const uint32_t age = (uint32_t)(millis() - g_lastHmiLineMs);
  D("seen=%d age=%ums", g_hmiEverSeen, (unsigned)age);
  return (g_hmiEverSeen && age < 5000) ? FTEST_PASS : FTEST_FAIL;
}

// ---------------------------------------------------------------------------
// 21-24: GSM, pasivos sobre el estado de GPRS_Task (design.md D7).
static FtestStatus ftest_gsm_at(char *detail, FtestCascade *) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 45000) {
    if (GPRS.powerUp) {
      D("powerUp");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("timeout");
  return FTEST_FAIL;
}

static FtestStatus ftest_gsm_sim(char *detail, FtestCascade *cascade) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 15000) {
    if (GPRS.CCID.length() > 0) {
      cascade->gsm_sim = FTEST_DEP_OK;
      D("ok");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      cascade->gsm_sim = FTEST_DEP_FAILED;
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  cascade->gsm_sim = FTEST_DEP_FAILED;
  D("sin ccid");
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
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("csq=%d", GPRS.CSQ);
  return FTEST_FAIL;
}

static FtestStatus ftest_gsm_net(char *detail, FtestCascade *cascade) {
  if (cascade->gsm_sim == FTEST_DEP_FAILED) {
    D("sin sim");
    return FTEST_SKIP;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 60000) {
    if (GPRS.post) {
      D("ok");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("sin red");
  return FTEST_SKIP;
}

// ---------------------------------------------------------------------------
// 25: WIFI (opcional)
static FtestStatus ftest_wifi(char *detail, FtestCascade *) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 30000) {
    if (WIFIIsConnected()) {
      D("rssi=%d", (int)WiFi.RSSI());
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  D("sin AP");
  return FTEST_SKIP;
}

// ---------------------------------------------------------------------------
// 26: TB_PROVISION (opcional, design.md D8)
static FtestStatus ftest_tb_provision(char *detail, FtestCascade *) {
  if (in3.serialNumber == 0) {
    D("sin serie");
    return FTEST_SKIP;
  }
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < 30000) {
    if (WIFIIsConnectedToServer()) {
      D("wifi");
      return FTEST_PASS;
    }
    if (GPRSIsConnectedToServer()) {
      D("gprs");
      return FTEST_PASS;
    }
    if (ftest_abort_requested()) {
      D("abort");
      return FTEST_SKIP;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  if (WIFIIsConnected() || GPRSIsAttached()) {
    D("sin servidor");
    return FTEST_FAIL;
  }
  D("sin transporte");
  return FTEST_SKIP;
}

// ---------------------------------------------------------------------------
// 27: TIME (opcional)
static FtestStatus ftest_time(char *detail, FtestCascade *) {
  if (time(nullptr) < 1609459200) {
    D("sin hora");
    return FTEST_SKIP;
  }
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

// ---------------------------------------------------------------------------
// 28: NVS
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
// 29: LITTLEFS
static FtestStatus ftest_littlefs(char *detail, FtestCascade *) {
  const bool mounted = LittleFS.begin(true);
  const size_t total = mounted ? LittleFS.totalBytes() : 0;
  D("total=%uB", (unsigned)total);
  return (mounted && total > 0) ? FTEST_PASS : FTEST_FAIL;
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
    {FTEST_MB_SENSOR_SRC, ftest_sensor_src},
    {FTEST_MB_SB_LINK, ftest_sb_link},
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
