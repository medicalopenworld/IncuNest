// Orquestacion de la configuracion remota con una NVS simulada (openspec:
// mb-remote-config, segunda revision de seguridad).
//
// Lo que se prueba aqui es QUE se persiste y CUANDO: la NVS de 20 KB se
// comparte con mb_cfg y mb_cal, y con el MQTT en claro cualquiera puede publicar
// atributos en bucle. Los contadores g_valueWrites y g_stateWrites son las
// escrituras de NVS que habria hecho la placa.

#include <math.h>
#include <string.h>

#include <unity.h>

#include "modules/config/runtime_config_core.h"

// --- plataforma simulada -----------------------------------------------------

static float g_val[RCFG_COUNT];
static float g_lastPersisted[RCFG_COUNT];
static uint32_t g_now;
static uint32_t g_epoch;
static bool g_therapy;
static bool g_persistOk;
static int g_valueWrites;
static int g_stateWrites;
static RcfgEvent g_ev[256];
static int g_nev;
static RcfgCore core;

static float pf_current(RuntimeConfigId id) { return g_val[id]; }
static void pf_apply_ram(RuntimeConfigId id, float v) { g_val[id] = v; }
static bool pf_persist_value(RuntimeConfigId id, float v) {
  if (!g_persistOk) return false;
  ++g_valueWrites;
  g_lastPersisted[id] = v;
  return true;
}
static bool pf_persist_state(RuntimeConfigId, const RcfgKeyState *) {
  if (!g_persistOk) return false;
  ++g_stateWrites;
  return true;
}
static uint32_t pf_now(void) { return g_now; }
static uint32_t pf_epoch(void) { return g_epoch; }
static bool pf_therapy(void) { return g_therapy; }
static void pf_context(RuntimeConfigRemoteCtx *c) {
  c->air_factory_max = 40.0f;
  c->skin_factory_max = 40.0f;
  c->heater_factory_max = 9.5f;
  c->fan_has_tach = true;
  c->heater_acting = false;
  c->air_setpoint_c = 0.0f;
  c->skin_setpoint_c = 0.0f;
}
static void pf_notify(const RcfgEvent *e) {
  if (g_nev < (int)(sizeof(g_ev) / sizeof(g_ev[0]))) g_ev[g_nev++] = *e;
}

static const RcfgPlatform PF = {pf_current, pf_apply_ram, pf_persist_value,
                                pf_persist_state, pf_now, pf_epoch,
                                pf_therapy, pf_context, pf_notify};

static int events_of(RcfgEventType t) {
  int n = 0;
  for (int i = 0; i < g_nev; ++i) n += (g_ev[i].type == t);
  return n;
}

static void start(bool safety_remote) {
  memset(g_val, 0, sizeof(g_val));
  g_val[RCFG_GPRS_ACT] = 60;
  g_val[RCFG_GPRS_PHOTO] = 180;
  g_val[RCFG_GPRS_STBY] = 3600;
  g_val[RCFG_AIR_TMAX] = 40.0f;
  g_val[RCFG_SKIN_TMAX] = 40.0f;
  g_val[RCFG_HEATER_AMPS] = 9.5f;
  g_val[RCFG_FAN_CTL_PWM] = 130;
  g_val[RCFG_FAN_SUPPLY_PWM] = 255;
  g_val[RCFG_FAN_PID_EN] = 1;
  memset(g_lastPersisted, 0, sizeof(g_lastPersisted));
  g_now = 0;
  g_epoch = 1730000000u;
  g_therapy = false;
  g_persistOk = true;
  g_valueWrites = 0;
  g_stateWrites = 0;
  g_nev = 0;
  rcfg_init(&core, &PF, safety_remote);
}

void setUp(void) { start(false); }
void tearDown(void) {}

// --- A1: NVS solo ve lo aplicado --------------------------------------------

static void test_valores_remotos_rechazados_no_escriben_nada(void) {
  // Rangos, no enteros, no finitos: todos distintos, por suscripcion y por
  // peticion. La NVS no puede enterarse de ninguno.
  const float malos[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 9.5f, 86401, 100000, -1, 0,
                         NAN, INFINITY};
  for (unsigned i = 0; i < sizeof(malos) / sizeof(malos[0]); ++i) {
    g_now += 1000;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, malos[i], true);
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_WIFI, RCFG_GPRS_STBY, malos[i], false);
  }
  TEST_ASSERT_EQUAL_INT(0, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(0, g_stateWrites);
  TEST_ASSERT_EQUAL_FLOAT(60.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].hasRemote);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_STBY].hasRemote);
}

static void test_clave_fuera_de_la_lista_blanca_no_escribe_nada(void) {
  for (int i = 0; i < 40; ++i) {
    g_now += 1000;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_AIR_TMAX, 34.0f + i * 0.1f,
                    true);
  }
  TEST_ASSERT_EQUAL_INT(0, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(0, g_stateWrites);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, g_val[RCFG_AIR_TMAX]);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_AIR_TMAX].hasRemote);
  TEST_ASSERT_TRUE(events_of(RCFG_EV_REJECTED) >= 1);
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_NOT_ALLOWED, g_ev[0].status);
}

static void test_lo_aplicado_se_persiste_una_vez(void) {
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f, true);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_INT(1, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(1, g_stateWrites);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, g_lastPersisted[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].hasRemote);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, core.st[RCFG_GPRS_ACT].lastRemote);
  TEST_ASSERT_EQUAL_UINT8(RCFG_ORIGIN_TB_GPRS + 1, core.st[RCFG_GPRS_ACT].src);
  TEST_ASSERT_EQUAL_UINT32(1730000000u, core.st[RCFG_GPRS_ACT].epoch);
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_CHANGED));
}

static void test_un_reenvio_de_lo_mismo_no_hace_nada(void) {
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f, true);
  const int v = g_valueWrites, s = g_stateWrites, n = g_nev;
  g_now += 5000;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f, false);
  TEST_ASSERT_EQUAL_INT(v, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(s, g_stateWrites);
  TEST_ASSERT_EQUAL_INT(n, g_nev);
}

// --- cubo de fichas de escrituras remotas ------------------------------------

static void test_las_escrituras_remotas_pasan_por_un_cubo_de_fichas(void) {
  // Cuatro cambios validos, uno cada 10 s (el ritmo por clave lo permite). Solo
  // hay fichas para tres.
  const float vals[] = {100, 110, 120, 130};
  for (int i = 0; i < 4; ++i) {
    g_now = (uint32_t)i * 10000u;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, vals[i], true);
  }
  TEST_ASSERT_EQUAL_FLOAT(130.0f, g_val[RCFG_GPRS_ACT]);  // RAM al momento
  TEST_ASSERT_EQUAL_INT(RCFG_NVS_BUCKET_CAP, g_valueWrites);
  TEST_ASSERT_EQUAL_FLOAT(120.0f, g_lastPersisted[RCFG_GPRS_ACT]);

  // Sin ficha, un poll no escribe.
  g_now = 59999;
  rcfg_poll(&core);
  TEST_ASSERT_EQUAL_INT(RCFG_NVS_BUCKET_CAP, g_valueWrites);

  // Con la ficha nueva se persiste lo ULTIMO, no lo intermedio.
  g_now = 60000;
  rcfg_poll(&core);
  TEST_ASSERT_EQUAL_INT(RCFG_NVS_BUCKET_CAP + 1, g_valueWrites);
  TEST_ASSERT_EQUAL_FLOAT(130.0f, g_lastPersisted[RCFG_GPRS_ACT]);

  // Y ya no queda nada sucio.
  g_now = 200000;
  rcfg_poll(&core);
  TEST_ASSERT_EQUAL_INT(RCFG_NVS_BUCKET_CAP + 1, g_valueWrites);
}

static void test_cambios_de_una_clave_a_ritmo_de_ataque_acotan_la_nvs(void) {
  // Un cambio valido por clave cada 10 s durante 10 minutos, tres claves.
  for (uint32_t t = 0; t < 600000u; t += 10000u) {
    g_now = t;
    const float v = 100.0f + (float)(t / 10000u);
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, v, true);
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_PHOTO, v + 1000, true);
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_STBY, v + 2000, true);
    rcfg_poll(&core);
  }
  // 600 s de ataque: tres fichas de salida mas una por minuto = 13 como mucho.
  TEST_ASSERT_TRUE(core.remoteNvsFlushes <= 13);
  TEST_ASSERT_TRUE(g_valueWrites <= 13);
  TEST_ASSERT_TRUE(g_stateWrites <= 13);
}

// --- cambio local y reaplicado ----------------------------------------------

static void test_un_cambio_local_persiste_al_momento_y_marca_override(void) {
  float applied = 0;
  bool persisted = false;
  TEST_ASSERT_EQUAL_INT(
      RCFG_OK, rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_GPRS_ACT, 90.0f,
                                &applied, &persisted));
  TEST_ASSERT_TRUE(persisted);
  TEST_ASSERT_EQUAL_FLOAT(90.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_INT(1, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(1, g_stateWrites);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].overrideLocal);
  TEST_ASSERT_EQUAL_UINT8(RCFG_ORIGIN_WEB + 1, core.st[RCFG_GPRS_ACT].src);
  TEST_ASSERT_EQUAL_INT(0, (int)core.remoteNvsFlushes);  // no gasta fichas
}

static void test_un_cambio_local_no_gasta_las_fichas_remotas(void) {
  for (int i = 0; i < 20; ++i) {
    float a;
    bool p;
    g_now += 100;
    (void)rcfg_apply_local(&core, RCFG_ORIGIN_USB, RCFG_GPRS_ACT, 100.0f + i, &a, &p);
  }
  TEST_ASSERT_EQUAL_INT(20, g_valueWrites);
  // El cubo sigue lleno: tres cambios remotos seguidos se persisten.
  for (int i = 0; i < 3; ++i) {
    g_now += 10000;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_STBY, 4000.0f + i, true);
  }
  TEST_ASSERT_EQUAL_INT(20 + 3, g_valueWrites);
}

static void test_escribir_el_mismo_valor_en_local_no_marca_override(void) {
  float a;
  bool p;
  TEST_ASSERT_EQUAL_INT(RCFG_OK, rcfg_apply_local(&core, RCFG_ORIGIN_WEB,
                                                   RCFG_GPRS_ACT, 60.0f, &a, &p));
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].overrideLocal);
  TEST_ASSERT_EQUAL_INT(0, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(0, g_stateWrites);
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_UNCHANGED));
}

static void test_el_local_gana_a_la_peticion_sin_escribir_nada(void) {
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 120.0f, true);
  float a;
  bool p;
  g_now += 1000;
  (void)rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_GPRS_ACT, 90.0f, &a, &p);
  const int v = g_valueWrites, s = g_stateWrites;

  // Veinte valores remotos DISTINTOS y validos llegan en la peticion: ninguno
  // se aplica y ninguno toca la NVS.
  for (int i = 0; i < 20; ++i) {
    g_now += 1000;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 200.0f + i, false);
  }
  TEST_ASSERT_EQUAL_FLOAT(90.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_INT(v, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(s, g_stateWrites);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].overrideLocal);
  TEST_ASSERT_TRUE(events_of(RCFG_EV_LOCAL_WINS) >= 1);
}

static void test_una_escritura_remota_nueva_levanta_el_override(void) {
  float a;
  bool p;
  (void)rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_GPRS_ACT, 90.0f, &a, &p);
  g_now += 20000;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 130.0f, true);
  TEST_ASSERT_EQUAL_FLOAT(130.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].overrideLocal);
}

// --- M3: un rechazo no levanta el override -----------------------------------

static void test_un_rechazo_remoto_no_levanta_el_override(void) {
  float a;
  bool p;
  (void)rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_GPRS_ACT, 90.0f, &a, &p);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].overrideLocal);
  g_now += 20000;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 5.0f, true);  // < 10 s
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].overrideLocal);
  TEST_ASSERT_EQUAL_FLOAT(90.0f, g_val[RCFG_GPRS_ACT]);
  rcfg_remote_bad_type(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].overrideLocal);
}

static void test_un_aplazado_si_levanta_el_override(void) {
  start(true);  // seguridad remota habilitada, solo para ejercitar las reglas
  float a;
  bool p;
  (void)rcfg_apply_local(&core, RCFG_ORIGIN_USB, RCFG_AIR_TMAX, 38.0f, &a, &p);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_AIR_TMAX].overrideLocal);
  g_therapy = true;
  g_now += 20000;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_WIFI, RCFG_AIR_TMAX, 36.0f, true);
  TEST_ASSERT_EQUAL_FLOAT(38.0f, g_val[RCFG_AIR_TMAX]);  // aplazado
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_DEFERRED));
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_AIR_TMAX].overrideLocal);
}

// --- aplazados ---------------------------------------------------------------

static void test_un_aplazado_no_se_da_por_visto_y_se_aplica_en_standby(void) {
  start(true);
  g_therapy = true;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_WIFI, RCFG_AIR_TMAX, 36.0f, true);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, g_val[RCFG_AIR_TMAX]);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_AIR_TMAX].hasRemote);
  TEST_ASSERT_EQUAL_INT(0, g_valueWrites);

  g_now = 30000;
  rcfg_poll(&core);  // sigue la terapia
  TEST_ASSERT_EQUAL_FLOAT(40.0f, g_val[RCFG_AIR_TMAX]);

  g_therapy = false;
  g_now = 31000;
  rcfg_poll(&core);
  TEST_ASSERT_EQUAL_FLOAT(36.0f, g_val[RCFG_AIR_TMAX]);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_AIR_TMAX].hasRemote);
  TEST_ASSERT_EQUAL_FLOAT(36.0f, core.st[RCFG_AIR_TMAX].lastRemote);
}

static void test_un_cambio_antes_de_10_segundos_queda_pendiente(void) {
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f, true);
  g_now = 4000;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 110.0f, true);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_DEFERRED));
  g_now = 10000;
  rcfg_poll(&core);
  TEST_ASSERT_EQUAL_FLOAT(110.0f, g_val[RCFG_GPRS_ACT]);
}

static void test_tras_reiniciar_la_peticion_reintenta_lo_aplazado(void) {
  start(true);
  g_therapy = true;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_WIFI, RCFG_AIR_TMAX, 36.0f, true);
  // Reinicio: estado nuevo, nada pendiente, nada persistido de ese valor.
  start(true);
  g_therapy = false;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_WIFI, RCFG_AIR_TMAX, 36.0f, false);
  TEST_ASSERT_EQUAL_FLOAT(36.0f, g_val[RCFG_AIR_TMAX]);
}

// --- B3: limite de avisos -----------------------------------------------------

static void test_los_rechazos_repetidos_avisan_una_vez_por_minuto(void) {
  for (int i = 0; i < 50; ++i) {
    g_now = (uint32_t)i * 10u;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, (float)(1 + i % 9),
                    true);
  }
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_REJECTED));
  TEST_ASSERT_EQUAL_UINT32(0, g_ev[0].suppressed);

  g_now = 60000;
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 3.0f, true);
  TEST_ASSERT_EQUAL_INT(2, events_of(RCFG_EV_REJECTED));
  TEST_ASSERT_EQUAL_UINT32(49, g_ev[g_nev - 1].suppressed);
}

static void test_los_avisos_son_por_clave(void) {
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 1.0f, true);
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_PHOTO, 1.0f, true);
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_STBY, 1.0f, true);
  TEST_ASSERT_EQUAL_INT(3, events_of(RCFG_EV_REJECTED));
}

static void test_el_tipo_equivocado_pasa_por_el_mismo_limitador(void) {
  for (int i = 0; i < 10; ++i) {
    g_now += 100;
    rcfg_remote_bad_type(&core, RCFG_ORIGIN_TB_WIFI, RCFG_GPRS_ACT);
  }
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_REJECTED));
  TEST_ASSERT_EQUAL_INT(RCFG_REJ_WRONG_TYPE, g_ev[0].status);
  TEST_ASSERT_EQUAL_INT(0, g_stateWrites);
}

// --- M2: la web es estricta ---------------------------------------------------

static void test_la_web_rechaza_un_corte_fuera_de_rango_en_vez_de_recortarlo(void) {
  float a = -1;
  bool p = true;
  TEST_ASSERT_EQUAL_INT(
      RCFG_REJ_RANGE,
      rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_AIR_TMAX, 45.0f, &a, &p));
  TEST_ASSERT_EQUAL_FLOAT(40.0f, g_val[RCFG_AIR_TMAX]);
  TEST_ASSERT_EQUAL_INT(0, g_valueWrites);
  TEST_ASSERT_EQUAL_INT(0, g_stateWrites);
  TEST_ASSERT_EQUAL_INT(
      RCFG_OK, rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_AIR_TMAX, 38.0f, &a,
                                &p));
  TEST_ASSERT_EQUAL_FLOAT(38.0f, g_val[RCFG_AIR_TMAX]);
}

static void test_si_la_nvs_falla_un_cambio_local_no_cambia_nada(void) {
  g_persistOk = false;
  float a;
  bool p = true;
  (void)rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_GPRS_ACT, 90.0f, &a, &p);
  TEST_ASSERT_FALSE(p);
  TEST_ASSERT_EQUAL_FLOAT(60.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].overrideLocal);
  TEST_ASSERT_EQUAL_INT(1, events_of(RCFG_EV_PERSIST_FAIL));
}

static void test_un_cambio_local_cancela_la_persistencia_remota_pendiente(void) {
  for (int i = 0; i < 4; ++i) {  // el cuarto se queda sin ficha: sucio
    g_now = (uint32_t)i * 10000u;
    rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f + i, true);
  }
  float a;
  bool p;
  g_now = 35000;
  (void)rcfg_apply_local(&core, RCFG_ORIGIN_WEB, RCFG_GPRS_ACT, 77.0f, &a, &p);
  const int v = g_valueWrites;
  g_now = 200000;
  rcfg_poll(&core);
  TEST_ASSERT_EQUAL_INT(v, g_valueWrites);  // nada rancio que escribir
  TEST_ASSERT_EQUAL_FLOAT(77.0f, g_lastPersisted[RCFG_GPRS_ACT]);
}

// --- B4: version del blob -----------------------------------------------------

static void test_un_blob_con_otra_version_se_ignora(void) {
  RcfgKeyState s;
  memset(&s, 0, sizeof(s));
  s.lastRemote = 100.0f;
  s.hasRemote = 1;
  s.overrideLocal = 1;
  s.version = 0;  // formato sin version
  rcfg_load_state(&core, RCFG_GPRS_ACT, &s);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].hasRemote);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].overrideLocal);
  s.version = 99;
  rcfg_load_state(&core, RCFG_GPRS_ACT, &s);
  TEST_ASSERT_EQUAL_UINT8(0, core.st[RCFG_GPRS_ACT].hasRemote);
}

static void test_un_blob_de_la_version_actual_se_respeta(void) {
  RcfgKeyState s;
  memset(&s, 0, sizeof(s));
  s.lastRemote = 100.0f;
  s.hasRemote = 1;
  s.version = RCFG_STATE_VERSION;
  rcfg_load_state(&core, RCFG_GPRS_ACT, &s);
  TEST_ASSERT_EQUAL_UINT8(1, core.st[RCFG_GPRS_ACT].hasRemote);
  // Y por tanto un reenvio de 100 no hace nada.
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f, false);
  TEST_ASSERT_EQUAL_FLOAT(60.0f, g_val[RCFG_GPRS_ACT]);
  TEST_ASSERT_EQUAL_INT(0, g_valueWrites);
}

static void test_el_estado_que_se_guarda_lleva_la_version(void) {
  rcfg_remote_key(&core, RCFG_ORIGIN_TB_GPRS, RCFG_GPRS_ACT, 100.0f, true);
  TEST_ASSERT_EQUAL_UINT8(RCFG_STATE_VERSION, core.st[RCFG_GPRS_ACT].version);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_valores_remotos_rechazados_no_escriben_nada);
  RUN_TEST(test_clave_fuera_de_la_lista_blanca_no_escribe_nada);
  RUN_TEST(test_lo_aplicado_se_persiste_una_vez);
  RUN_TEST(test_un_reenvio_de_lo_mismo_no_hace_nada);
  RUN_TEST(test_las_escrituras_remotas_pasan_por_un_cubo_de_fichas);
  RUN_TEST(test_cambios_de_una_clave_a_ritmo_de_ataque_acotan_la_nvs);
  RUN_TEST(test_un_cambio_local_persiste_al_momento_y_marca_override);
  RUN_TEST(test_un_cambio_local_no_gasta_las_fichas_remotas);
  RUN_TEST(test_escribir_el_mismo_valor_en_local_no_marca_override);
  RUN_TEST(test_el_local_gana_a_la_peticion_sin_escribir_nada);
  RUN_TEST(test_una_escritura_remota_nueva_levanta_el_override);
  RUN_TEST(test_un_rechazo_remoto_no_levanta_el_override);
  RUN_TEST(test_un_aplazado_si_levanta_el_override);
  RUN_TEST(test_un_aplazado_no_se_da_por_visto_y_se_aplica_en_standby);
  RUN_TEST(test_un_cambio_antes_de_10_segundos_queda_pendiente);
  RUN_TEST(test_tras_reiniciar_la_peticion_reintenta_lo_aplazado);
  RUN_TEST(test_los_rechazos_repetidos_avisan_una_vez_por_minuto);
  RUN_TEST(test_los_avisos_son_por_clave);
  RUN_TEST(test_el_tipo_equivocado_pasa_por_el_mismo_limitador);
  RUN_TEST(test_la_web_rechaza_un_corte_fuera_de_rango_en_vez_de_recortarlo);
  RUN_TEST(test_si_la_nvs_falla_un_cambio_local_no_cambia_nada);
  RUN_TEST(test_un_cambio_local_cancela_la_persistencia_remota_pendiente);
  RUN_TEST(test_un_blob_con_otra_version_se_ignora);
  RUN_TEST(test_un_blob_de_la_version_actual_se_respeta);
  RUN_TEST(test_el_estado_que_se_guarda_lleva_la_version);
  return UNITY_END();
}
