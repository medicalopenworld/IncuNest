/**
 * @file ArduinoHttpClient.h
 * @brief Stub mínimo para que ThingsBoard SDK compile sin la librería real.
 *        Solo se usa MQTT, no HTTP. Este header satisface el #include.
 */
#pragma once

#include <Arduino.h>
#include <Client.h>

class HttpClient : public Client {
public:
    HttpClient(Client& c, const char* h, uint16_t p) : _client(c) {}
    HttpClient(Client& c, const String& h, uint16_t p) : _client(c) {}

    // Client interface stubs
    int connect(IPAddress ip, uint16_t port) override { return 0; }
    int connect(const char *host, uint16_t port) override { return 0; }
    int connect(IPAddress ip, uint16_t port, int32_t timeout) override { return 0; }
    int connect(const char *host, uint16_t port, int32_t timeout) override { return 0; }
    size_t write(uint8_t b) override { return 0; }
    size_t write(const uint8_t *buf, size_t size) override { return 0; }
    int available() override { return 0; }
    int read() override { return -1; }
    int read(uint8_t *buf, size_t size) override { return 0; }
    int peek() override { return -1; }
    void flush() override {}
    void stop() override {}
    uint8_t connected() override { return 0; }
    operator bool() override { return false; }

    // HttpClient API stubs used by ThingsBoard
    void connectionKeepAlive() {}
    void noConnectionKeepAlive() {}
    int get(const char* path) { return 0; }
    int get(const String& path) { return 0; }
    int post(const char* path) { return 0; }
    int post(const String& path) { return 0; }
    int post(const char* path, const char* contentType, const char* body) { return 0; }
    int post(const char* path, const char* contentType, int contentLength, const byte body[]) { return 0; }
    int put(const char* path) { return 0; }
    int put(const char* path, const char* contentType, const char* body) { return 0; }
    int startRequest(const char* path, const char* method) { return 0; }
    void sendHeader(const char* name, const char* value) {}
    void sendHeader(const char* name, int value) {}
    void sendBasicAuth(const char* user, const char* pass) {}
    void beginBody() {}
    void endRequest() {}
    int responseStatusCode() { return 0; }
    String responseBody() { return String(); }
    int contentLength() { return 0; }
    void beginRequest() {}
    int skipResponseHeaders() { return 0; }
    bool headerAvailable() { return false; }
    String readHeaderName() { return String(); }
    String readHeaderValue() { return String(); }
    bool endOfHeadersReached() { return true; }
    bool endOfBodyReached() { return true; }

    static const int kNoContentLengthHeader = -1;
    static const int kHttpSuccess = 0;
    static const int HTTP_ERROR_CONNECTION_FAILED = -1;
    static const int HTTP_ERROR_API = -2;
    static const int HTTP_ERROR_TIMED_OUT = -3;
    static const int HTTP_ERROR_INVALID_RESPONSE = -4;

private:
    Client& _client;
};
