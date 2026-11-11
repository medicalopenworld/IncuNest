#include "DriveUpload.h"
#include "main.h"
#include "modules/util/system_clock.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// LittleFS mount prefix (default in Arduino-ESP32). POSIX open/stat need the
// fully-qualified vfs path; LittleFS.* methods take the relative one.
#define LFS_MOUNT "/littlefs"

// The raw PPG capture (one /pox_<ms>.csv per 60 s window, 500 samples/s,
// uploaded to <SN>/2-Pulsioximetry/) was REMOVED in 18.56.0. Writing to the
// flash while the AFE4490 is sampling stalls the CPU cache for every page
// program / sector erase, the library's DRDY handshake is a binary semaphore
// and the AFE has no FIFO, so every sample that arrives during a flash write
// is lost without trace. Measured on the bench (2026-11-02, V18, patient
// simulator at 135 bpm): ~11 % of the samples lost with the probe applied,
// HR shown 152 bpm. HR is "samples between beats" scaled by the nominal
// 500 Hz, so fewer samples inflate it; SpO2 and PI are ratios of signals that
// lose the same samples and were not affected. Lowering the capture rate only
// reduces how often the flash is written, not the loss per write, so there is
// no rate at which this design keeps the sample stream complete. A RAM-only
// capture published over the network (like PpgSnapshot) is the replacement.
//
// What stays: the upload task for the crash reports (<SN>/1-Logs/), which are
// written at boot after a failure, never while measuring.

extern IncuNest_parameters in3;

static QueueHandle_t s_upload_queue = nullptr;
static TaskHandle_t  s_upload_task  = nullptr;

static bool s_time_synced = false;

// One request = one upload. `source_path` holds the log on LittleFS.
struct DriveUploadRequest {
  char source_path[32];
  char drive_folder[32];
  char drive_filename[96];
};

#define DRIVE_UPLOAD_QUEUE_LEN 4

// Silent remove: LittleFS.exists() + LittleFS.remove() both log [E] at the
// framework level when the file is missing. We clean pre-emptively so probe
// via POSIX stat() on the mounted path, which does not log.
// Un informe de caida NO se borra al fallar la subida.
//
// Es el unico artefacto que no se puede volver a generar: son las ultimas
// lineas antes de morir, y si se pierden hay que esperar a que la averia se
// repita.
//
// El borrado indiscriminado dejaba la placa SIEMPRE sin informes: se escribia
// el fichero, se encolaba para Drive, la subida no era posible (sin red o sin
// Drive configurado) y la tarea lo borraba igual, incluso por la rama
// "network not ready, dropping upload". Comprobado el 2026-09-21: seis caidas
// forzadas seguidas y en la placa seguian solo cuatro informes de la 18.2.
//
// No hace falta limite extra: keepNewestCrashLog() ya los acota a
// DRIVE_CRASH_LOG_RETENTION_CAP (5) en cada arranque, o sea 22 KB como mucho
// en una particion de 2 MB. Y se pueden descargar con GET /debug/crash.
static bool isCrashReport(const char *path) {
  if (path == nullptr) {
    return false;
  }
  const char *base = (path[0] == '/') ? path + 1 : path;
  return strncmp(base, "crash_", 6) == 0;
}

static void removeIfExists(const char *path) {
  char full[40];
  snprintf(full, sizeof(full), LFS_MOUNT "%s", path);
  struct stat st;
  if (stat(full, &st) == 0)
    LittleFS.remove(path);
}

// crash_mb_/crash_hmi_ logs are normally deleted by driveUploadTask once
// uploaded (or once it confirms there is no network). But a crash loop
// reboots faster than that task can ever get scheduled, so logs from a
// crash loop pile up forever and each one adds to every future boot's
// directory scan. Track only the newest DRIVE_CRASH_LOG_RETENTION_CAP
// per prefix during that scan and delete the rest outright.
#define DRIVE_CRASH_LOG_RETENTION_CAP 5

struct CrashLogEntry {
  char name[32];
  unsigned long key; // numeric suffix embedded in the filename (millis() at write time)
};

static void keepNewestCrashLog(CrashLogEntry *kept, int *count, const char *name,
                                unsigned long key) {
  if (*count < DRIVE_CRASH_LOG_RETENTION_CAP) {
    snprintf(kept[*count].name, sizeof(kept[*count].name), "%s", name);
    kept[*count].key = key;
    (*count)++;
    return;
  }
  int oldestIdx = 0;
  for (int i = 1; i < *count; i++)
    if (kept[i].key < kept[oldestIdx].key)
      oldestIdx = i;
  if (key > kept[oldestIdx].key) {
    LittleFS.remove(String("/") + kept[oldestIdx].name);
    snprintf(kept[oldestIdx].name, sizeof(kept[oldestIdx].name), "%s", name);
    kept[oldestIdx].key = key;
  } else {
    LittleFS.remove(String("/") + name);
  }
}

// ─── Base64 encoder (3 bytes -> 4 chars) ─────────────────────────────────────
static const char B64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64Block(const uint8_t *in, int len, char *out) {
  out[0] = B64_ALPHABET[(in[0] >> 2) & 0x3F];
  out[1] = B64_ALPHABET[((in[0] & 0x03) << 4) | ((len > 1 ? in[1] : 0) >> 4)];
  out[2] = (len > 1)
              ? B64_ALPHABET[((in[1] & 0x0F) << 2) |
                             ((len > 2 ? in[2] : 0) >> 6)]
              : '=';
  out[3] = (len > 2) ? B64_ALPHABET[in[2] & 0x3F] : '=';
}

static void parseLocation(const String &url, String &host, String &path) {
  String u = url;
  if (u.startsWith("https://"))
    u = u.substring(8);
  int sl = u.indexOf('/');
  if (sl < 0) {
    host = u;
    path = "/";
  } else {
    host = u.substring(0, sl);
    path = u.substring(sl);
  }
}

// ─── Streamed POST: JSON envelope around base64(csv) ─────────────────────────
static bool streamPost(const String &host, const String &path, File &csv,
                       const String &prefix, const String &suffix,
                       size_t bodyLen, int &outStatus, String &outLocation,
                       String &outBody) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(30);
  if (!client.connect(host.c_str(), 443)) {
    logDrive("HTTPS connect failed");
    return false;
  }

  client.printf("POST %s HTTP/1.1\r\n", path.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.println("Content-Type: application/json");
  client.printf("Content-Length: %u\r\n", (unsigned)bodyLen);
  client.println("Connection: close");
  client.println();
  client.print(prefix);

  csv.seek(0);
  constexpr int OUT_CAP = 1200;
  char outBuf[OUT_CAP + 4];
  int outPos = 0;
  while (csv.available()) {
    uint8_t in[3];
    int n = csv.readBytes((char *)in, 3);
    b64Block(in, n, &outBuf[outPos]);
    outPos += 4;
    if (outPos >= OUT_CAP) {
      client.write((const uint8_t *)outBuf, outPos);
      outPos = 0;
      yield();
    }
  }
  if (outPos > 0)
    client.write((const uint8_t *)outBuf, outPos);
  client.print(suffix);

  outStatus = 0;
  outLocation = "";
  outBody = "";
  bool inBody = false;
  unsigned long t0 = millis();
  while ((client.connected() || client.available()) &&
         millis() - t0 < 20000) {
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
      if (line.startsWith("HTTP/"))
        outStatus = line.substring(9, 12).toInt();
      if (line.startsWith("Location:")) {
        outLocation = line.substring(9);
        outLocation.trim();
      }
    } else {
      outBody += line;
      if (outBody.length() >= 512)
        break;
    }
  }
  client.stop();
  return true;
}

// GAS always answers 302 -> script.googleusercontent.com; we follow by hand
// because WiFiClientSecure does not.
static bool uploadToGoogleDrive(const DriveUploadRequest &req) {
  File csv = LittleFS.open(req.source_path, "r");
  if (!csv) {
    logDrive(String("cannot open ") + req.source_path);
    return false;
  }

  size_t csvSize = csv.size();
  size_t b64Size = ((csvSize + 2) / 3) * 4;
  String folder = String(in3.serialNumber) + "/" + req.drive_folder;
  String prefix = String("{\"folder\":\"") + folder + "\",\"filename\":\"" +
                  req.drive_filename + "\",\"data\":\"";
  String suffix = "\"}";
  size_t bodyLen = prefix.length() + b64Size + suffix.length();

  int status = 0;
  String location, body;
  bool sent = streamPost(DRIVE_GAS_HOST, DRIVE_GAS_PATH, csv, prefix, suffix,
                         bodyLen, status, location, body);
  csv.close();
  if (!sent)
    return false;

  if ((status == 301 || status == 302 || status == 307) &&
      location.length() > 0) {
    String echoHost, echoPath;
    parseLocation(location, echoHost, echoPath);
    WiFiClientSecure echoClient;
    echoClient.setInsecure();
    echoClient.setTimeout(15);
    if (!echoClient.connect(echoHost.c_str(), 443))
      return false;
    echoClient.printf("GET %s HTTP/1.1\r\n", echoPath.c_str());
    echoClient.printf("Host: %s\r\n", echoHost.c_str());
    echoClient.println("Connection: close");
    echoClient.println();
    int echoStatus = 0;
    String echoBody;
    bool inBody = false;
    unsigned long t0 = millis();
    while ((echoClient.connected() || echoClient.available()) &&
           millis() - t0 < 15000) {
      if (!echoClient.available()) {
        delay(5);
        continue;
      }
      String line = echoClient.readStringUntil('\n');
      line.trim();
      if (!inBody) {
        if (line.startsWith("HTTP/"))
          echoStatus = line.substring(9, 12).toInt();
        if (line.length() == 0)
          inBody = true;
      } else {
        echoBody += line;
        if (echoBody.length() >= 256)
          break;
      }
    }
    echoClient.stop();
    return echoStatus == 200 && echoBody.indexOf("\"ok\"") >= 0;
  }
  return status == 200 && body.indexOf("\"ok\"") >= 0;
}

// configTime requires WiFi; run it once in the upload task so boot is not
// blocked if the AP is missing.
static void ensureTimeSynced() {
  if (s_time_synced)
    return;
  // Una hora puesta a mano desde /config manda: arrancar SNTP aquí la
  // desplazaría en silencio en cuanto apareciese el AP.
  if (systemClockIsManual()) {
    s_time_synced = true;
    logDrive("clock set manually, skipping NTP");
    return;
  }
  while (!WIFIIsConnected()) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = 0;
  int tries = 0;
  while (now < 1609459200UL && tries++ < 60) {
    vTaskDelay(pdMS_TO_TICKS(500));
    time(&now);
  }
  if (now >= 1609459200UL) {
    s_time_synced = true;
    logDrive("NTP time synced");
  } else {
    logDrive("NTP sync failed, will retry");
  }
}

// Minimum heap we accept after a TLS failure. Below this the mbedTLS stack
// has likely corrupted internal allocator state (observed: next LFS write
// asserts `lfs_mlist_isopen`). Restart cleanly instead of operating blind.
//
// ============ 50000 GARANTIZABA UN REINICIO POR CADA SUBIDA ============
//
// Medido en banco (2026-09-14, unidad 353): esta placa NO lleva PSRAM, asi que
// ESP.getFreeHeap() es heap interno a secas, y en regimen —WiFi asociado, MQTT
// arriba, enlace con el display— se estabiliza en unos 32 KB:
//
//     recien arrancada  ~63 KB
//     en regimen        ~32 KB   (20 peticiones seguidas cuestan 8 B: no hay fuga)
//
// O sea que heap_after < 50000 se cumplia SIEMPRE, y una guarda pensada para
// un caso excepcional reiniciaba la placa cada vez que una subida terminaba
// BIEN. Es el mismo defecto de forma que el umbral del ventilador: una
// constante elegida contra un punto de trabajo supuesto que no es el real.
//
// Baja a 20 KB: 12 por debajo del regimen medido, asi que solo salta ante un
// agotamiento de verdad y no en operacion normal.
//
// Y lo que de verdad protege no es este numero, sino heap_caps_check_integrity_all()
// unas lineas mas abajo: esa comprueba la corrupcion DIRECTAMENTE, sin umbral
// que calibrar. El umbral es solo la red por si la corrupcion no se detecta.
#define DRIVE_MIN_HEAP_AFTER_UPLOAD 20000

// DNS preflight: skip TLS entirely if the host is not resolvable quickly.
// Broken/partial handshakes are the path that corrupts heap.
static bool driveHostReachable() {
  // WIFIIsConnected() ya incluye la comprobación de localIP() != 0.0.0.0 que
  // aquí estaba suelta, más el flag de evento que sobrevive a AUTH_EXPIRE.
  if (!WIFIIsConnected())
    return false;
  IPAddress ip;
  if (!WiFi.hostByName(DRIVE_GAS_HOST, ip)) {
    logDrive("DNS failed for " DRIVE_GAS_HOST);
    return false;
  }
  return true;
}

static void driveUploadTask(void *pv) {
  for (;;) {
    DriveUploadRequest req;
    if (xQueueReceive(s_upload_queue, &req, portMAX_DELAY) != pdTRUE)
      continue;

    ensureTimeSynced();

    if (!driveHostReachable()) {
      logDrive("network not ready, dropping upload");
      if (!isCrashReport(req.source_path)) {
        removeIfExists(req.source_path);
      }
      continue;
    }

    uint32_t heap_before = ESP.getFreeHeap();
    logDrive(String("uploading ") + req.drive_folder + "/" +
             req.drive_filename + " (heap=" + heap_before + ")");
    bool ok = uploadToGoogleDrive(req);
    uint32_t heap_after = ESP.getFreeHeap();
    bool heap_corrupt = !heap_caps_check_integrity_all(false);

    // Check the heap BEFORE touching LittleFS again: if the TLS stack corrupted
    // the allocator, the next LFS open on a broken heap asserts
    // lfs_mlist_isopen.
    if (heap_after < DRIVE_MIN_HEAP_AFTER_UPLOAD || heap_corrupt) {
      // ESP_LOGE y NO logDrive: esto REINICIA una placa que gobierna el
      // calefactor, y logDrive va por LOG_DRIVE, que es false. O sea que el
      // equipo se reiniciaba solo y el motivo no salia por ningun lado.
      //
      // En banco (2026-09-14) eso costo una investigacion entera: tras una
      // tanda con la sonda puesta aparecio un `rst:0xc (RTC_SW_CPU_RST)` sin
      // panico, sin backtrace y sin una linea que lo explicara. Parecia un
      // crash y era este reinicio a proposito. Un reinicio deliberado siempre
      // tiene que dejar rastro.
      ESP_LOGE("DRIVE",
               "heap CRITICO %u B%s tras subir %s/%s; se reinicia para "
               "recuperar (umbral %u)",
               (unsigned)heap_after, heap_corrupt ? " (corrupto)" : "",
               req.drive_folder, req.drive_filename,
               (unsigned)DRIVE_MIN_HEAP_AFTER_UPLOAD);
      Serial.flush();
      vTaskDelay(pdMS_TO_TICKS(200));
      esp_restart();
    }

    // Los informes de caida solo se borran si la subida SALIO BIEN: si fallo,
    // el fichero es lo unico que queda de esa averia (ver isCrashReport).
    if (ok || !isCrashReport(req.source_path)) {
      removeIfExists(req.source_path);
    }
    logDrive(String(ok ? "upload OK" : "upload FAILED") +
             " (heap=" + heap_after +
             ", delta=" + (int32_t)(heap_after - heap_before) + ")");
  }
}

bool driveEnqueueLogUpload(const char *source_path,
                           const char *drive_filename) {
  if (s_upload_queue == nullptr)
    return false;
  DriveUploadRequest req{};
  snprintf(req.source_path, sizeof(req.source_path), "%s", source_path);
  snprintf(req.drive_folder, sizeof(req.drive_folder), "%s",
           DRIVE_SUBFOLDER_LOGS);
  snprintf(req.drive_filename, sizeof(req.drive_filename), "%s",
           drive_filename);
  return xQueueSend(s_upload_queue, &req, 0) == pdTRUE;
}

void initDriveUpload() {
  if (!LittleFS.begin(true)) {
    logDrive("LittleFS begin failed");
    return;
  }
  // Drop any /pox_*.csv left by a firmware older than 18.56.0 (the raw PPG
  // capture, see the note at the top), and cap crash_mb_/crash_hmi_ logs that
  // a crash loop left un-uploaded.
  CrashLogEntry mbKept[DRIVE_CRASH_LOG_RETENTION_CAP]  = {};
  CrashLogEntry hmiKept[DRIVE_CRASH_LOG_RETENTION_CAP] = {};
  int mbKeptCount = 0, hmiKeptCount = 0;

  File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    File f;
    while ((f = root.openNextFile())) {
      const char *n = f.name();
      char nameBuf[32] = {0};
      if (n)
        snprintf(nameBuf, sizeof(nameBuf), "%s", n);
      f.close();

      if (nameBuf[0] == '\0')
        continue;
      if (strncmp(nameBuf, "pox_", 4) == 0) {
        LittleFS.remove(String("/") + nameBuf);
      } else if (strncmp(nameBuf, "crash_mb_", 9) == 0) {
        keepNewestCrashLog(mbKept, &mbKeptCount, nameBuf,
                            strtoul(nameBuf + 9, nullptr, 10));
      } else if (strncmp(nameBuf, "crash_hmi_", 10) == 0) {
        keepNewestCrashLog(hmiKept, &hmiKeptCount, nameBuf,
                            strtoul(nameBuf + 10, nullptr, 10));
      }
    }
    root.close();
  }

  s_upload_queue = xQueueCreate(DRIVE_UPLOAD_QUEUE_LEN,
                                sizeof(DriveUploadRequest));
  if (!s_upload_queue) {
    logDrive("queue alloc failed");
    return;
  }

  xTaskCreatePinnedToCore(driveUploadTask, "DRV_UP", 8192, nullptr,
                          OTA_TASK_PRIORITY, &s_upload_task,
                          CORE_MONITOR_FREERTOS);
  logDrive("DriveUpload initialized");
}
