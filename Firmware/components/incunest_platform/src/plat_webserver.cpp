#include "platform/plat_webserver.h"

#include <cstring>

#include "esp_log.h"
#include "mbedtls/base64.h"

static const char *TAG = "plat_http";

// Plazos de la sincronizacion entre la tarea de httpd y handleClient(). La
// tarea de OTA llama a handleClient() cada OTA_TASK_PERIOD_MS (50 ms), asi
// que 30 s es de sobra; si vence, es que la tarea de OTA esta colgada y mas
// vale soltar el socket que dejar el navegador esperando para siempre.
static constexpr TickType_t kHandoffTimeout = pdMS_TO_TICKS(30000);

WebServer::WebServer(uint16_t port) : port_(port) {
  request_ready_ = xSemaphoreCreateBinary();
  request_done_ = xSemaphoreCreateBinary();
  event_consumed_ = xSemaphoreCreateBinary();
  events_ = xQueueCreate(1, sizeof(Event));
}

WebServer::~WebServer() {
  stop();
  vSemaphoreDelete(request_ready_);
  vSemaphoreDelete(request_done_);
  vSemaphoreDelete(event_consumed_);
  vQueueDelete(events_);
}

void WebServer::on(const char *uri, THandlerFunction handler) {
  routes_.push_back(Route{uri, HTTP_GET, std::move(handler), nullptr, true});
}

void WebServer::on(const char *uri, http_method method, THandlerFunction handler) {
  routes_.push_back(Route{uri, method, std::move(handler), nullptr, false});
}

void WebServer::on(const char *uri, http_method method, THandlerFunction handler,
                   THandlerFunction uploadHandler) {
  routes_.push_back(Route{uri, method, std::move(handler), std::move(uploadHandler), false});
}

void WebServer::begin() {
  if (server_ != nullptr) {
    return;
  }
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = port_;
  cfg.max_uri_handlers = static_cast<uint16_t>(routes_.size() + 2);
  // Un cliente a la vez, como la WebServer de Arduino: es la pagina de
  // flasheo, no un servidor de verdad. Y las conexiones se cierran al
  // responder (lru_purge) para no dejar sockets abiertos del navegador.
  cfg.max_open_sockets = 3;
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 30;
  cfg.send_wait_timeout = 30;
  // 6144 se quedaba corto: el analizador multipart de /update desbordaba la
  // pila de httpd en banco. Con el bufer de recepcion movido a miembro, 10 KB
  // dan margen de sobra para los marcos de esp_http_server mas los nuestros.
  cfg.stack_size = 10240;

  esp_err_t err = httpd_start(&server_, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "httpd_start(%u) -> %s", port_, esp_err_to_name(err));
    server_ = nullptr;
    return;
  }
  for (Route &r : routes_) {
    httpd_uri_t u = {};
    u.uri = r.uri.c_str();
    u.method = r.anyMethod ? HTTP_GET : r.method;
    u.handler = &WebServer::dispatch;
    u.user_ctx = this;
    httpd_register_uri_handler(server_, &u);
    if (r.anyMethod) {
      u.method = HTTP_POST;
      httpd_register_uri_handler(server_, &u);
    }
  }
}

void WebServer::stop() {
  if (server_ != nullptr) {
    httpd_stop(server_);
    server_ = nullptr;
  }
}

// ---- lado httpd -----------------------------------------------------------

esp_err_t WebServer::dispatch(httpd_req_t *req) {
  WebServer *self = static_cast<WebServer *>(req->user_ctx);
  for (Route &r : self->routes_) {
    if (r.uri == req->uri || (strncmp(req->uri, r.uri.c_str(), r.uri.size()) == 0 &&
                              req->uri[r.uri.size()] == '?')) {
      if (r.anyMethod || r.method == req->method) {
        return self->serve(req, r);
      }
    }
  }
  httpd_resp_send_404(req);
  return ESP_OK;
}

esp_err_t WebServer::serve(httpd_req_t *req, Route &route) {
  // Preparar el estado de la peticion para handleClient().
  req_ = req;
  route_ = &route;
  method_ = static_cast<http_method>(req->method);
  uri_ = req->uri;
  args_.clear();
  resp_headers_.clear();
  responded_ = false;

  const size_t qlen = httpd_req_get_url_query_len(req);
  if (qlen > 0) {
    std::string q(qlen + 1, '\0');
    if (httpd_req_get_url_query_str(req, q.data(), q.size()) == ESP_OK) {
      parseQuery(q.c_str());
    }
  }

  // Despertar a handleClient() y darle la peticion. A partir de aqui la
  // tarea de httpd solo alimenta trozos del cuerpo y espera.
  xQueueReset(events_);
  xSemaphoreGive(request_ready_);

  receiveBody(req, route);
  postEvent(EvType::BodyDone);

  if (xSemaphoreTake(request_done_, kHandoffTimeout) != pdTRUE) {
    ESP_LOGE(TAG, "handleClient() no respondio a %s en %lu ms", req->uri,
             (unsigned long)(kHandoffTimeout * portTICK_PERIOD_MS));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler timeout");
  }
  req_ = nullptr;
  route_ = nullptr;
  return ESP_OK;
}

void WebServer::postEvent(EvType type) {
  Event ev{type};
  if (xQueueSend(events_, &ev, kHandoffTimeout) != pdTRUE) {
    return;
  }
  // Esperar a que handleClient() lo haya consumido antes de sobrescribir el
  // bufer de subida con el siguiente trozo.
  xSemaphoreTake(event_consumed_, kHandoffTimeout);
}

void WebServer::receiveBody(httpd_req_t *req, Route &route) {
  if (req->content_len == 0) {
    return;
  }
  char ctype[96] = {};
  httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype));

  if (route.uploadHandler && strncmp(ctype, "multipart/form-data", 19) == 0) {
    receiveMultipart(req);
    return;
  }

  // Cuerpo pequeño (formulario urlencoded o JSON): se lee entero y, si es
  // urlencoded, se vuelca en arg() como hacia Arduino.
  std::string body;
  body.resize(req->content_len);
  size_t got = 0;
  while (got < req->content_len) {
    const int n = httpd_req_recv(req, body.data() + got, req->content_len - got);
    if (n <= 0) {
      break;
    }
    got += static_cast<size_t>(n);
  }
  body.resize(got);
  if (strncmp(ctype, "application/x-www-form-urlencoded", 33) == 0) {
    parseUrlEncodedBody(body);
  } else {
    args_.emplace_back("plain", body);
  }
}

// Analizador multipart/form-data minimo: un unico campo de fichero, que es lo
// que envia la pagina de /update. Va llamando al manejador de subida con
// START, WRITE (trozo a trozo) y END, igual que la WebServer de Arduino.
void WebServer::receiveMultipart(httpd_req_t *req) {
  char ctype[160] = {};
  httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype));
  const char *b = strstr(ctype, "boundary=");
  if (b == nullptr) {
    return;
  }
  std::string boundary = "--";
  boundary += (b + 9);
  if (!boundary.empty() && boundary.back() == '"') {
    boundary.pop_back();
  }
  if (boundary.size() > 2 && boundary[2] == '"') {
    boundary.erase(2, 1);
  }

  // OJO: NO hacer `upload_ = HTTPUpload{}`. HTTPUpload lleva un bufer de
  // HTTP_UPLOAD_BUFLEN (4 KB), asi que esa asignacion construye un temporal de
  // 4 KB EN LA PILA y luego lo copia. Eso desbordaba la pila de la tarea de
  // httpd en banco (2026-09-11, "STACK: OVERFLOW in task 'httpd'") y tumbaba
  // la subida de /update. Se reinician los campos uno a uno; el bufer no hace
  // falta limpiarlo, cada trozo lo sobrescribe y currentSize dice cuanto vale.
  upload_.status = UPLOAD_FILE_START;
  upload_.filename = String();
  upload_.name = String();
  upload_.type = String();
  upload_.totalSize = 0;
  upload_.currentSize = 0;
  std::string window; // bytes recibidos aun no entregados
  size_t remaining = req->content_len;
  bool in_part_headers = false;
  bool in_data = false;
  bool started = false;
  char *chunk = rx_chunk_;              // miembro, no pila: ver plat_webserver.h
  const size_t chunk_size = sizeof(rx_chunk_);

  auto deliver = [&](const char *data, size_t len) {
    // Entregar en trozos de HTTP_UPLOAD_BUFLEN como maximo.
    while (len > 0) {
      const size_t n = len < HTTP_UPLOAD_BUFLEN ? len : HTTP_UPLOAD_BUFLEN;
      memcpy(upload_.buf, data, n);
      upload_.currentSize = n;
      upload_.totalSize += n;
      upload_.status = UPLOAD_FILE_WRITE;
      postEvent(EvType::UploadWrite);
      data += n;
      len -= n;
    }
  };

  while (remaining > 0 || !window.empty()) {
    if (remaining > 0) {
      const size_t want = remaining < chunk_size ? remaining : chunk_size;
      const int n = httpd_req_recv(req, chunk, want);
      if (n <= 0) {
        if (started) {
          upload_.status = UPLOAD_FILE_ABORTED;
          postEvent(EvType::UploadAborted);
        }
        return;
      }
      remaining -= static_cast<size_t>(n);
      window.append(chunk, static_cast<size_t>(n));
    }

    if (!in_part_headers && !in_data) {
      // Buscar el primer boundary.
      const size_t p = window.find(boundary);
      if (p == std::string::npos) {
        if (remaining == 0) break;
        continue;
      }
      window.erase(0, p + boundary.size());
      in_part_headers = true;
    }

    if (in_part_headers) {
      const size_t hend = window.find("\r\n\r\n");
      if (hend == std::string::npos) {
        if (remaining == 0) break;
        continue;
      }
      const std::string headers = window.substr(0, hend);
      window.erase(0, hend + 4);
      const size_t fn = headers.find("filename=\"");
      if (fn != std::string::npos) {
        const size_t fe = headers.find('"', fn + 10);
        upload_.filename = String(headers.substr(fn + 10, fe - fn - 10));
      }
      const size_t nm = headers.find("name=\"");
      if (nm != std::string::npos) {
        const size_t ne = headers.find('"', nm + 6);
        upload_.name = String(headers.substr(nm + 6, ne - nm - 6));
      }
      upload_.status = UPLOAD_FILE_START;
      upload_.totalSize = 0;
      upload_.currentSize = 0;
      started = true;
      postEvent(EvType::UploadStart);
      in_part_headers = false;
      in_data = true;
    }

    if (in_data) {
      // Los datos terminan en "\r\n" + boundary. Se entrega todo lo que con
      // seguridad no forma parte de ese terminador.
      const std::string term = "\r\n" + boundary;
      const size_t p = window.find(term);
      if (p != std::string::npos) {
        if (p > 0) deliver(window.data(), p);
        window.erase(0, p + term.size());
        upload_.status = UPLOAD_FILE_END;
        upload_.currentSize = 0;
        postEvent(EvType::UploadEnd);
        in_data = false;
        // Lo que quede (--\r\n o mas partes) no se procesa: un solo fichero.
        window.clear();
        // Drenar el resto del cuerpo para no dejar la conexion a medias.
        while (remaining > 0) {
          const size_t want = remaining < chunk_size ? remaining : chunk_size;
          const int n = httpd_req_recv(req, chunk, want);
          if (n <= 0) break;
          remaining -= static_cast<size_t>(n);
        }
        return;
      }
      if (window.size() > term.size()) {
        const size_t safe = window.size() - term.size();
        deliver(window.data(), safe);
        window.erase(0, safe);
      }
      if (remaining == 0) {
        // Cuerpo agotado sin boundary final: entregar lo que quede y cerrar.
        if (!window.empty()) deliver(window.data(), window.size());
        window.clear();
        upload_.status = UPLOAD_FILE_END;
        upload_.currentSize = 0;
        postEvent(EvType::UploadEnd);
        in_data = false;
      }
    }
  }
}

// ---- lado handleClient() (tarea de OTA) ----------------------------------

void WebServer::handleClient() {
  if (server_ == nullptr) {
    return;
  }
  if (xSemaphoreTake(request_ready_, 0) != pdTRUE) {
    return; // nada pendiente
  }
  Route *route = route_;
  if (route == nullptr) {
    xSemaphoreGive(request_done_);
    return;
  }

  // Consumir los eventos del cuerpo (trozos de subida) hasta que llegue el
  // fin, invocando al manejador de subida en ESTA tarea.
  for (;;) {
    Event ev;
    if (xQueueReceive(events_, &ev, kHandoffTimeout) != pdTRUE) {
      break;
    }
    if (ev.type == EvType::BodyDone) {
      xSemaphoreGive(event_consumed_);
      break;
    }
    if (route->uploadHandler) {
      route->uploadHandler();
    }
    xSemaphoreGive(event_consumed_);
  }

  if (route->handler) {
    route->handler();
  }
  if (!responded_ && req_ != nullptr) {
    // Arduino respondia 200 vacio si el manejador no decia nada.
    send(200, "text/plain", "");
  }
  xSemaphoreGive(request_done_);
}

void WebServer::send(int code, const char *content_type, const String &content) {
  send(code, content_type, content.c_str());
}

void WebServer::send(int code, const char *content_type, const char *content) {
  if (req_ == nullptr || responded_) {
    return;
  }
  char status[32];
  const char *phrase = "OK";
  switch (code) {
  case 200: phrase = "OK"; break;
  case 400: phrase = "Bad Request"; break;
  case 401: phrase = "Unauthorized"; break;
  case 403: phrase = "Forbidden"; break;
  case 404: phrase = "Not Found"; break;
  case 500: phrase = "Internal Server Error"; break;
  default: phrase = ""; break;
  }
  snprintf(status, sizeof(status), "%d %s", code, phrase);
  httpd_resp_set_status(req_, status);
  httpd_resp_set_type(req_, content_type ? content_type : "text/plain");
  for (auto &h : resp_headers_) {
    httpd_resp_set_hdr(req_, h.first.c_str(), h.second.c_str());
  }
  httpd_resp_send(req_, content ? content : "", content ? strlen(content) : 0);
  responded_ = true;
}

void WebServer::sendHeader(const String &name, const String &value, bool first) {
  if (first) {
    resp_headers_.insert(resp_headers_.begin(), {name.c_str(), value.c_str()});
  } else {
    resp_headers_.emplace_back(name.c_str(), value.c_str());
  }
}

static std::string url_decode(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') {
      out.push_back(' ');
    } else if (s[i] == '%' && i + 2 < s.size()) {
      const char hex[3] = {s[i + 1], s[i + 2], '\0'};
      out.push_back(static_cast<char>(strtol(hex, nullptr, 16)));
      i += 2;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

void WebServer::parseQuery(const char *query) {
  std::string q(query ? query : "");
  size_t start = 0;
  while (start < q.size()) {
    size_t end = q.find('&', start);
    if (end == std::string::npos) end = q.size();
    const std::string kv = q.substr(start, end - start);
    const size_t eq = kv.find('=');
    if (eq == std::string::npos) {
      args_.emplace_back(url_decode(kv), "");
    } else {
      args_.emplace_back(url_decode(kv.substr(0, eq)), url_decode(kv.substr(eq + 1)));
    }
    start = end + 1;
  }
}

void WebServer::parseUrlEncodedBody(const std::string &body) { parseQuery(body.c_str()); }

String WebServer::arg(const char *name) {
  for (auto &a : args_) {
    if (a.first == name) {
      return String(a.second);
    }
  }
  return String();
}

bool WebServer::hasArg(const char *name) {
  for (auto &a : args_) {
    if (a.first == name) {
      return true;
    }
  }
  return false;
}

String WebServer::header(const char *name) {
  if (req_ == nullptr) {
    return String();
  }
  const size_t len = httpd_req_get_hdr_value_len(req_, name);
  if (len == 0) {
    return String();
  }
  std::string v(len + 1, '\0');
  if (httpd_req_get_hdr_value_str(req_, name, v.data(), v.size()) != ESP_OK) {
    return String();
  }
  v.resize(len);
  return String(v);
}

void WebServer::collectHeaders(const char *names[], size_t count) {
  // Arduino necesitaba saber de antemano que cabeceras guardar; esp_http_server
  // permite leer cualquiera durante la peticion, asi que no hay nada que hacer.
  (void)names;
  (void)count;
}

bool WebServer::authenticate(const char *username, const char *password) {
  const String auth = header("Authorization");
  if (auth.length() < 7 || !auth.startsWith("Basic ")) {
    return false;
  }
  const std::string b64 = auth.str().substr(6);
  unsigned char decoded[128] = {};
  size_t olen = 0;
  if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &olen,
                            reinterpret_cast<const unsigned char *>(b64.c_str()),
                            b64.size()) != 0) {
    return false;
  }
  std::string expected = std::string(username ? username : "") + ":" + (password ? password : "");
  return expected == std::string(reinterpret_cast<char *>(decoded), olen);
}

void WebServer::requestAuthentication() {
  sendHeader("WWW-Authenticate", "Basic realm=\"Login Required\"");
  send(401, "text/html", "");
}
