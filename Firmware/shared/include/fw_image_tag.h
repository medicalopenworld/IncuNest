#pragma once

#include <cstddef>
#include <cstdint>

// Marca de placa embebida en el binario, para que cada placa pueda RECHAZAR
// por si misma un firmware que no es suyo.
//
// Por que existe: el 2026-09-08 el flasher WiFi clasifico un Display HMI como
// motherBoard (su hostname mDNS es "IncuNest-Display-<sn>" y el parser solo
// reconocia "IncuNest_Display", con guion bajo, asi que caia en la rama
// generica "IncuNest") y le subio por OTA el firmware de la MB. Nada lo paro:
// las dos placas son ESP32-S3, el binario de la MB (1,5 MB) entra de sobra en
// el slot de app del HMI (5 MB) y esp_ota solo valida el chip y el tamano. La
// unidad arranco con firmware de motherBoard: pantalla violeta (nadie crea el
// panel RGB), sin tactil, sin enlace con la placa — y la MB dando alarma. Solo
// se recupera por USB.
//
// La leccion es que arreglar la herramienta no basta: el .exe que corre el
// operario puede ser una version antigua, o alguien puede subir el fichero a
// mano desde el navegador. La unica barrera que sobrevive a una herramienta
// equivocada es la del propio dispositivo.
//
// Por que no vale el descriptor de la app: en las builds Arduino/PlatformIO
// esp_app_desc_t.project_name es "arduino-lib-builder" en las DOS placas (se
// hereda del core precompilado), asi que no distingue nada. La marca de aqui
// es una cadena normal en .rodata: viaja dentro del binario y se puede buscar
// en el flujo OTA segun llega, sin conocer su posicion.

#define FW_TAG_PREFIX           "IncuNestFW:"
#define FW_BOARD_ID_MOTHERBOARD "motherboard"
#define FW_BOARD_ID_DISPLAY_HMI "display_hmi"

// IMPORTANTE: cada firmware define y busca UNICAMENTE su propia marca
// (FW_TAG_PREFIX FW_BOARD_ID_<suya>). Si un binario llevase ademas la cadena
// completa de la otra placa, la comprobacion de esa otra placa lo aceptaria.
// Por eso este fichero no tiene ninguna tabla que junte las dos, y
// shared/src/fw_image_tag.cpp no menciona ningun identificador de placa.

// Busca un patron ASCII en un flujo que llega troceado (los ~1,4 KB por
// callback de HTTPUpload), sin bufferizar el binario entero: se queda con los
// ultimos patLen-1 bytes de cada trozo para no perder las coincidencias que
// caen justo en la frontera.
class FwStreamMatcher {
public:
  static const size_t kMaxPattern = 32;

  // pattern debe ser un literal (o vivir mas que el matcher). Un patron vacio
  // o de mas de kMaxPattern bytes desactiva el matcher: found() siempre falso.
  explicit FwStreamMatcher(const char *pattern);

  void reset();
  void feed(const uint8_t *data, size_t len);
  bool found() const { return found_; }

private:
  bool contains(const uint8_t *buf, size_t len) const;

  const char *pat_;
  size_t patLen_;
  uint8_t tail_[kMaxPattern];
  size_t tailLen_;
  bool found_;
};

// Veredicto sobre un binario recien recibido.
//
// Se rechaza SOLO con prueba positiva de que es de otra placa: lleva marca y
// no es la nuestra. Un binario sin ninguna marca (cualquier build anterior a
// este cambio) se acepta, para no bloquear un rollback por WiFi a una version
// antigua — esas builds no son peligrosas por si mismas, y bloquearlas dejaria
// la flota sin camino de vuelta.
bool fw_image_is_foreign(bool selfTagSeen, bool anyTagSeen);

// Cada firmware define esta cadena UNA vez con SU identificador:
//   extern "C" const char kFwBoardTag[] = FW_TAG_PREFIX FW_BOARD_ID_<la suya>;
// Se declara aqui para que el resto de ficheros de esa placa (la OTA por GPRS,
// por ejemplo) puedan usarla sin repetir el identificador.
extern "C" const char kFwBoardTag[];
