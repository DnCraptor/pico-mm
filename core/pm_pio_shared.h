#ifndef PICOMITE_PM_PIO_SHARED_H
#define PICOMITE_PM_PIO_SHARED_H
#include <stddef.h>
#include <stdint.h>

/* Common hardware implementation used directly by both BASIC and C. */
int pm_pio_available_hw(int pio);
void pm_pio_exec_hw(int pio, int sm, uint16_t instruction);
void pm_pio_write_hw(int pio, int sm, uint32_t value);
int pm_pio_read_hw(int pio, int sm, uint32_t *value);
void pm_pio_start_hw(int pio, int sm);
void pm_pio_stop_hw(int pio, int sm);
void pm_pio_init_hw(int pio, int sm, uint32_t pinctrl, uint32_t execctrl,
                    uint32_t shiftctrl, int start, float clock,
                    int sideout, int setout, int outout);

/* Shared with BASIC PIO PROGRAM / PROGRAM LINE / CLEAR / PIO(FLEVEL). */
void pm_pio_program_hw(int pio, const uint16_t *words, size_t count);
void pm_pio_program_line_hw(int pio, int slot, uint16_t instruction);
void pm_pio_clear_hw(int pio);
uint32_t pm_pio_flevel_hw(int pio, int sm, int tx);

/* DMA transfer size in bits: 8, 16 or 32. count is number of samples.
 * Caller retains the buffer while DMA is active. 1 = TX, 0 = RX. */
void pm_pio_dma_transfer_hw(int tx, int pio, int sm, void *buffer,
                            size_t capacity_bytes, uint32_t count,
                            unsigned bits, int from_c);
void pm_pio_dma_stop_hw(int tx);
int pm_pio_dma_busy_hw(int tx);
#endif
