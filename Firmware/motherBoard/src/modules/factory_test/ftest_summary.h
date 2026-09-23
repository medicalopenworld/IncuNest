#pragma once
#include <stdint.h>

#include "factory_test.h"

#ifdef __cplusplus
extern "C" {
#endif

// Acumulador puro de resultados del test de fabrica (design.md D5,
// mb-factory-test "Batería completa en orden fijo" / "Persistencia del
// resultado"). Sin I/O: la tarea `FTEST` llama a `ftest_summary_note()` una
// vez por resultado final y usa el resultado para `CTRL,FTEST_DONE` y para
// las tres mascaras que se persisten en NVS (`mb_ftest`).
typedef struct {
  uint32_t pass_mask;
  uint32_t fail_mask;
  uint32_t warn_mask;
  uint32_t run_mask;
  uint8_t pass;
  uint8_t fail;
  uint8_t skip;
  uint8_t warn;
} FtestSummary;

void ftest_summary_init(FtestSummary *s);

// Anota un resultado. Ignora silenciosamente RUNNING/WAIT/CONFIRM (no son
// resultados finales) y cualquier id que no sea de motherBoard. Un mismo id
// notificado dos veces con estado final cuenta una sola vez: la marca en
// `run_mask` hace que la segunda llamada sea un no-op, para no inflar los
// contadores si algo, por error, emite dos veces el cierre de un test.
// WARN es tambien un estado FINAL (shared-factory-test-bench): cuenta en
// `warn`/`warn_mask`, igual que PASA/FALLA en las suyas.
void ftest_summary_note(FtestSummary *s, unsigned id, FtestStatus st);

// Fusion de un reintento (`HMI,FTEST,RUN,<id>`) sobre las cuatro mascaras ya
// persistidas de una bateria anterior: limpia el bit de `id` en PASA, FALLA y
// AVISO, marca siempre `run_mask` y lo vuelve a poner en la mascara que
// corresponda a `st` (SKIP no marca ninguna de las tres, solo queda como
// "ejecutado").
void ftest_summary_merge_single(uint32_t *pass_mask, uint32_t *fail_mask,
                                 uint32_t *warn_mask, uint32_t *run_mask,
                                 unsigned id, FtestStatus st);

#ifdef __cplusplus
}
#endif
