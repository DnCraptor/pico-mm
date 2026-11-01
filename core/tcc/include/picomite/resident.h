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
/* Exactly one exclusive GPIO subscription per installed resident. Valid
 * edges: PM_RESIDENT_GPIO_RISE/FALL/BOTH; pull: NONE/UP/DOWN.
 * The callback is also required to live in flash LIBRARY CSUB code.
 * It runs from routinechecks() on the foreground core, NOT in GPIO IRQ.
 * 'events' is a mask of latched edges, not a reliable edge count: multiple
 * changes can coalesce, especially during RUN .c/.elf or long BASIC work.
 * The GPIO reservation survives END/RUN and is released on stop/restart or
 * flash LIBRARY deletion. A failed installation returns -1. */
#define PM_RESIDENT_GPIO_RISE 1u
#define PM_RESIDENT_GPIO_FALL 2u
#define PM_RESIDENT_GPIO_BOTH 3u
#define PM_RESIDENT_GPIO_NOPULL 0
#define PM_RESIDENT_GPIO_PULLUP 1
#define PM_RESIDENT_GPIO_PULLDOWN 2
int pm_resident_gpio_on_edge(int gp, unsigned edges, int pull,
    void (*callback)(int gp, unsigned events, void *state));
void pm_resident_gpio_off(void);
/* 64 bytes allocated from the system heap on installation, not BASIC's
 * resettable heap. Survives RUN/END, zeroed on start, freed on stop/unload.
 * Returns NULL when no resident is installed. */
uint32_t *pm_resident_state(void);
#endif
