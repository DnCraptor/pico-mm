/* Native C PIO API shared by RP2040 and RP2350 RUN/ELF programs. */
#ifndef PICOMITE_NATIVE_PIO_H
#define PICOMITE_NATIVE_PIO_H
#include <stddef.h>
#include <stdint.h>

/* All operations require a running C program. Availability depends on
 * firmware resources (HDMI/video, USB and CYW43 may reserve PIO blocks). */
int pm_pio_available(int pio);
void pm_pio_exec(int pio, int sm, uint16_t instruction);
void pm_pio_write(int pio, int sm, uint32_t value);
/* Returns 1 when a word was read, 0 when RX FIFO was empty. */
int pm_pio_read(int pio, int sm, uint32_t *value);
void pm_pio_start(int pio, int sm);
void pm_pio_stop(int pio, int sm);
void pm_pio_init_machine(int pio, int sm, uint32_t pinctrl,
                         uint32_t execctrl, uint32_t shiftctrl,
                         int start, float clock_hz,
                         int sideout, int setout, int outout);

/* One-shot DMA from/to a PIO FIFO. Buffer MUST be in a live malloc/pm_alloc
 * block of the current C run and remain there until the transfer ends.
 * The runtime aborts C-owned DMA on exit/CTRL-C before freeing its heap. */
void pm_pio_dma_rx(int pio, int sm, void *buffer,
                   size_t capacity_bytes, uint32_t count, unsigned bits);
void pm_pio_dma_tx(int pio, int sm, const void *buffer,
                   size_t capacity_bytes, uint32_t count, unsigned bits);
int pm_pio_dma_busy(int tx);
void pm_pio_dma_stop(int tx);

#endif
