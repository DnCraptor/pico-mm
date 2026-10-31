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

/* These mirror BASIC PIO PROGRAM / PROGRAM LINE / CLEAR.
 * PROGRAM replaces instruction memory starting at slot zero, stops all four
 * state machines, and accepts 1..32 16-bit instructions.
 * CLEAR also resets the SM registers and FIFOs. These affect the whole PIO
 * block; use only a block reserved for the C program. */
void pm_pio_program(int pio, const uint16_t *words, size_t count);
void pm_pio_program_line(int pio, int slot, uint16_t instruction);
void pm_pio_clear(int pio);
/* sm = -1 returns raw FLEVEL register; otherwise return the selected
 * FIFO occupancy (0..15), where tx=1 selects TX and tx=0 selects RX. */
uint32_t pm_pio_flevel(int pio, int sm, int tx);

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
