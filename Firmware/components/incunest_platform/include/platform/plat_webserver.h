#pragma once

// Servidor HTTP con la forma de la WebServer de Arduino, sobre
// esp_http_server (ESP-IDF puro). Sirve la pagina de flasheo por WiFi
// (/update) y sus endpoints auxiliares en las dos placas.
//
// LA DECISION QUE IMPORTA: EN QUE TAREA CORREN LOS MANEJADORES.
// En Arduino, handleClient() atendia la peticion completa DENTRO de la tarea
// que lo llamaba —la tarea de OTA— y no volvia hasta haber respondido. El
// manejador de /update escribe la flash y comparte estado (Update, la guarda
// de placa, OTA_inprogress) con el resto de esa tarea. esp_http_server, en
// cambio, ejecuta los manejadores en SU propia tarea.
//
// Mover esos manejadores a otra tarea seria meter una carrera entre la OTA
// por web y la OTA por ThingsBoard, las dos escribiendo flash. Asi que aqui se
// conserva el modelo de Arduino: la tarea de httpd solo recibe la peticion y
// se la pasa a handleClient(), que la ejecuta entera en la tarea de OTA
// (manejador de subida trozo a trozo, y luego el manejador principal) antes
// de devolver el control. Cuesta un par de semaforos y una cola de un
// elemento; a cambio la concurrencia es EXACTAMENTE la de antes.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "platform/plat_print.h"
#include "platform/plat_string.h"

// Estados de la subida multipart, como en Arduino.
typedef enum {
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED,
} HTTPUploadStatus;

#define HTTP_UPLOAD_BUFLEN 4096

struct HTTPUpload {
  HTTPUploadStatus status = UPLOAD_FILE_START;
  String filename;
  String name;
  String type;
  size_t totalSize = 0;   // bytes recibidos hasta ahora
  size_t currentSize = 0; // bytes validos en buf en este trozo
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
};

class WebServer {
public:
  using THandlerFunction = std::function<void(void)>;

  explicit WebServer(uint16_t port = 80);
  ~WebServer();

  void begin();
  void stop();
  // Atiende, en la tarea que lo llama, una peticion pendiente si la hay.
  void handleClient();

  void on(const char *uri, THandlerFunction handler);
  void on(const char *uri, http_method method, THandlerFunction handler);
  void on(const char *uri, http_method method, THandlerFunction handler,
          THandlerFunction uploadHandler);

  // --- dentro de un manejador ---
  void send(int code, const char *content_type = nullptr, const String &content = String());
  void send(int code, const char *content_type, const char *content);
  void sendHeader(const String &name, const String &value, bool first = false);
  String arg(const char *name);
  String arg(const String &name) { return arg(name.c_str()); }
  bool hasArg(const char *name);
  bool hasArg(const String &name) { return hasArg(name.c_str()); }
  String header(const char *name);
  String header(const String &name) { return header(name.c_str()); }
  void collectHeaders(const char *names[], size_t count);
  bool authenticate(const char *username, const char *password);
  void requestAuthentication();
  HTTPUpload &upload() { return upload_; }
  http_method method() const { return method_; }
  String uri() const { return String(uri_.c_str()); }

private:
  struct Route {
    std::string uri;
    http_method method;
    THandlerFunction handler;
    THandlerFunction uploadHandler;
    bool anyMethod;
  };

  // Mensajes de la tarea de httpd hacia handleClient().
  enum class EvType { UploadStart, UploadWrite, UploadEnd, UploadAborted, BodyDone };
  struct Event {
    EvType type;
  };

  static esp_err_t dispatch(httpd_req_t *req);
  esp_err_t serve(httpd_req_t *req, Route &route);
  void receiveBody(httpd_req_t *req, Route &route);
  void receiveMultipart(httpd_req_t *req);
  void postEvent(EvType type);
  void parseQuery(const char *query);
  void parseUrlEncodedBody(const std::string &body);

  uint16_t port_;
  httpd_handle_t server_ = nullptr;
  std::vector<Route> routes_;

  // Estado de la peticion en curso (la atiende una sola tarea a la vez).
  httpd_req_t *req_ = nullptr;
  Route *route_ = nullptr;
  http_method method_ = HTTP_GET;
  std::string uri_;
  std::vector<std::pair<std::string, std::string>> args_;
  std::vector<std::pair<std::string, std::string>> resp_headers_;
  bool responded_ = false;
  HTTPUpload upload_;
  // Bufer de recepcion del analizador multipart. Va como MIEMBRO, no en la
  // pila de receiveMultipart(): la tarea de httpd desbordo su pila con el
  // subida de /update en banco (2026-09-11, "STACK: OVERFLOW in task 'httpd'").
  // 1 KB en .bss cuesta menos que 1 KB de pila en una tarea que ya arrastra
  // los marcos de esp_http_server.
  char rx_chunk_[1024];

  // Sincronizacion httpd <-> handleClient().
  SemaphoreHandle_t request_ready_ = nullptr;  // hay peticion para handleClient()
  SemaphoreHandle_t request_done_ = nullptr;   // handleClient() ya respondio
  QueueHandle_t events_ = nullptr;             // trozos de subida / fin de cuerpo
  SemaphoreHandle_t event_consumed_ = nullptr; // handleClient() proceso el trozo
};
