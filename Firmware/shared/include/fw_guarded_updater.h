#pragma once

#include <Espressif_Updater.h>

#include "fw_image_tag.h"

// Espressif_Updater con la misma guarda de placa que /update: mira la marca
// que va dentro del binario segun llega y, si es de otra placa, no confirma la
// particion. Solo hace falta cambiar el tipo de la variable global que se le
// pasa a OTA_Update_Callback; el resto del flujo de ThingsBoard no se entera.
//
// Por que tambien aqui: el flasher WiFi es un riesgo de fabrica, pero la OTA de
// ThingsBoard es un riesgo de flota. Un fichero subido al slot que no toca
// (mas facil todavia con la OTA en cascada, donde la MB es el unico
// dispositivo en ThingsBoard) llega igual a la placa, y por ahi no hay ningun
// operario que pueda parar la unidad.
//
// La marca se pasa por constructor a proposito: este fichero se compila en las
// dos placas, y una cadena de placa escrita aqui acabaria dentro de los dos
// binarios, que es justo lo que romperia la comprobacion (ver fw_image_tag.h).
class FwGuardedUpdater : public IUpdater {
public:
  explicit FwGuardedUpdater(const char *selfTag)
      : selfTag_(selfTag), anyTag_(FW_TAG_PREFIX), foreign_(false) {}

  bool begin(size_t const &firmware_size) override {
    selfTag_.reset();
    anyTag_.reset();
    foreign_ = false;
    return inner_.begin(firmware_size);
  }

  size_t write(uint8_t *const payload, size_t const &total_bytes) override {
    selfTag_.feed(payload, total_bytes);
    anyTag_.feed(payload, total_bytes);
    return inner_.write(payload, total_bytes);
  }

  void reset() override {
    selfTag_.reset();
    anyTag_.reset();
    foreign_ = false;
    inner_.reset();
  }

  bool end() override {
    if (fw_image_is_foreign(selfTag_.found(), anyTag_.found())) {
      foreign_ = true;
      inner_.reset(); // aborta la escritura: otadata se queda como estaba
      return false;
    }
    return inner_.end();
  }

  // true si la ultima actualizacion se rechazo por ser de otra placa (para
  // distinguirlo de un fallo de descarga en el log).
  bool rejectedForeignImage() const { return foreign_; }

private:
  Espressif_Updater inner_;
  FwStreamMatcher selfTag_;
  FwStreamMatcher anyTag_;
  bool foreign_;
};
