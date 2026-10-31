/* Native C PIO API shared by RP2040 and RP2350 RUN/ELF programs. */
#ifndef PICOMITE_NATIVE_PIO_H
#define PICOMITE_NATIVE_PIO_H
#include <stddef.h>
#include <stdint.h>

/* All operations require a running C program. Availability depends on
 * firmware resources (HDMI/video, USB and CYW43 may reserve PIO blocks). */
int pm_pio_available(int pio);
/* 1 only if firmware hasn't reserved the block, all its SMs are disabled,
 * no PIO FIFO event handlers are installed and no PIO DMA is active there.
 * Checks SDK instruction reservations too, but cannot detect instructions
 * written directly through BASIC PIO PROGRAM LINE. */
int pm_pio_idle(int pio);
/* RP2350-only BASIC PIO WRITEFIFO / SET BASE extensions. On RP2040 these
 * report a PicoMite error rather than touching unsupported registers. */
void pm_pio_writefifo(int pio,int sm,int fifo,uint32_t value);
void pm_pio_set_base(int pio,int base);
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
/* Read the DMA address register as BASIC PIO(DMA ... POINTER) does.
 * Only the DMA channel belonging to this C RUN may be queried. */
uintptr_t pm_pio_dma_pointer(int tx);

/* FIFO event conditions match BASIC PIO INTERRUPT. tx=0: RX nonempty;
 * tx=1: TX falls from full to a nonzero non-full level.
 * Callbacks run in normal C context when the API is serviced or pm_pio_poll
 * is called. They are never executed from an IRQ. Pass NULL to unregister. */
void pm_pio_on_fifo(int pio, int sm, int tx,
                    void (*callback)(int pio, int sm));
/* One-shot completion: RX DMA finishes, or TX DMA and TX FIFO both empty.
 * Not invoked for continuously retriggered transfers. */
void pm_pio_on_dma_done(int tx, void (*callback)(int tx));
void pm_pio_poll(void);

/* PIO CONFIGURE, using the same configurePIO() implementation as BASIC.
 * GPIO bases use the PIO hardware pin coordinates (0..31), not BASIC pin
 * designators. All Boolean fields must be 0 or 1. Pass a fully initialized
 * structure; start_address and wrap fields must refer to loaded PIO code.
 */
typedef struct pm_pio_config {
    unsigned clock_hz;
    int start_address;
    int sideset_base, sideset_count, sideset_output;
    int set_base, set_count, set_output;
    int out_base, out_count, out_output;
    int in_base, jmp_pin;
    int wrap_target, wrap;
    int side_enable, side_pindir;
    int push_threshold, pull_threshold;
    int autopush, autopull, in_shift_right, out_shift_right;
    int join_rx_fifo, join_tx_fifo;
    /* RP2350-specific FIFO modes; must be zero on RP2040. */
    int join_rx_get, join_rx_put;
} pm_pio_config;
void pm_pio_configure(int pio, int sm, const pm_pio_config *config);

/* PIO SYNC: masks select SMs in the current/adjacent PIO instances.
 * A zero prev/next mask means that neighbour is excluded. */
void pm_pio_sync(int pio, unsigned my, unsigned prev, unsigned next);

#endif
