#include "DriveUpload.h"
#include "main.h"

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

extern IncuNest_parameters in3;

static QueueHandle_t s_sample_queue = nullptr;
static QueueHandle_t s_upload_queue = nullptr;

static TaskHandle_t s_write_task  = nullptr;
static TaskHandle_t s_upload_task = nullptr;

static volatile bool s_upload_slot_busy = false;
static bool s_time_synced = false;

// One request = one upload. `source_path` holds the CSV/log on LittleFS;
// `clear_pulsiox_slot` tells the uploader to release the single pulsiox slot
// (pox_upload.csv) when done, so the writer task can rotate again.
struct DriveUploadRequest {
  char source_path[32];
  char drive_folder[32];
  char drive_filename[96];
  bool clear_pulsiox_slot;
};

#define DRIVE_UPLOAD_QUEUE_LEN 4

// Silent remove: LittleFS.exists() + LittleFS.remove() both log [E] at the
// framework level when the file is missing. We clean pre-emptively so probe
// via POSIX stat() on the mounted path, which does not log.
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
  while (WiFi.status() != WL_CONNECTED) {
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

// ─── Writer: queue -> unique CSV per window, rotate every WINDOW_MS ──────────
// Each window writes to /pox_<ms>.csv with a fresh inode — we never reuse a
// path or rename an existing file, which avoided an esp_littlefs fd-table
// corruption that asserted `lfs_mlist_isopen` on the next open after rotation.
static void driveWriteTask(void *pv) {
  int      csv_fd          = -1;
  char     csv_rel[24]     = {0};  // "/pox_<ms>.csv" (LittleFS-relative)
  uint32_t window_start_ms = 0;
  bool     hb_detected     = false;

  auto rotate = [&]() {
    if (csv_fd < 0)
      return;
    ::close(csv_fd);
    csv_fd = -1;

#if DRIVE_DISABLE_UPLOAD
    removeIfExists(csv_rel);
    logDrive("upload disabled, dropped window");
    return;
#endif

    if (s_upload_slot_busy) {
      removeIfExists(csv_rel);
      logDrive("upload still busy, dropped window");
      return;
    }

    DriveUploadRequest req{};
    time_t now;
    time(&now);
    struct tm t;
    gmtime_r(&now, &t);
    char tsbuf[32];
    strftime(tsbuf, sizeof(tsbuf), "%Y_%m_%d_%H_%M_%S", &t);
    snprintf(req.source_path, sizeof(req.source_path), "%s", csv_rel);
    snprintf(req.drive_folder, sizeof(req.drive_folder), "%s",
             DRIVE_SUBFOLDER_PULSIOX);
    snprintf(req.drive_filename, sizeof(req.drive_filename),
             "%s%s_%d_PulseOximeter_data.csv", hb_detected ? "DH_" : "", tsbuf,
             (int)in3.serialNumber);
    req.clear_pulsiox_slot = true;

    s_upload_slot_busy = true;
    if (xQueueSend(s_upload_queue, &req, 0) != pdTRUE) {
      // Should not happen (slot_busy guards the queue) — recover.
      s_upload_slot_busy = false;
      removeIfExists(csv_rel);
      logDrive("upload queue full");
    }
  };

  for (;;) {
    DrivePpgSample s;
    bool got = xQueueReceive(s_sample_queue, &s, pdMS_TO_TICKS(200)) == pdTRUE;

    // Rotate on wall-clock boundary even if samples stop arriving.
    if (csv_fd >= 0 && (millis() - window_start_ms) >= DRIVE_UPLOAD_WINDOW_MS) {
      rotate();
    }
    if (!got)
      continue;

    do {
      if (csv_fd < 0) {
        // Serialize with the upload task: esp_littlefs in Arduino 2.x corrupts
        // the open-files mlist if two fds from different tasks are alive at
        // once. Skip opening a new window while an upload is in flight —
        // samples are dropped (~upload duration) but system stays alive.
        if (s_upload_slot_busy) {
          break;
        }
        snprintf(csv_rel, sizeof(csv_rel),
                 DRIVE_CSV_PATH_PREFIX "%lu" DRIVE_CSV_PATH_SUFFIX,
                 (unsigned long)s.t_ms);
        char full[40];
        snprintf(full, sizeof(full), LFS_MOUNT "%s", csv_rel);
        csv_fd = ::open(full, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (csv_fd < 0) {
          logDrive(String("cannot open ") + csv_rel);
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
        static const char header[] = "t_ms,led1_sub,led2_sub,ppg_disp\n";
        ::write(csv_fd, header, sizeof(header) - 1);
        window_start_ms = s.t_ms;
        hb_detected = false;
      }

      uint32_t rel_ms = s.t_ms - window_start_ms;
      char line[48];
      int  n = snprintf(line, sizeof(line), "%u,%d,%d,%d\n", (unsigned)rel_ms,
                        (int)s.led1_sub, (int)s.led2_sub, (int)s.ppg_disp);
      if (n > 0)
        ::write(csv_fd, line, n);

      if (fmaxf(s.hr2_sqi, s.hr3_sqi) >= DRIVE_HR_SQI_MAX_THRESHOLD &&
          fminf(s.hr2_sqi, s.hr3_sqi) >= DRIVE_HR_SQI_MIN_THRESHOLD) {
        hb_detected = true;
      }

      if (rel_ms >= DRIVE_UPLOAD_WINDOW_MS) {
        rotate();
      }
    } while (xQueueReceive(s_sample_queue, &s, 0) == pdTRUE);
  }
}

// Minimum heap we accept after a TLS failure. Below this the mbedTLS stack
// has likely corrupted internal allocator state (observed: next LFS write
// asserts `lfs_mlist_isopen`). Restart cleanly instead of operating blind.
#define DRIVE_MIN_HEAP_AFTER_UPLOAD 50000

// DNS preflight: skip TLS entirely if the host is not resolvable quickly.
// Broken/partial handshakes are the path that corrupts heap.
static bool driveHostReachable() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  if (WiFi.localIP() == IPAddress(0, 0, 0, 0))
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
      removeIfExists(req.source_path);
      if (req.clear_pulsiox_slot)
        s_upload_slot_busy = false;
      continue;
    }

    uint32_t heap_before = ESP.getFreeHeap();
    logDrive(String("uploading ") + req.drive_folder + "/" +
             req.drive_filename + " (heap=" + heap_before + ")");
    bool ok = uploadToGoogleDrive(req);
    uint32_t heap_after = ESP.getFreeHeap();
    bool heap_corrupt = !heap_caps_check_integrity_all(false);

    // Check heap BEFORE releasing the slot: if the TLS stack corrupted the
    // allocator, releasing s_upload_slot_busy would let the write task open a
    // new LFS file on a broken heap, which asserts lfs_mlist_isopen.
    if (heap_after < DRIVE_MIN_HEAP_AFTER_UPLOAD || heap_corrupt) {
      logDrive(String("CRITICAL heap ") + heap_after +
               (heap_corrupt ? " (corrupt)" : "") +
               " after upload, restarting to recover");
      Serial.flush();
      vTaskDelay(pdMS_TO_TICKS(200));
      esp_restart();
    }

    removeIfExists(req.source_path);
    if (req.clear_pulsiox_slot)
      s_upload_slot_busy = false;
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
  req.clear_pulsiox_slot = false;
  return xQueueSend(s_upload_queue, &req, 0) == pdTRUE;
}

void drivePushSample(const AFE4490Data &data) {
  if (s_sample_queue == nullptr)
    return;
  DrivePpgSample s;
  s.t_ms       = millis();
  s.led1_sub  = data.led1_sub;
  s.led2_sub  = data.led2_sub;
  s.ppg_disp  = data.ppg_disp;
  s.hr2_sqi    = data.hr2_sqi;
  s.hr3_sqi    = data.hr3_sqi;
  xQueueSend(s_sample_queue, &s, 0);
}

void initDriveUpload() {
  if (!LittleFS.begin(true)) {
    logDrive("LittleFS begin failed");
    return;
  }
  // Drop leftover per-window CSVs from a previous (possibly crashed) boot,
  // and cap crash_mb_/crash_hmi_ logs that a crash loop left un-uploaded.
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

  s_sample_queue = xQueueCreate(DRIVE_SAMPLE_QUEUE_LEN, sizeof(DrivePpgSample));
  s_upload_queue = xQueueCreate(DRIVE_UPLOAD_QUEUE_LEN,
                                sizeof(DriveUploadRequest));
  if (!s_sample_queue || !s_upload_queue) {
    logDrive("queue alloc failed");
    return;
  }

  xTaskCreatePinnedToCore(driveWriteTask, "DRV_WR", 6144, nullptr,
                          SPO2_TASK_PRIORITY - 1, &s_write_task,
                          CORE_MONITOR_FREERTOS);
#if DRIVE_DISABLE_UPLOAD
  logDrive("DriveUpload initialized (UPLOAD DISABLED — diagnostic)");
#else
  xTaskCreatePinnedToCore(driveUploadTask, "DRV_UP", 8192, nullptr,
                          OTA_TASK_PRIORITY, &s_upload_task,
                          CORE_MONITOR_FREERTOS);
  logDrive("DriveUpload initialized");
#endif
}
