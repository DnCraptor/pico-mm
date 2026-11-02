/* Cooperative flash-LIBRARY C CSUB resident service (API v27). */
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
/* A PIO claim owns one *whole block* (all four SMs and instruction memory),
 * never just an SM. Resident DMA is managed by API v27.
 * Existing hardware must be idle. Claim alone does not change PIO memory.
 * Explicit PROGRAM replaces instruction memory; stop/restart/erase clears
 * the claimed PIO and releases all PIO GPIOs. No RUN-owned buffers survive.
 * Calls return 0 on success and -1 when invalid/unavailable. */
int pm_resident_pio_claim(int pio);
void pm_resident_pio_release(void);
int pm_resident_pio_pin(int gp); /* Configure/reserve GPIO for claimed PIO. */
int pm_resident_pio_program(const uint16_t *words, unsigned count);
int pm_resident_pio_init_machine(int sm, uint32_t pinctrl,
                                 uint32_t execctrl, uint32_t shiftctrl,
                                 int start, unsigned clock_hz,
                                 int sideout, int setout, int outout);
int pm_resident_pio_start(int sm);
int pm_resident_pio_stop(int sm);
/* Managed DMA on a claimed resident PIO block; TX=1, RX=0.
 * Data lives ONLY in the 64-byte resident state (16 aligned words).
 * offset_words/count select a contiguous subrange. Concurrent RX/TX must
 * not overlap; do not modify a DMA-owned region until stopped. 32-bit data
 * transfers only. cyclic=1 is address-reload RX/TX (RX count power of two).
 * 0 on success, -1 for invalid/unavailable resources. No IRQ callbacks.
 * Channel ownership survives END/RUN and releases before PIO/CSUB teardown. */
int pm_resident_pio_dma_start(int tx, int sm, unsigned offset_words,
                               unsigned count, int cyclic);
int pm_resident_pio_dma_busy(int tx); /* -1 when not owned */
int pm_resident_pio_dma_stop(int tx);
/* 64 bytes allocated from the system heap on installation, not BASIC's
 * resettable heap. Survives RUN/END, zeroed on start, freed on stop/unload.
 * Returns NULL when no resident is installed. */
uint32_t *pm_resident_state(void);
#endif
