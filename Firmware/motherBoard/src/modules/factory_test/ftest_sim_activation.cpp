#include "ftest_sim_activation.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "factory_test.h" // FTEST_DETAIL_MAX (shared/)
#include "main.h"         // logI / logE
#include "protocol/Credentials_public.h"

// ---------------------------------------------------------------------------
// Contrato de la API de Onomondo (comprobado contra la API viva el
// 2026-09-06, https://docs.onomondo.com/readme/sims):
//
//   GET   /sims/{id}   -> 200 con un JSON que trae "activated": true|false
//   PATCH /sims/{id}   con {"activated":true} -> 2xx
//
// {id} acepta el ICCID directamente; no hace falta traducirlo al SIM ID
// interno de 9 digitos que enseña el portal de Onomondo. La validacion del
// propio endpoint es ^[0-9]{9}$|^894573[0-9]{13,14}$, asi que solo valen
// ICCIDs de Onomondo.
//
// OJO con la longitud: GPRS.CCID NO es el ICCID completo. GPRS.cpp:658 hace
//     GPRS.CCID.remove(GPRS.CCID.length() - 1);
// justo despues de modem.getSimCCID(), asi que lo que llega aqui son 19
// digitos y no los 20 del ICCID real (al de la SIM 005120030,
// 89457300000051200304, le falta el 4 final). El endpoint acepta las dos
// formas -- el {13,14} de la regex es exactamente eso -- y se comprobo
// contra la API viva que 19 digitos, 20 digitos y el SIM ID de 9 devuelven
// la MISMA SIM. Si algun dia se quita ese recorte en GPRS.cpp, esto sigue
// funcionando; lo que no hay que hacer es "arreglarlo" añadiendo un digito
// aqui.
//
// La cabecera es `authorization: <clave>` CRUDA, sin prefijo "Bearer". La
// spec OpenAPI publicada etiqueta el esquema como bearerAuth, pero mandar
// "Bearer <clave>" devuelve 401.
// ---------------------------------------------------------------------------

static const char *const kHost = "api.onomondo.com";
static const uint16_t kPort = 443;

// 10 s por peticion: el peor caso del test entero (GET + reintento +
// PATCH + reintento = 4 peticiones mas dos esperas) tiene que caber en
// FTEST_SIM_ACT_TIMEOUT_MS, y ese a su vez bajo la cota de 90 s por test
// del runner (FTEST_TEST_TIMEOUT_MS).
#define SIM_HTTP_TIMEOUT_MS 10000u
#define SIM_RETRY_DELAY_MS 1500u
// 8192 es el mismo tamano que usa driveUploadTask (DriveUpload.cpp), la otra
// tarea de esta placa que abre un WiFiClientSecure; el handshake TLS no cabe
// en los 4096 habituales del resto de tareas.
#define SIM_TASK_STACK 8192
#define SIM_ICCID_MAX 24

static volatile FtestSimState s_state = FTEST_SIM_IDLE;
static char s_detail[FTEST_DETAIL_MAX + 1] = "";
static char s_iccid[SIM_ICCID_MAX] = "";
static TaskHandle_t s_task = nullptr;

// ---------------------------------------------------------------------------

// Marca de tiempo UTC para la traza por unidad. Si el reloj todavia no esta
// en hora (mismo umbral que usa el test `time`) cae a los ms desde el
// arranque, que al menos ordena los eventos dentro de la tanda.
static String utcStamp(void) {
  const time_t now = time(nullptr);
  if (now >= 1609459200) {
    struct tm tmv;
    gmtime_r(&now, &tmv);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return String(buf);
  }
  return String("+") + String(millis()) + "ms";
}

static void finish(FtestSimState st, const char *detail) {
  snprintf(s_detail, sizeof(s_detail), "%s", detail);
  // Barrera antes de publicar el estado: s_detail NO es volatile y esta tarea
  // corre en el core 1 mientras la tarea FTEST lee desde el otro core. Sin
  // esto, nada impide que la escritura de s_state se vea antes que la de
  // s_detail y que el pasivo publique un detail vacio o a medias. `volatile`
  // en s_state solo ordena los accesos volatiles entre si, no respecto a un
  // buffer normal.
  __sync_synchronize();
  s_state = st;
}

// Busca "activated" en el cuerpo y devuelve el booleano que lo sigue.
//
// Escaneo textual y no un parser JSON a proposito: la respuesta puede llegar
// con Transfer-Encoding: chunked, y entonces el cuerpo que acumulamos lleva
// intercaladas las lineas con el tamano de cada trozo. Eso rompe a un parser
// JSON, pero no afecta a esta busqueda: las lineas de tamano son numeros en
// hexadecimal y no contienen la subcadena que buscamos.
static bool parseActivated(const String &body, bool *out) {
  const int key = body.indexOf("\"activated\"");
  if (key < 0) return false;
  const int colon = body.indexOf(':', key);
  if (colon < 0) return false;
  int p = colon + 1;
  while (p < (int)body.length() && (body[p] == ' ' || body[p] == '\t')) p++;
  const char *q = body.c_str() + p;
  if (strncmp(q, "true", 4) == 0) {
    *out = true;
    return true;
  }
  if (strncmp(q, "false", 5) == 0) {
    *out = false;
    return true;
  }
  return false;
}

// Una peticion. Devuelve false si no se pudo hablar con el servidor (DNS,
// TCP, TLS o respuesta sin linea de estado); en ese caso *status queda a 0.
static bool httpRequest(const char *method, const char *path,
                        const char *jsonBody, int *status, String *respBody) {
  *status = 0;
  *respBody = "";

  WiFiClientSecure client;
  // setInsecure(): no se valida el certificado del servidor, igual que hace
  // DriveUpload.cpp. Aceptable para un paso que solo corre en la red de
  // fabrica, pero conviene saber que un MITM en esa red podria responder por
  // Onomondo. Si algun dia importa, aqui es donde iria setCACert().
  client.setInsecure();
  client.setTimeout(SIM_HTTP_TIMEOUT_MS / 1000);
  if (!client.connect(kHost, kPort)) return false;

  client.printf("%s %s HTTP/1.1\r\n", method, path);
  client.printf("Host: %s\r\n", kHost);
  // Clave cruda, sin "Bearer". Va al socket y a ningun otro sitio.
  client.printf("authorization: %s\r\n", ONOMONDO_API_KEY);
  client.print("accept: application/json\r\n");
  client.print("user-agent: IncuNest-FactoryTest/1.0\r\n");
  if (jsonBody != nullptr) {
    client.print("content-type: application/json\r\n");
    client.printf("content-length: %u\r\n", (unsigned)strlen(jsonBody));
  }
  client.print("connection: close\r\n\r\n");
  if (jsonBody != nullptr) client.print(jsonBody);

  bool inBody = false;
  const uint32_t t0 = millis();
  while ((client.connected() || client.available()) &&
         (millis() - t0) < SIM_HTTP_TIMEOUT_MS) {
    if (!client.available()) {
      delay(5);
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();
    if (!inBody) {
      if (line.length() == 0) {
        inBody = true;
        continue;
      }
      if (line.startsWith("HTTP/")) *status = line.substring(9, 12).toInt();
    } else {
      *respBody += line;
      if (respBody->length() >= 512) break; // el JSON de una SIM cabe de sobra
    }
  }
  client.stop();
  return *status != 0;
}

// 429 y 5xx son transitorios: la misma peticion puede salir bien si se
// repite. Los 4xx restantes son deterministas y no se reintentan.
static bool isTransient(int status) {
  return status == 429 || (status >= 500 && status <= 599);
}

// Una peticion con UN reintento ante fallo de red o respuesta transitoria.
static bool requestWithRetry(const char *method, const char *path,
                             const char *jsonBody, int *status,
                             String *respBody) {
  for (int attempt = 1; attempt <= 2; ++attempt) {
    const bool answered = httpRequest(method, path, jsonBody, status, respBody);
    if (answered && !isTransient(*status)) return true;
    if (attempt == 1) {
      logE(String("[FTEST] sim_act: ") + method + " fallido (status=" +
           String(*status) + "), reintentando una vez");
      vTaskDelay(pdMS_TO_TICKS(SIM_RETRY_DELAY_MS));
    }
  }
  return false;
}

static void simActivationTask(void *) {
  char path[16 + SIM_ICCID_MAX];
  snprintf(path, sizeof(path), "/sims/%s", s_iccid);

  int status = 0;
  String body;
  char msg[FTEST_DETAIL_MAX + 1];

  if (!requestWithRetry("GET", path, nullptr, &status, &body) || status != 200) {
    snprintf(msg, sizeof(msg), "get %d", status);
    finish(FTEST_SIM_ERROR, msg);
  } else {
    bool activated = false;
    if (!parseActivated(body, &activated)) {
      finish(FTEST_SIM_ERROR, "resp sin activated");
    } else if (activated) {
      finish(FTEST_SIM_ALREADY_ACTIVE, "ya activada");
    } else if (!requestWithRetry("PATCH", path, "{\"activated\":true}", &status,
                                 &body) ||
               status < 200 || status > 299) {
      snprintf(msg, sizeof(msg), "patch %d", status);
      finish(FTEST_SIM_ERROR, msg);
    } else {
      finish(FTEST_SIM_ACTIVATED, "activada");
    }
  }

  // Traza por unidad (requisito de fabrica): ICCID, resultado y marca de
  // tiempo. La clave NO aparece.
  const char *outcome = (s_state == FTEST_SIM_ALREADY_ACTIVE) ? "ya activada"
                        : (s_state == FTEST_SIM_ACTIVATED)    ? "activada"
                                                              : "ERROR";
  const String line = String("[FTEST] sim_act iccid=") + s_iccid +
                      " resultado=" + outcome + " (" + s_detail + ") ts=" +
                      utcStamp();
  if (s_state == FTEST_SIM_ERROR) {
    logE(line);
  } else {
    logI(line);
  }

  s_task = nullptr;
  vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------

bool ftest_sim_activation_start(const char *iccid) {
  if (s_state != FTEST_SIM_IDLE || s_task != nullptr) return false;
  if (iccid == nullptr || iccid[0] == '\0') return false;

  // Sin clave real no se intenta siquiera el TLS: FAIL inmediato con un motivo
  // legible. Es lo que pasa en cualquier build hecho fuera de fabrica, donde
  // Credentials.h no existe y Credentials_public.h da el valor dummy.
  if (strcmp(ONOMONDO_API_KEY, ONOMONDO_API_KEY_DUMMY) == 0) {
    finish(FTEST_SIM_ERROR, "sin key");
    logE("[FTEST] sim_act: ONOMONDO_API_KEY sin definir en Credentials.h");
    return false;
  }

  snprintf(s_iccid, sizeof(s_iccid), "%s", iccid);
  s_detail[0] = '\0';
  s_state = FTEST_SIM_RUNNING;

  if (xTaskCreatePinnedToCore(simActivationTask, "FTEST_SIM", SIM_TASK_STACK,
                              nullptr, 1, &s_task, 1) != pdPASS) {
    s_task = nullptr;
    finish(FTEST_SIM_ERROR, "sin tarea");
    logE("[FTEST] sim_act: no se pudo crear la tarea");
    return false;
  }
  return true;
}

FtestSimState ftest_sim_activation_state(void) { return s_state; }

const char *ftest_sim_activation_detail(void) { return s_detail; }

void ftest_sim_activation_reset(void) {
  if (s_task != nullptr) return; // tarea en vuelo: no se toca
  s_state = FTEST_SIM_IDLE;
  s_detail[0] = '\0';
  s_iccid[0] = '\0';
}
