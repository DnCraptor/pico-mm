/* Hardware implementation shared by PicoMite BASIC PIO commands and C. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_pio_shared.h"
#include "hardware/dma.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/pio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
extern bool PIO0, PIO1;
#ifdef rp2350
extern bool PIO2;
#endif
extern char *DMAinterruptRX, *DMAinterruptTX;
extern uint32_t dma_rx_chan, dma_tx_chan, dma_rx_chan2, dma_tx_chan2;
extern int dma_rx_pio, dma_tx_pio, dma_rx_sm, dma_tx_sm;
extern void startPIO(PIO pio, int sm);
extern void pio_init(int pior, int sm, uint32_t pinctrl, uint32_t execctrl,
                     uint32_t shiftctrl, int start, float clock,
                     bool sideout, bool setout, bool outout);
#define CLKMIN ((Option.CPU_Speed * 125) >> 13)
#define CLKMAX (Option.CPU_Speed * 1000)

/* PIO hardware primitives are shared by BASIC's cmd_pio and native C.
 * Keep the resource restrictions exactly where BASIC enforces them. */
static PIO pm_pio_checked(int pior)
{
    if (pior < 0 || pior >= PIOMAX) error("Invalid PIO");
    if (pior == 0 && !PIO0) StandardError(3);
    if (pior == 1 && !PIO1) StandardError(4);
#ifdef rp2350
    if (pior == 2 && !PIO2) StandardError(5);
    return pior == 0 ? pio0 : (pior == 1 ? pio1 : pio2);
#else
    return pior == 0 ? pio0 : pio1;
#endif
}
static PIO pm_pio_sm_checked(int pior,int sm)
{
    if (sm < 0 || sm >= 4) error("Invalid PIO state machine");
    return pm_pio_checked(pior);
}
int pm_pio_available_hw(int pior)
{
    if (pior == 0) return PIO0;
    if (pior == 1) return PIO1;
#ifdef rp2350
    if (pior == 2) return PIO2;
#endif
    return 0;
}
void pm_pio_exec_hw(int pior,int sm,uint16_t instr)
{ pio_sm_exec(pm_pio_sm_checked(pior,sm),sm,instr); }
void pm_pio_write_hw(int pior,int sm,uint32_t value)
{ pio_sm_put_blocking(pm_pio_sm_checked(pior,sm),sm,value); }
int pm_pio_read_hw(int pior,int sm,uint32_t *value)
{
    PIO pio=pm_pio_sm_checked(pior,sm);
    uint32_t under=1u << (PIO_FDEBUG_RXUNDER_LSB+sm);
    if (!value) error("Invalid PIO buffer");
    pio->fdebug=under;
    *value=pio_sm_get(pio,sm);
    if (pio->fdebug & under) { pio->fdebug=under; return 0; }
    return 1;
}
void pm_pio_start_hw(int pior,int sm)
{ startPIO(pm_pio_sm_checked(pior,sm),sm); }
void pm_pio_stop_hw(int pior,int sm)
{ pio_sm_set_enabled(pm_pio_sm_checked(pior,sm),sm,false); }
void pm_pio_init_hw(int pior,int sm,uint32_t pinctrl,uint32_t execctrl,
                    uint32_t shiftctrl,int start,float clock,
                    int sideout,int setout,int outout)
{
    (void)pm_pio_sm_checked(pior,sm);
    if (clock < CLKMIN || clock > CLKMAX) error("Invalid PIO clock");
    if (start < 0 || start > 31) error("Invalid PIO start");
    pio_init(pior,sm,pinctrl,execctrl,shiftctrl,start,clock,
             sideout!=0,setout!=0,outout!=0);
}

void pm_pio_dma_stop_hw(int tx)
{
    unsigned ch,c2;
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    ch=tx ? dma_tx_chan : dma_rx_chan;
    c2=tx ? dma_tx_chan2 : dma_rx_chan2;
    dma_hw->abort=(1u<<ch)|(1u<<c2);
    if (dma_channel_is_busy(ch)) dma_channel_abort(ch);
    if (dma_channel_is_busy(c2)) dma_channel_abort(c2);
}
int pm_pio_dma_busy_hw(int tx)
{
    if (tx != 0 && tx != 1) error("Invalid PIO DMA direction");
    return dma_channel_is_busy(tx ? dma_tx_chan : dma_rx_chan) ||
           dma_channel_is_busy(tx ? dma_tx_chan2 : dma_rx_chan2);
}
void pm_pio_dma_transfer_hw(int tx,int pior,int sm,void *buffer,
                            size_t capacity,uint32_t count,unsigned bits,int from_c)
{
    PIO pio=pm_pio_sm_checked(pior,sm);
    unsigned chan=tx ? dma_tx_chan : dma_rx_chan;
    unsigned shift=bits==8?0:bits==16?1:bits==32?2:3;
    dma_channel_config cfg;
    if (!buffer || shift==3 || !count ||
        count > capacity / (1u<<shift))
        error("Invalid PIO DMA buffer or transfer size");
    if ((uintptr_t)buffer & ((1u<<shift)-1u))
        error("PIO DMA buffer alignment error");
    pm_pio_dma_stop_hw(tx);
    if (from_c) {
        if (tx) DMAinterruptTX=NULL;
        else DMAinterruptRX=NULL;
    }
    if (tx) { dma_tx_pio=pior;dma_tx_sm=sm; }
    else { dma_rx_pio=pior;dma_rx_sm=sm; }
    cfg=dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&cfg,(enum dma_channel_transfer_size)shift);
    channel_config_set_dreq(&cfg,pio_get_dreq(pio,sm,tx!=0));
    channel_config_set_read_increment(&cfg,tx!=0);
    channel_config_set_write_increment(&cfg,tx==0);
    dma_channel_configure(chan,&cfg,
        tx ? (volatile void *)&pio->txf[sm] : buffer,
        tx ? buffer : (const volatile void *)&pio->rxf[sm],
        count,true);
    pio_sm_restart(pio,sm);
    pio_sm_set_enabled(pio,sm,true);
}
