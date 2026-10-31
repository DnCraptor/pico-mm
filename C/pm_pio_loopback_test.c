/* On-chip PIO loopback with no external pins. Requires an otherwise idle
 * PIO block. Change TEST_PIO_BLOCK to a free block for your board.
 * PIO PROGRAM / CLEAR affect ALL SMs in that block, including video/audio!
 */
#include <picomite/pio.h>
#include <picomite/system.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_PIO_BLOCK
#define TEST_PIO_BLOCK (-1) /* safe default: non-destructive probe only */
#endif

static volatile int rx_events, rx_dma_done, tx_dma_done;
static void on_fifo(int pio, int sm) { (void)pio; (void)sm; ++rx_events; }
static void on_dma(int tx) { if (tx) ++tx_dma_done; else ++rx_dma_done; }
static const uint16_t echo_program[] = {
    0x80a0, /* pull block */
    0xa0c7, /* mov isr, osr */
    0x8020, /* push block */
    0x0000  /* jmp 0 */
};
int main(void)
{
    const int pio = TEST_PIO_BLOCK, sm = 0;
    unsigned i;
    uint32_t got = 0;
    uint32_t *tx = NULL, *rx = NULL;
    int result = 1, armed_rx = 0, armed_tx = 0;
    pm_pio_config config = {0};
    unsigned long long deadline;
    for (i = 0; i < 3; ++i)
        printf("PIO%u available=%d\n", i, pm_pio_available((int)i));
    if (pio < 0) {
        puts("Non-destructive probe passed. Define TEST_PIO_BLOCK for loopback.");
        return 0;
    }
    if (!pm_pio_available(pio)) {
        puts("Selected PIO block reserved by firmware");
        return 2;
    }
    pm_pio_clear(pio);
    pm_pio_program(pio, echo_program, 4);
    config.clock_hz = 15000000;
    config.jmp_pin = -1;
    config.wrap = 3;
    config.in_shift_right = 1;
    config.out_shift_right = 1;
    pm_pio_configure(pio, sm, &config);
    pm_pio_sync(pio, 1u << sm, 0, 0);
    pm_pio_start(pio, sm);
    pm_pio_on_fifo(pio, sm, 0, on_fifo);
    pm_pio_write(pio, sm, 0x13579bdfu);
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && !rx_events) pm_pio_poll();
    pm_pio_on_fifo(pio, sm, 0, NULL);
    if (!rx_events || !pm_pio_read(pio, sm, &got) || got != 0x13579bdfu) {
        printf("FIFO failed: events=%d got=%08lx\n", rx_events, (unsigned long)got);
        goto finish;
    }
    puts("FIFO loopback PASS");
    tx = (uint32_t *)malloc(16 * sizeof(uint32_t));
    rx = (uint32_t *)malloc(16 * sizeof(uint32_t));
    if (!tx || !rx) { puts("malloc failed"); goto finish; }
    for (i = 0; i < 16; ++i) tx[i] = 0x07000000u + i;
    memset(rx, 0, 16 * sizeof(uint32_t));
    pm_pio_on_dma_done(0, on_dma);
    pm_pio_on_dma_done(1, on_dma);
    pm_pio_dma_rx(pio, sm, rx, 16 * sizeof(uint32_t), 16, 32);
    armed_rx = 1;
    pm_pio_dma_tx(pio, sm, tx, 16 * sizeof(uint32_t), 16, 32);
    armed_tx = 1;
    deadline = pm_timer_us() + 1000000u;
    while (pm_timer_us() < deadline && (!rx_dma_done || !tx_dma_done))
        pm_pio_poll();
    if (rx_dma_done && tx_dma_done && !memcmp(tx, rx, 16 * sizeof(uint32_t))) {
        puts("DMA loopback PASS");
        result = 0;
    } else
        printf("DMA failed: RX done=%d TX done=%d\n", rx_dma_done, tx_dma_done);
finish:
    if (armed_rx) pm_pio_dma_stop(0);
    if (armed_tx) pm_pio_dma_stop(1);
    pm_pio_on_dma_done(0, NULL);
    pm_pio_on_dma_done(1, NULL);
    pm_pio_stop(pio, sm);
    pm_pio_clear(pio);
    free(tx);
    free(rx);
    return result;
}
