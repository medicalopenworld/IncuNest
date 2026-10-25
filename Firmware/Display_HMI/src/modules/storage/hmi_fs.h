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

// CAMBIO DE SEMANTICA (revision de codigo, C1): HmiFs_Begin() YA NO FORMATEA
// EN EL ACTO. Antes montaba con formateo si fallaba, y en campo la particion
// "spiffs" (~6 MB) esta virgen o lleva una imagen SPIFFS antigua: ese primer
// formateo podia borrarla entera de una vez y dejar a UI_Task parado decenas de
// segundos, sin banner ni sonido de alarma. Ahora:
//   - Begin() solo INTENTA montar (sin formatear) y devuelve el resultado.
//   - Si no hay imagen valida, la particion pasa a HmiFsState::NeedsFormat y
//     Begin() devuelve false (sin reintentar el montaje, para no llenar el log).
//   - El formateo es DIFERIDO y lo reparte HmiFs_Service(): se borra la particion
//     por rangos de 64 KB (esp_partition_erase_range), un rango por pasada y con
//     >= 100 ms entre ellos, SOLO mientras no haya alarma activa y el enlace con
//     la placa este vivo; despues LittleFS.format() sobre la particion ya
//     borrada y un montaje. Registra el tiempo total.
//   - Mientras tanto Begin() devuelve false: quien lo use debe funcionar solo en
//     RAM (la tendencia lo hace) y volver a llamar a Begin() mas tarde.
// Alternativa de produccion: flashear una imagen LittleFS vacia en 0xA10000 y no
// formatear nunca en campo (pendiente del flasher, no incluido aqui).

// Intenta montar. Idempotente: tras un montaje correcto devuelve true sin
// repetir nada. Nunca formatea. Registra el heap interno consumido al montar.
bool HmiFs_Begin(void);

enum class HmiFsState : uint8_t {
  Unmounted,    // aun no se ha intentado montar
  Mounted,
  NeedsFormat,  // no hay imagen valida; formateo diferido pendiente
  Formatting,   // borrando por rangos / formateando (en curso)
  Failed,       // el formateo fallo: se rinde para siempre (sin persistencia)
};
HmiFsState HmiFs_State(void);

// Hace UN paso del formateo diferido si toca (ver arriba). Llamar en cada
// vuelta del bucle de UI_Task, fuera de LVGL_Lock. `ioAllowed` = sin alarma
// activa y con el enlace vivo (trend_io_allowed): con false no empieza ni
// continua (queda en pausa). `nowMs` = millis(). No hace nada fuera de
// NeedsFormat/Formatting.
void HmiFs_Service(bool ioAllowed, uint32_t nowMs);

bool HmiFs_IsMounted(void);

// Espacio total y usado, en bytes. false si no esta montado.
bool HmiFs_Info(size_t *total, size_t *used);

// Tamano de un fichero, o -1 si no existe / no esta montado.
int32_t HmiFs_FileSize(const char *path);

// Lee el fichero entero a `buf` (capacidad `cap`). false si no existe, no cabe
// o la lectura se queda corta. `outLen` recibe los bytes leidos.
bool HmiFs_ReadAll(const char *path, uint8_t *buf, size_t cap, size_t *outLen);

bool HmiFs_Remove(const char *path);

// Lectura INCREMENTAL de un fichero (para no leer 100 KB de una vez): open()
// devuelve el tamano (-1 si no existe), read() lee hasta n bytes y devuelve los
// leidos (0 al final o en error). close() es idempotente y lo llama el
// destructor.
class HmiFsReader {
 public:
  HmiFsReader() = default;
  ~HmiFsReader() { close(); }
  HmiFsReader(const HmiFsReader &) = delete;
  HmiFsReader &operator=(const HmiFsReader &) = delete;

  int32_t open(const char *path);
  size_t read(uint8_t *buf, size_t n);
  void close(void);
  bool isOpen(void) const { return open_; }

 private:
  fs::File file_;
  bool open_ = false;
};

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
