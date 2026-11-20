#include "maintenance_seq.h"

MntSeqDecision mnt_seq_decide(uint32_t lastSeq, uint32_t curSeq,
                              uint32_t droppedSeq) {
  MntSeqDecision d = {false, false};
  if (curSeq == lastSeq) return d;
  d.changed = true;
  // Una alta es dejar atras a un bebe conocido. El descarte de una identidad
  // sin confirmar que lleva a "nadie" (el mismo seq que se tenia) no lo es.
  const bool droppedUnconfirmed = (curSeq == 0 && droppedSeq == lastSeq);
  d.markTerminalPending = (lastSeq != 0) && !droppedUnconfirmed;
  return d;
}
