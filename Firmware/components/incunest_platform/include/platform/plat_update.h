#pragma once

// Escritura de un firmware nuevo en la particion OTA libre, con la forma del
// objeto Update de Arduino, sobre esp_ota_ops (ESP-IDF puro).
//
// Lo usa el manejador de /update del flasheo por WiFi en las dos placas. La
// OTA de ThingsBoard NO pasa por aqui: usa FwGuardedUpdater (shared/), que
// ya iba sobre el Espressif_Updater del SDK, que es IDF nativo.
//
// UPDATE_SIZE_UNKNOWN se traduce a OTA_WITH_SEQUENTIAL_WRITES: la particion se
// borra sector a sector segun se escribe, que es exactamente lo que hacia
// Arduino. La alternativa (OTA_SIZE_UNKNOWN) borra los 5 MB del slot de golpe
// al empezar y deja al navegador varios segundos sin respuesta.

#include <cstddef>
#include <cstdint>

#include "esp_ota_ops.h"
#include "platform/plat_print.h"

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class UpdateClass {
public:
  bool begin(size_t size = UPDATE_SIZE_UNKNOWN);
  size_t write(const uint8_t *data, size_t len);
  // evenIfRemaining: Arduino cerraba aunque quedasen bytes por llegar si el
  // tamano era desconocido. Aqui siempre cierra y marca la particion de
  // arranque si no hubo error.
  bool end(bool evenIfRemaining = false);
  void abort();

  bool hasError() const { return error_ != 0; }
  bool isFinished() const { return finished_; }
  bool isRunning() const { return running_; }
  size_t progress() const { return written_; }
  const char *errorString() const;
  void printError(Print &out);

private:
  esp_ota_handle_t handle_ = 0;
  const esp_partition_t *part_ = nullptr;
  bool running_ = false;
  bool finished_ = false;
  size_t written_ = 0;
  int error_ = 0; // esp_err_t del ultimo fallo, 0 si ninguno
};

extern UpdateClass Update;
