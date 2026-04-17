#include "DriveUpload.h"
#include "main.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

extern in3ator_parameters in3;

static QueueHandle_t s_sample_queue = nullptr;
static QueueHandle_t s_upload_queue = nullptr;

static TaskHandle_t s_write_task  = nullptr;
static TaskHandle_t s_upload_task = nullptr;

static volatile bool s_upload_slot_busy = false;
static bool s_time_synced = false;

struct DriveUploadRequest {
  char filename[96];
};

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
    logSPO2("[DRV ] HTTPS connect failed");
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
static bool uploadToGoogleDrive(const String &filename) {
  File csv = LittleFS.open(DRIVE_CSV_UPLOAD_PATH, "r");
  if (!csv) {
    logSPO2("[DRV ] cannot open upload file");
    return false;
  }

  size_t csvSize = csv.size();
  size_t b64Size = ((csvSize + 2) / 3) * 4;
  String folder = String(in3.serialNumber) + "/" + DRIVE_SUBFOLDER_PULSIOX;
  String prefix = String("{\"folder\":\"") + folder + "\",\"filename\":\"" +
                  filename + "\",\"data\":\"";
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
    logSPO2("[DRV ] NTP time synced");
  } else {
    logSPO2("[DRV ] NTP sync failed, will retry");
  }
}

// ─── Writer: queue -> active CSV, rotate every DRIVE_UPLOAD_WINDOW_MS ────────
static void driveWriteTask(void *pv) {
  File csv;
  bool open = false;
  uint32_t window_start_ms = 0;
  bool hb_detected = false;

  auto rotate = [&]() {
    if (!open)
      return;
    csv.close();
    open = false;

    if (s_upload_slot_busy) {
      LittleFS.remove(DRIVE_CSV_ACTIVE_PATH);
      logSPO2("[DRV ] upload still busy, dropped window");
      return;
    }

    LittleFS.remove(DRIVE_CSV_UPLOAD_PATH);
    if (!LittleFS.rename(DRIVE_CSV_ACTIVE_PATH, DRIVE_CSV_UPLOAD_PATH)) {
      logSPO2("[DRV ] rename active->upload failed");
      LittleFS.remove(DRIVE_CSV_ACTIVE_PATH);
      return;
    }

    DriveUploadRequest req{};
    time_t now;
    time(&now);
    struct tm t;
    gmtime_r(&now, &t);
    char tsbuf[32];
    strftime(tsbuf, sizeof(tsbuf), "%Y_%m_%d_%H_%M_%S", &t);
    snprintf(req.filename, sizeof(req.filename),
             "%s%s_%d_PulseOximeter_data.csv", hb_detected ? "DH_" : "", tsbuf,
             (int)in3.serialNumber);

    s_upload_slot_busy = true;
    if (xQueueSend(s_upload_queue, &req, 0) != pdTRUE) {
      // Should not happen (slot_busy guards the queue) — recover.
      s_upload_slot_busy = false;
      LittleFS.remove(DRIVE_CSV_UPLOAD_PATH);
      logSPO2("[DRV ] upload queue full");
    }
  };

  for (;;) {
    DrivePpgSample s;
    bool got = xQueueReceive(s_sample_queue, &s, pdMS_TO_TICKS(200)) == pdTRUE;

    // Rotate on wall-clock boundary even if samples stop arriving.
    if (open && (millis() - window_start_ms) >= DRIVE_UPLOAD_WINDOW_MS) {
      rotate();
    }
    if (!got)
      continue;

    do {
      if (!open) {
        LittleFS.remove(DRIVE_CSV_ACTIVE_PATH);
        csv = LittleFS.open(DRIVE_CSV_ACTIVE_PATH, "w", true);
        if (!csv) {
          logSPO2("[DRV ] cannot open active CSV");
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
        csv.println("t_ms,led1_aled1,led2_aled2,ppg");
        window_start_ms = s.t_ms;
        hb_detected = false;
        open = true;
      }

      uint32_t rel_ms = s.t_ms - window_start_ms;
      csv.printf("%u,%d,%d,%d\n", (unsigned)rel_ms, (int)s.led1_aled1,
                 (int)s.led2_aled2, (int)s.ppg);

      if (s.hr2_sqi > DRIVE_HR_SQI_THRESHOLD &&
          s.hr3_sqi > DRIVE_HR_SQI_THRESHOLD) {
        hb_detected = true;
      }

      if (rel_ms >= DRIVE_UPLOAD_WINDOW_MS) {
        rotate();
      }
    } while (xQueueReceive(s_sample_queue, &s, 0) == pdTRUE);
  }
}

static void driveUploadTask(void *pv) {
  for (;;) {
    DriveUploadRequest req;
    if (xQueueReceive(s_upload_queue, &req, portMAX_DELAY) != pdTRUE)
      continue;

    ensureTimeSynced();

    if (WiFi.status() != WL_CONNECTED) {
      logSPO2("[DRV ] no WiFi, skipping upload");
      LittleFS.remove(DRIVE_CSV_UPLOAD_PATH);
      s_upload_slot_busy = false;
      continue;
    }

    logSPO2(String("[DRV ] uploading ") + req.filename);
    bool ok = uploadToGoogleDrive(String(req.filename));
    LittleFS.remove(DRIVE_CSV_UPLOAD_PATH);
    s_upload_slot_busy = false;
    logSPO2(ok ? "[DRV ] upload OK" : "[DRV ] upload FAILED");
  }
}

void drivePushSample(const AFE4490Data &data) {
  if (s_sample_queue == nullptr)
    return;
  DrivePpgSample s;
  s.t_ms       = millis();
  s.led1_aled1 = data.led1_aled1;
  s.led2_aled2 = data.led2_aled2;
  s.ppg        = data.ppg;
  s.hr2_sqi    = data.hr2_sqi;
  s.hr3_sqi    = data.hr3_sqi;
  xQueueSend(s_sample_queue, &s, 0);
}

void initDriveUpload() {
  if (!LittleFS.begin(true)) {
    logSPO2("[DRV ] LittleFS begin failed");
    return;
  }
  LittleFS.remove(DRIVE_CSV_ACTIVE_PATH);
  LittleFS.remove(DRIVE_CSV_UPLOAD_PATH);

  s_sample_queue = xQueueCreate(DRIVE_SAMPLE_QUEUE_LEN, sizeof(DrivePpgSample));
  s_upload_queue = xQueueCreate(1, sizeof(DriveUploadRequest));
  if (!s_sample_queue || !s_upload_queue) {
    logSPO2("[DRV ] queue alloc failed");
    return;
  }

  xTaskCreatePinnedToCore(driveWriteTask, "DRV_WR", 6144, nullptr,
                          SPO2_TASK_PRIORITY - 1, &s_write_task,
                          CORE_MONITOR_FREERTOS);
  xTaskCreatePinnedToCore(driveUploadTask, "DRV_UP", 8192, nullptr,
                          OTA_TASK_PRIORITY, &s_upload_task,
                          CORE_MONITOR_FREERTOS);
  logSPO2("[DRV ] DriveUpload initialized");
}
