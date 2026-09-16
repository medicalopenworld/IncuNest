#pragma once

// Almacen de configuracion sobre nvs_flash (ESP-IDF puro). Sustituye a la
// clase Preferences de arduino-esp32.
//
// ================== COMPATIBILIDAD DE DATOS: NO TOCAR ==================
// Las unidades que ya estan en campo (Togo) tienen la NVS escrita por la
// Preferences de Arduino. Un OTA a este firmware NO puede perder esos datos:
// ahi viven los perfiles de bebe, la configuracion de control y las claves.
// Por eso cada metodo reproduce EXACTAMENTE la llamada nvs_* que hacia
// Arduino, verificado leyendo su Preferences.cpp:
//
//   putChar/getChar      -> nvs_set_i8  / nvs_get_i8
//   putUChar/getUChar    -> nvs_set_u8  / nvs_get_u8
//   putShort/getShort    -> nvs_set_i16 / nvs_get_i16
//   putUShort/getUShort  -> nvs_set_u16 / nvs_get_u16
//   putInt/getInt        -> nvs_set_i32 / nvs_get_i32
//   putUInt/getUInt      -> nvs_set_u32 / nvs_get_u32
//   putLong/getLong      -> alias de Int  (i32)
//   putULong/getULong    -> alias de UInt (u32)
//   putLong64/getLong64  -> nvs_set_i64 / nvs_get_i64
//   putULong64/...       -> nvs_set_u64 / nvs_get_u64
//   putString/getString  -> nvs_set_str / nvs_get_str
//   putBytes/getBytes    -> nvs_set_blob / nvs_get_blob
//   putBool/getBool      -> u8 con valor 1/0; getBool es (getUChar(...) == 1)
//   putFloat/getFloat    -> BLOB de sizeof(float), NO un tipo numerico de NVS
//   putDouble/getDouble  -> BLOB de sizeof(double)
//
// Las dos ultimas son la trampa: float y double se guardaban como blob. Si
// alguien las "mejora" a un tipo nativo de NVS, las 52 escrituras y 18
// lecturas de float del firmware dejan de encontrar lo que hay en las placas
// y la unidad arranca con los valores por defecto. Ese es el fallo silencioso
// que este comentario existe para evitar.
//
// Tambien se conserva el nvs_commit() despues de CADA escritura, como hacia
// Arduino: cambia el desgaste de la flash, y con el a su vez el
// comportamiento ante un corte de corriente a mitad de guardado.
// =======================================================================
//
// Los nombres de los metodos se conservan (putFloat, getUChar...) para que las
// ~167 referencias del firmware no cambien: lo unico que se renombra es el
// tipo, de Preferences a NvsPrefs, que son ~60 declaraciones.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "nvs.h"
#include "platform/plat_string.h"
#include "nvs_flash.h"

class NvsPrefs {
public:
  NvsPrefs() = default;
  ~NvsPrefs();

  NvsPrefs(const NvsPrefs &) = delete;
  NvsPrefs &operator=(const NvsPrefs &) = delete;

  // Devuelve false si ya estaba abierta o si nvs_open falla (igual que antes).
  bool begin(const char *name, bool readOnly = false);
  void end();

  bool clear();
  bool remove(const char *key);
  bool isKey(const char *key);

  size_t putChar(const char *key, int8_t value);
  size_t putUChar(const char *key, uint8_t value);
  size_t putShort(const char *key, int16_t value);
  size_t putUShort(const char *key, uint16_t value);
  size_t putInt(const char *key, int32_t value);
  size_t putUInt(const char *key, uint32_t value);
  size_t putLong(const char *key, int32_t value);
  size_t putULong(const char *key, uint32_t value);
  size_t putLong64(const char *key, int64_t value);
  size_t putULong64(const char *key, uint64_t value);
  size_t putFloat(const char *key, float value);
  size_t putDouble(const char *key, double value);
  size_t putBool(const char *key, bool value);
  size_t putString(const char *key, const char *value);
  size_t putString(const char *key, const std::string &value);
  size_t putString(const char *key, const String &value);
  size_t putBytes(const char *key, const void *value, size_t len);

  int8_t getChar(const char *key, int8_t defaultValue = 0);
  uint8_t getUChar(const char *key, uint8_t defaultValue = 0);
  int16_t getShort(const char *key, int16_t defaultValue = 0);
  uint16_t getUShort(const char *key, uint16_t defaultValue = 0);
  int32_t getInt(const char *key, int32_t defaultValue = 0);
  uint32_t getUInt(const char *key, uint32_t defaultValue = 0);
  int32_t getLong(const char *key, int32_t defaultValue = 0);
  uint32_t getULong(const char *key, uint32_t defaultValue = 0);
  int64_t getLong64(const char *key, int64_t defaultValue = 0);
  uint64_t getULong64(const char *key, uint64_t defaultValue = 0);
  float getFloat(const char *key, float defaultValue = 0.0f);
  double getDouble(const char *key, double defaultValue = 0.0);
  bool getBool(const char *key, bool defaultValue = false);
  size_t getString(const char *key, char *value, size_t maxLen);
  std::string getString(const char *key, const char *defaultValue = "");
  size_t getBytesLength(const char *key);
  size_t getBytes(const char *key, void *buf, size_t maxLen);


private:
  nvs_handle_t handle_ = 0;
  bool started_ = false;
  bool read_only_ = false;
};

// Arranca la particion NVS por defecto. Arduino lo hacia por su cuenta al
// iniciar el core; aqui hay que llamarlo una vez en el arranque de la app,
// antes del primer NvsPrefs::begin(). Reintenta tras borrar si la particion
// esta llena o es de una version anterior, igual que el ejemplo de IDF.
void plat_nvs_init(void);
