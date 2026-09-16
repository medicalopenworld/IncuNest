#pragma once

// Sistema de ficheros LittleFS con la forma de la API FS de Arduino, montado
// sobre el componente joltwallet/littlefs y el VFS de ESP-IDF.
//
// POR QUE SE CONSERVA LA FORMA (LittleFS.open(), File, print/printf/seek...):
// los consumidores son el almacen de perfiles de bebe y el archivo de pesos
// (baby_profile_store.cpp), el registro de fallos (CrashReporter.cpp) y el
// test de fabrica. Son ~150 puntos de llamada sobre DATOS DE PACIENTE que ya
// estan escritos en las unidades desplegadas. Reescribirlos a stdio uno a uno
// seria la edicion con mas superficie de error de todo el porte, sin ninguna
// ganancia: por debajo esto ya es POSIX sobre el VFS de IDF.
//
// COMPATIBILIDAD DE LAS RUTAS — LO IMPORTANTE:
// Arduino montaba LittleFS en la raiz, asi que el firmware abre
// "/baby_history.log" o "/weight_archive/12.bin". ESP-IDF monta el sistema de
// ficheros bajo un prefijo ("/littlefs"). Esta clase ANTEPONE ese prefijo por
// dentro, de forma que:
//   - los puntos de llamada no cambian, y
//   - la ruta que se guarda DENTRO del LittleFS es la misma de siempre.
// Es lo que permite que una unidad ya desplegada siga encontrando sus
// ficheros despues del OTA. La particion tambien es la misma: la etiquetada
// "spiffs" (que monta LittleFS pese al nombre, ver partitions/ESP32S3_8MB.csv).

#include <cstdarg>
#include <string.h>  // strrchr(), para name()
#include <cstdint>
#include <cstdio>
#include <dirent.h>

#include "platform/plat_string.h"

class FsFile {
public:
  FsFile() = default;
  explicit FsFile(FILE *f, const String &path) : f_(f), path_(path) {}
  explicit FsFile(DIR *d, const String &path) : d_(d), path_(path) {}
  ~FsFile();

  FsFile(const FsFile &) = delete;
  FsFile &operator=(const FsFile &) = delete;
  FsFile(FsFile &&o) noexcept;
  FsFile &operator=(FsFile &&o) noexcept;

  explicit operator bool() const { return f_ != nullptr || d_ != nullptr; }

  size_t write(uint8_t b);
  size_t write(const uint8_t *buf, size_t len);
  int read();
  size_t read(uint8_t *buf, size_t len);
  size_t readBytes(char *buf, size_t len);
  String readStringUntil(char terminator);

  size_t print(const String &s);
  size_t print(const char *s);
  size_t print(int v);
  size_t print(unsigned int v);
  size_t print(long v);
  size_t print(unsigned long v);
  size_t print(double v, int decimals = 2);
  size_t println(const String &s);
  size_t println(const char *s);
  size_t println();
  int printf(const char *fmt, ...);

  bool seek(uint32_t pos);
  uint32_t position();
  uint32_t size();
  int available();
  void flush();
  void close();

  // name() DEVUELVE EL NOMBRE PELADO, no la ruta. Es lo que espera todo el
  // firmware que venia de Arduino, y devolver la ruta completa rompio TRES
  // subsistemas en silencio (banco 2026-09-14):
  //
  //   - DriveUpload.cpp: la limpieza de arranque hace
  //     `strncmp(nameBuf, "pox_", 4)`. Con "/littlefs/pox_123.csv" no casa, asi
  //     que las ventanas de PPG de arranques anteriores NO SE BORRABAN NUNCA.
  //     Se encontraron 1,76 MB en 4 ficheros sobre una particion de 2 MB.
  //   - El mismo bucle aplica el tope de 5 ficheros a los crash_mb_/crash_hmi_.
  //     Tampoco casaba: habia 11 crash_mb_ con el tope en 5.
  //   - baby_profile_store.cpp: `strtoul(f.name())` sobre "<seq>.bin" para
  //     recuperar el numero de secuencia del historico de pesos. Con la ruta
  //     delante devuelve 0.
  //
  // Ninguno daba error: simplemente no hacian nada. Por eso la particion se
  // llenaba hasta que la placa abortaba.
  //
  // Quien necesite la ruta completa tiene path().
  const char *name() const {
    const char *slash = strrchr(path_.c_str(), '/');
    return slash ? slash + 1 : path_.c_str();
  }
  const char *path() const { return path_.c_str(); }
  bool isDirectory() const { return d_ != nullptr; }
  FsFile openNextFile();

private:
  FILE *f_ = nullptr;
  DIR *d_ = nullptr;
  String path_;
};

class LittleFsWrapper {
public:
  // Misma firma que la de Arduino, para no tocar las llamadas. El
  // partitionLabel por defecto es "spiffs" porque asi se llama la particion
  // en la tabla, aunque lo que monte sea LittleFS.
  bool begin(bool formatOnFail = false, const char *basePath = "/littlefs",
             uint8_t maxOpenFiles = 10, const char *partitionLabel = "spiffs");
  void end();

  FsFile open(const char *path, const char *mode = "r", bool create = false);
  FsFile open(const String &path, const char *mode = "r", bool create = false);

  bool exists(const char *path);
  bool exists(const String &path);
  bool remove(const char *path);
  bool remove(const String &path);
  bool rename(const char *from, const char *to);
  bool rename(const String &from, const String &to);
  bool mkdir(const char *path);
  bool rmdir(const char *path);

  size_t totalBytes();
  size_t usedBytes();

  // Traduce una ruta del firmware ("/x.log") a la del VFS ("/littlefs/x.log").
  String fullPath(const char *path) const;

private:
  String base_ = "/littlefs";
  const char *label_ = "spiffs";
  bool mounted_ = false;
};

extern LittleFsWrapper LittleFS;
