#pragma once
// Sistema de ficheros persistente del HMI: LittleFS sobre la particion "spiffs"
// que ya existe en partitions/hmi_16mb_ota.csv (0xA10000, 0x5E0000, ~5,9 MB).
// No toca la tabla de particiones: LittleFS monta por ETIQUETA (como hace la
// motherBoard), el subtipo spiffs es solo el nombre historico.
//
// REGLAS DE USO
//  - Nunca con LVGL_Lock tomado (ARQ-LOCK-001): cada operacion es E/S de flash.
//  - Montar DESPUES de que el panel RGB haya reservado sus bounce buffers
//    (38,4 KB contiguos DMA-capaces x2, en UI_Task): el montaje asigna heap
//    interno y no debe competir con esa reserva. En la practica: desde el
//    bucle de UI_Task, no desde setup().
//  - Una escritura en flash para la cache y puede dar un parpadeo benigno del
//    panel (ver el aviso en platformio.ini). Para ficheros grandes, escribir en
//    trozos de un bloque (4 KB) con holgura entre ellos: HmiFsAtomicWriter.
//  - Las funciones no son reentrantes entre tareas: las llama UI_Task.
//
// Pensado para ser reutilizado por mas modulos (tendencia, y el diario de
// eventos que vendra): rutas absolutas dentro del FS ("/trend.bin").

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

// Monta el sistema de ficheros. Idempotente: tras un montaje correcto devuelve
// true sin repetir nada. Si el montaje falla (particion virgen o corrupta),
// formatea UNA vez y reintenta; si aun asi falla, se rinde para siempre
// (false) en vez de formatear en bucle. Registra el heap interno consumido.
bool HmiFs_Begin(void);

bool HmiFs_IsMounted(void);

// Espacio total y usado, en bytes. false si no esta montado.
bool HmiFs_Info(size_t *total, size_t *used);

// Tamano de un fichero, o -1 si no existe / no esta montado.
int32_t HmiFs_FileSize(const char *path);

// Lee el fichero entero a `buf` (capacidad `cap`). false si no existe, no cabe
// o la lectura se queda corta. `outLen` recibe los bytes leidos.
bool HmiFs_ReadAll(const char *path, uint8_t *buf, size_t cap, size_t *outLen);

bool HmiFs_Remove(const char *path);

// Escritura ATOMICA e incremental: se escribe en `path` + ".tmp" y solo
// commit() lo renombra sobre `path`. Un corte de corriente a mitad deja el
// fichero anterior intacto y, como mucho, un .tmp huerfano que el siguiente
// begin() trunca.
class HmiFsAtomicWriter {
 public:
  HmiFsAtomicWriter() = default;
  ~HmiFsAtomicWriter() { abort(); }
  HmiFsAtomicWriter(const HmiFsAtomicWriter &) = delete;
  HmiFsAtomicWriter &operator=(const HmiFsAtomicWriter &) = delete;

  // Abre (y trunca) el temporal. false si no esta montado o no se pudo abrir.
  bool begin(const char *path);
  // Escribe; devuelve los bytes escritos (menos que n = fallo).
  size_t write(const uint8_t *data, size_t n);
  // Cierra y renombra el temporal sobre el destino. false si algo fallo (el
  // destino anterior se conserva si el renombrado no llego a ocurrir).
  bool commit(void);
  // Cierra y borra el temporal. Idempotente; lo llama el destructor.
  void abort(void);
  bool isOpen(void) const { return open_; }

 private:
  static constexpr size_t kPathMax = 48;
  fs::File file_;
  char path_[kPathMax] = {0};
  char tmp_[kPathMax + 8] = {0};
  bool open_ = false;
};

// Escritura atomica de una sola vez (bloquea mientras dura). Para datos
// pequenos; para ficheros grandes usar HmiFsAtomicWriter en trozos.
bool HmiFs_WriteAtomic(const char *path, const uint8_t *data, size_t len);
