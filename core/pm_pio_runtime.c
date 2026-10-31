/* C entry points for the EXISTING BASIC PIO hardware implementation.
 * All underlying operations are in misc/Custom.c. No second PIO driver. */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include "core/pm_pio_shared.h"
#include "core/tcc/include/picomite/pio.h"

/* Only resources started by this RUN may be stopped during teardown. */
static unsigned pm_c_dma_owned;
static const void *pm_c_dma_buffer[2];
static unsigned pm_c_sm_owned[3];
void pm_pio_runtime_begin(void)
{
    pm_c_dma_owned = 0;
    pm_c_dma_buffer[0] = pm_c_dma_buffer[1] = NULL;
    pm_c_sm_owned[0] = pm_c_sm_owned[1] = pm_c_sm_owned[2] = 0;
}
void pm_pio_runtime_end(void)
{
    int pio, sm;
    if (pm_c_dma_owned & 1u) pm_pio_dma_stop_hw(0);
    if (pm_c_dma_owned & 2u) pm_pio_dma_stop_hw(1);
    for (pio = 0; pio < 3; ++pio)
        for (sm = 0; sm < 4; ++sm)
            if (pm_c_sm_owned[pio] & (1u << sm))
                if (pm_pio_available_hw(pio)) pm_pio_stop_hw(pio, sm);
    pm_pio_runtime_begin();
}
void pm_pio_runtime_before_free(const void *allocation, size_t size)
{
    uintptr_t start = (uintptr_t)allocation;
    unsigned tx;
    if (!allocation) return;
    for (tx = 0; tx < 2; ++tx) {
        uintptr_t p = (uintptr_t)pm_c_dma_buffer[tx];
        if ((pm_c_dma_owned & (1u << tx)) &&
            p >= start && p - start < size) {
            pm_pio_dma_stop_hw((int)tx);
            pm_c_dma_owned &= ~(1u << tx);
            pm_c_dma_buffer[tx] = NULL;
        }
    }
}
static void pm_pio_require_run(void)
{
    if (!pm_runtime_is_running()) error("PIO requires a running C program");
}
static void pm_pio_exec_do(void *p)
{ int *a = p; pm_pio_require_run(); pm_pio_exec_hw(a[0], a[1], (uint16_t)a[2]); }
void pm_pio_exec(int pio, int sm, uint16_t code)
{ int a[] = {pio, sm, code}; pm_core_call(pm_pio_exec_do, a); }
static void pm_pio_write_do(void *p)
{ struct {int pio, sm; uint32_t word;} *a=p;
  pm_pio_require_run(); pm_pio_write_hw(a->pio, a->sm, a->word); }
void pm_pio_write(int pio, int sm, uint32_t word)
{ struct {int pio,sm;uint32_t word;} a={pio,sm,word}; pm_core_call(pm_pio_write_do,&a); }
static void pm_pio_read_do(void *p)
{ struct {int pio,sm; uint32_t word; int got;} *a=p;
  pm_pio_require_run(); a->got=pm_pio_read_hw(a->pio,a->sm,&a->word); }
int pm_pio_read(int pio, int sm, uint32_t *out)
{
    struct {int pio,sm;uint32_t word;int got;} a={pio,sm,0,0};
    if (!out) return 0;
    pm_core_call(pm_pio_read_do, &a);
    if (a.got) *out = a.word;
    return a.got;
}
static void pm_pio_sm_do(void *p)
{
    int *a = p;
    pm_pio_require_run();
    if (a[0] < 0 || a[0] >= 3 || a[1] < 0 || a[1] >= 4)
        error("Invalid PIO state machine");
    if (a[2]) {
        pm_pio_start_hw(a[0],a[1]);
        pm_c_sm_owned[a[0]] |= 1u << a[1];
    } else {
        pm_pio_stop_hw(a[0],a[1]);
        pm_c_sm_owned[a[0]] &= ~(1u << a[1]);
    }
}
void pm_pio_start(int pio, int sm)
{ int a[]={pio,sm,1}; pm_core_call(pm_pio_sm_do,a); }
void pm_pio_stop(int pio, int sm)
{ int a[]={pio,sm,0}; pm_core_call(pm_pio_sm_do,a); }
static void pm_pio_init_do(void *p)
{ struct {int pio,sm;uint32_t pinctrl,execctrl,shiftctrl;
          int start;float clock;int sideout,setout,outout;} *a=p;
  pm_pio_require_run();
  pm_pio_init_hw(a->pio,a->sm,a->pinctrl,a->execctrl,a->shiftctrl,
                 a->start,a->clock,a->sideout,a->setout,a->outout); }
void pm_pio_init_machine(int pio,int sm,uint32_t pinctrl,uint32_t execctrl,
                          uint32_t shiftctrl,int start,float clock,
                          int sideout,int setout,int outout)
{
    struct {int pio,sm;uint32_t pinctrl,execctrl,shiftctrl;
            int start;float clock;int sideout,setout,outout;} a=
        {pio,sm,pinctrl,execctrl,shiftctrl,start,clock,sideout,setout,outout};
    pm_core_call(pm_pio_init_do,&a);
}
static void pm_pio_available_do(void *p)
{ int *a=p; a[1]=pm_pio_available_hw(a[0]); }
int pm_pio_available(int pio)
{ int a[]={pio,0};pm_core_call(pm_pio_available_do,a);return a[1]; }

typedef struct {
    int tx,pio,sm;
    void *ptr;
    size_t capacity;
    uint32_t count;
    unsigned bits;
} pm_pio_dma_args;
static void pm_pio_dma_do(void *arg)
{
    pm_pio_dma_args *a=arg;
    unsigned mask=1u << a->tx;
    pm_pio_require_run();
    if (a->pio < 0 || a->pio >= 3 || a->sm < 0 || a->sm >= 4)
        error("Invalid PIO state machine");
    if (!a->count || !a->capacity ||
        !pm_runtime_owns_range(a->ptr,a->capacity))
        error("PIO DMA requires a live C allocation");
    /* Set ownership BEFORE touching hardware: errors must still clean up. */
    if (pm_pio_dma_busy_hw(a->tx))
        error("PIO DMA channel in use");
    pm_c_dma_owned |= mask;
    pm_c_dma_buffer[a->tx] = a->ptr;
    pm_pio_dma_transfer_hw(a->tx,a->pio,a->sm,a->ptr,a->capacity,
                           a->count,a->bits,1);
    pm_c_sm_owned[a->pio] |= 1u << a->sm;
}
void pm_pio_dma_rx(int pio,int sm,void *buf,size_t capacity,uint32_t count,unsigned bits)
{ pm_pio_dma_args a={0,pio,sm,buf,capacity,count,bits};pm_core_call(pm_pio_dma_do,&a); }
void pm_pio_dma_tx(int pio,int sm,const void *buf,size_t capacity,uint32_t count,unsigned bits)
{ pm_pio_dma_args a={1,pio,sm,(void *)buf,capacity,count,bits};
  pm_core_call(pm_pio_dma_do,&a); }
static void pm_pio_dma_busy_do(void *p)
{ int *a=p; pm_pio_require_run();a[1]=pm_pio_dma_busy_hw(a[0]); }
int pm_pio_dma_busy(int tx)
{ int a[]={tx,0};pm_core_call(pm_pio_dma_busy_do,a);return a[1]; }
static void pm_pio_dma_stop_do(void *p)
{ int tx=*(int *)p;
  pm_pio_require_run();
  if (tx < 0 || tx > 1 || !(pm_c_dma_owned & (1u << tx)))
      error("PIO DMA not owned by C program");
  pm_pio_dma_stop_hw(tx);
  pm_c_dma_owned &= ~(1u << tx);
  pm_c_dma_buffer[tx] = NULL; }
void pm_pio_dma_stop(int tx)
{ pm_core_call(pm_pio_dma_stop_do,&tx); }

#include <string.h>
typedef struct {const char *name;const void *address;} pm_pio_symbol;
#define PM_PIO_SYM(n) {#n,(const void *)&n},
static const pm_pio_symbol pm_pio_syms[]={
    PM_PIO_SYM(pm_pio_available)
    PM_PIO_SYM(pm_pio_exec)
    PM_PIO_SYM(pm_pio_write)
    PM_PIO_SYM(pm_pio_read)
    PM_PIO_SYM(pm_pio_start)
    PM_PIO_SYM(pm_pio_stop)
    PM_PIO_SYM(pm_pio_init_machine)
    PM_PIO_SYM(pm_pio_dma_rx)
    PM_PIO_SYM(pm_pio_dma_tx)
    PM_PIO_SYM(pm_pio_dma_busy)
    PM_PIO_SYM(pm_pio_dma_stop)
};
const void *pm_pio_resolve_symbol(const char *name)
{
    unsigned i;
    for (i=0;i<sizeof(pm_pio_syms)/sizeof(pm_pio_syms[0]);i++)
        if (!strcmp(pm_pio_syms[i].name,name)) return pm_pio_syms[i].address;
    return NULL;
}
