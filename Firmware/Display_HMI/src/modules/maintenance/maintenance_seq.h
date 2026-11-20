#pragma once
// Decision pura de "esto ha sido un alta": la parte de Maintenance_Tick() que
// compara el seq al mando con el ultimo visto. Aparte, sin Arduino ni NVS, para
// poder probarla en host.
//
// Alta = el seq pasa de uno conocido a otro (0 incluido): deja pendiente una
// limpieza terminal. NO lo es que el HMI DESCARTE una identidad que no pudo
// confirmar con la placa (otro bebe al mando, otra placa, NVS borrada): ahi no
// ha salido nadie de esta incubadora, y una limpieza terminal pendiente es un
// aviso falso, ademas persistido. `droppedSeq` es el seq de esa identidad
// descartada (0 = no hubo descarte).
#include <stdbool.h>
#include <stdint.h>

struct MntSeqDecision {
  bool changed;               // el seq cambio: hay que guardar `curSeq`
  bool markTerminalPending;   // ...y ademas cuenta como alta
};

MntSeqDecision mnt_seq_decide(uint32_t lastSeq, uint32_t curSeq,
                              uint32_t droppedSeq);
