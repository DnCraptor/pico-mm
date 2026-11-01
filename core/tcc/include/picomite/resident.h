/* Cooperative flash-LIBRARY C CSUB resident service (API v24). */
#ifndef _PICOMITE_RESIDENT_H
#define _PICOMITE_RESIDENT_H
#include <stdint.h>

/* Register only from a CSUB linked in FLASH LIBRARY. No RUN-scoped pointers.
 * Period is 10..60000 ms. Callback runs on the main thread, not in IRQ.
 * It must not call BASIC, allocate memory or use RUN-scoped I/O/IRQ/DMA.
 * Returns 0 on success, -1 if registration is unsafe or unsupported. */
int pm_resident_start(void (*callback)(void *state), unsigned interval_ms);
void pm_resident_stop(void);
int pm_resident_active(void);
/* 64 bytes allocated from the system heap on installation, not BASIC's
 * resettable heap. Survives RUN/END, zeroed on start, freed on stop/unload.
 * Returns NULL when no resident is installed. */
uint32_t *pm_resident_state(void);
#endif
