/* Native C services common to the RP2040 ELF runner and RP2350 TinyCC.
 * This module is linked with PicoMite itself, never with the compiler library.
 */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "core/pm_api.h"
#include "core/pm_runtime.h"
#include "core/tcc/include/picomite/input.h"
#include "pico/time.h"
#include <stdint.h>
#include <string.h>

/* RP2350 keeps the existing PM_CALL error/abort boundary in the C runtime.
 * RP2040's ELF runner uses the BASIC prompt's error boundary and performs
 * abandoned-image cleanup before resetting its heap. */
#ifdef rp2350
extern void tcc_picomite_api_call(void (*fn)(void *), void *arg);
#endif
void pm_core_call(void (*fn)(void *), void *arg)
{
    pm_runtime_tick_service();
#ifdef rp2350
    tcc_picomite_api_call(fn, arg);
#else
    if (MMAbort) CheckAbort();
    if (fn) fn(arg);
#endif
}

int pm_hres(void) { return HRes; }
int pm_vres(void) { return VRes; }
unsigned long long pm_timer_us(void) { return time_us_64(); }
int pm_abort_requested(void) { return MMAbort != 0; }

/* Keep these arguments on the caller's C stack. All callbacks are synchronous.
 * The trampoline adds no heap allocation or persistent callback. */
static void pm_cls_do(void *p) { ClearScreen(((int *)p)[0]); }
void pm_cls(int c) { int a[] = {c}; pm_core_call(pm_cls_do, a); }

static void pm_pixel_do(void *p) { int *a = p; DrawPixel(a[0], a[1], a[2]); }
void pm_pixel(int x, int y, int c)
{ int a[] = {x, y, c}; pm_core_call(pm_pixel_do, a); }

static void pm_line_do(void *p)
{ int *a = p; DrawLine(a[0], a[1], a[2], a[3], a[4], a[5]); }
void pm_core_line(int x1, int y1, int x2, int y2, int w, int c)
{ int a[] = {x1, y1, x2, y2, w, c}; pm_core_call(pm_line_do, a); }

static void pm_box_do(void *p)
{ int *a = p; DrawRectangle(a[0], a[1], a[2], a[3], a[4]); }
void pm_box(int x1, int y1, int x2, int y2, int c)
{ int a[] = {x1, y1, x2, y2, c}; pm_core_call(pm_box_do, a); }

static void pm_circle_do(void *p)
{ int *a = p; DrawCircle(a[0], a[1], a[2], a[3], a[4], a[5], 1.0); }
void pm_circle(int x, int y, int r, int w, int c, int fill)
{ int a[] = {x, y, r, w, c, fill}; pm_core_call(pm_circle_do, a); }

static void pm_triangle_do(void *p)
{ int *a = p; DrawTriangle(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); }
void pm_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int c, int fill)
{ int a[] = {x0, y0, x1, y1, x2, y2, c, fill}; pm_core_call(pm_triangle_do, a); }

static void pm_refresh_do(void *p) { (void)p; Display_Refresh(); }
void pm_refresh(void) { pm_core_call(pm_refresh_do, NULL); }

static void pm_inkey_do(void *p) { *(int *)p = MMInkey(); }
int pm_inkey(void)
{ int value = -1; pm_core_call(pm_inkey_do, &value); return value; }

static void pm_pin_mode_do(void *p)
{ int *a = p; ExtCfg(codemap(a[0]), a[1], 0); }
void pm_pin_mode(int gp, int mode)
{ int a[] = {gp, mode}; pm_core_call(pm_pin_mode_do, a); }

static void pm_pin_write_do(void *p)
{ int *a = p; ExtSet(codemap(a[0]), a[1]); }
void pm_pin_write(int gp, int value)
{ int a[] = {gp, value}; pm_core_call(pm_pin_write_do, a); }

static void pm_pin_read_do(void *p)
{ int *a = p; a[1] = (int)ExtInp(codemap(a[0])); }
int pm_pin_read(int gp)
{ int a[] = {gp, 0}; pm_core_call(pm_pin_read_do, a); return a[1]; }

extern void pm_gpio_subscribe_hw(int,int,int,void (*)(int,int));
static void pm_gpio_on_do(void *p)
{
    struct {int gp,edge,pull;void (*fn)(int,int);} *a=p;
    pm_gpio_subscribe_hw(a->gp,a->edge,a->pull,a->fn);
}
void pm_gpio_on_edge(int gp,int edge,int pull,void (*fn)(int,int))
{
    struct {int gp,edge,pull;void (*fn)(int,int);} a={gp,edge,pull,fn};
    pm_core_call(pm_gpio_on_do,&a);
}
void pm_gpio_poll(void) { pm_core_call(NULL,NULL); }

/* USB/BLE gamepads already decode into BASIC's nunstruct[1..4]. Reuse
 * nunfoundc[] without altering BASIC's x1 mask or nunInterruptc[] address.
 * Never pass a C function pointer to the BASIC dispatcher. */
typedef void (*pm_gamepad_callback)(int, unsigned);
static pm_gamepad_callback pm_c_gamepad_cb[5];
static unsigned pm_c_gamepad_owned;
static unsigned pm_c_gamepad_masks[5], pm_c_gamepad_last[5];
static bool pm_c_gamepad_polling;

static bool pm_gamepad_valid_type(unsigned type)
{
    return type == PM_GAMEPAD_PS4 || type == PM_GAMEPAD_PS3 ||
           type == PM_GAMEPAD_GENERIC || type == PM_GAMEPAD_XBOX ||
           type == PM_GAMEPAD_SWITCHPRO || type == PM_GAMEPAD_BLE;
}
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
extern bool bt_keyboard_has_gamepad(void);
#endif
static bool pm_gamepad_live_type(unsigned type)
{
    if (!pm_gamepad_valid_type(type)) return false;
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)
    if (type == PM_GAMEPAD_BLE && !bt_keyboard_has_gamepad()) return false;
#endif
    return true;
}

int pm_gamepad_c_owns(int channel)
{
    return channel >= 1 && channel <= 4 &&
           (pm_c_gamepad_owned & (1u << channel)) != 0;
}
/* Called only by the existing USB HID gamepad report decoders. BASIC
 * keeps its own x1 change mask; these are ORed for event detection. */
unsigned pm_gamepad_c_mask(int channel)
{
    return pm_gamepad_c_owns(channel) ? pm_c_gamepad_masks[channel] : 0;
}
void pm_gamepad_runtime_begin(void)
{
    memset(pm_c_gamepad_cb, 0, sizeof(pm_c_gamepad_cb));
    memset(pm_c_gamepad_masks, 0, sizeof(pm_c_gamepad_masks));
    memset(pm_c_gamepad_last, 0, sizeof(pm_c_gamepad_last));
    pm_c_gamepad_owned = 0;
    pm_c_gamepad_polling = false;
}
void pm_gamepad_runtime_end(void)
{
    for (int n = 1; n <= 4; ++n) {
        if (!pm_gamepad_c_owns(n)) continue;
        pm_c_gamepad_cb[n] = NULL;
        pm_c_gamepad_masks[n] = 0;
        nunfoundc[n] = false;
    }
    pm_c_gamepad_owned = 0;
    pm_c_gamepad_polling = false;
}
void pm_gamepad_runtime_poll(void)
{
    if (!pm_runtime_is_running() || pm_c_gamepad_polling ||
        !pm_c_gamepad_owned) return;
    pm_c_gamepad_polling = true;
    for (int n = 1; n <= 4; ++n) {
        if (!pm_gamepad_c_owns(n) || !nunfoundc[n]) continue;
        nunfoundc[n] = false;
        pm_gamepad_callback cb = pm_c_gamepad_cb[n];
        unsigned buttons = nunstruct[n].x0;
        unsigned change = (buttons ^ pm_c_gamepad_last[n]) & pm_c_gamepad_masks[n];
        pm_c_gamepad_last[n] = buttons;
        /* BLE also sets this BASIC latch for axis movement. Mouse and
         * other HID channels can set it too; only button changes on a
         * recognised gamepad are delivered to the C callback. */
        if (cb && change && pm_gamepad_live_type((unsigned)nunstruct[n].type))
            cb(n, buttons);
    }
    pm_c_gamepad_polling = false;
}

static void pm_gamepad_get_do(void *arg)
{
    struct {int channel, result; pm_gamepad_state *out;} *a = arg;
    pm_gamepad_state value = {0};
    unsigned want;
    int n = a->channel;
    if (n < 1 || n > 4) error("Invalid gamepad channel");
    if (!a->out || (want = a->out->size) < sizeof(unsigned)) {
        a->result = -1;
        return;
    }
    a->result = 0;
    /* Only gamepad types, not keyboards, mice or other HID functions. */
    unsigned type = (unsigned)nunstruct[n].type;
    if (pm_gamepad_live_type(type)) {
        a->result = 1;
        value.type = type;
        value.buttons = nunstruct[n].x0;
        value.hat = (uint8_t)nunstruct[n].y0;
        value.lx = nunstruct[n].ax;
        value.ly = nunstruct[n].ay;
        value.rx = nunstruct[n].Z;
        value.ry = nunstruct[n].C;
        value.l = nunstruct[n].L;
        value.r = nunstruct[n].R;
        for (int i = 0; i < 3; ++i) {
            value.gyro[i] = nunstruct[n].gyro[i];
            value.accel[i] = nunstruct[n].accs[i];
        }
#ifdef USBKEYBOARD
        if (HID[n - 1].active && HID[n - 1].Device_type == type) {
            value.vid = HID[n - 1].vid;
            value.pid = HID[n - 1].pid;
        }
#endif
    }
    /* Match pm_get_info: preserve the caller's size field and copy only
     * the portion understood by both sides of the ABI. */
    size_t copy = want < sizeof(value) ? want : sizeof(value);
    memcpy((unsigned char *)a->out + sizeof(unsigned),
           (const unsigned char *)&value + sizeof(unsigned),
           copy - sizeof(unsigned));
}
int pm_gamepad_get(int channel, pm_gamepad_state *out)
{
    struct {int channel, result; pm_gamepad_state *out;} a = {channel, -1, out};
    pm_core_call(pm_gamepad_get_do, &a);
    return a.result;
}

static void pm_gamepad_subscribe_do(void *arg)
{
    struct {int n; unsigned mask; pm_gamepad_callback cb;} *a = arg;
    int n = a->n;
    if (!pm_runtime_is_running()) error("Gamepad events require a running C program");
    if (n < 1 || n > 4) error("Invalid gamepad channel");
    if (!a->cb) {
        if (pm_gamepad_c_owns(n)) {
            pm_c_gamepad_cb[n] = NULL;
            pm_c_gamepad_owned &= ~(1u << n);
            pm_c_gamepad_masks[n] = 0;
            nunfoundc[n] = false;
        }
        return;
    }
    if (a->mask > 0xFFFFu || !a->mask) error("Invalid gamepad button mask");
#if !defined(USBKEYBOARD) && !defined(PICOMITEBTH) && !defined(PICOMITEHDMIBTH)
    error("Gamepad events unavailable in this firmware");
#elif !defined(USBKEYBOARD)
    if (n != 3) error("BLE gamepad uses channel 3");
#endif
#ifdef USBKEYBOARD
    if (HID[n - 1].active &&
        !pm_gamepad_valid_type(HID[n - 1].Device_type))
        error("HID channel is not a gamepad");
#endif
    if (!pm_gamepad_c_owns(n) && nunInterruptc[n])
        error("Gamepad interrupt already configured by BASIC");
    /* BASIC retains nunInterruptc[] and x1. The USB decoders OR this
     * C mask into their existing edge detection; BLE uses the same latch. */
    pm_c_gamepad_last[n] = nunstruct[n].x0;
    pm_c_gamepad_masks[n] = a->mask;
    pm_c_gamepad_cb[n] = a->cb;
    pm_c_gamepad_owned |= 1u << n;
    nunfoundc[n] = false;
}
void pm_gamepad_on_change(int channel, unsigned mask, pm_gamepad_callback cb)
{
    struct {int n; unsigned mask; pm_gamepad_callback cb;} a = {channel,mask,cb};
    pm_core_call(pm_gamepad_subscribe_do, &a);
}
void pm_gamepad_poll(void) { pm_core_call(NULL,NULL); }

/* Compiler-independent native functions; local declarations avoid clashing
 * with the legacy static TinyCC adapter implementations during migration. */
extern void pm_print(const char *);
extern void pm_yield(void);
extern void pm_delay_us(unsigned int);
extern void pm_pause(unsigned int);
extern int pm_keydown(int);
extern int pm_mouse(int, int);
extern int pm_usb_device(int);
extern int pm_errno(void);
extern const char *pm_errmsg(void);
extern void pm_error_clear(void);
extern int pm_get_framebuffer(void *);
extern unsigned pm_fb_colour(int);
extern int pm_font_width(int, int);
extern int pm_font_height(int, int);
extern void pm_text(int, int, const char *, const char *, int, int, int, int);
extern void pm_draw_box(int, int, int, int, int, int, int);
extern void pm_draw_rbox(int, int, int, int, int, int, int);
extern void pm_blit_framebuffer(int, int, int, int, int, int, int, int, int);
extern void pm_sprite_show(int, int, int, int, int);
extern void pm_sprite_hide(int);
extern void pm_sprite_write(int, int, int, int);
extern void pm_sprite_close_all(void);
extern void pm_framebuffer_copy(int, int, int);
extern void pm_pcm_play(const int16_t *, unsigned, unsigned, unsigned);
extern void pm_pcm_stop(void);
extern int pm_pcm_playing(void);
extern void pm_pcm_poll(void);
extern void pm_colour(int, int);
extern void pm_mode(int);
extern int pm_touch(const char *);
extern int pm_click(const char *);
extern void pm_gui(const char *);
extern int pm_ctrlval_get_number(int, double *);
extern int pm_ctrlval_set_number(int, double);
extern int pm_ctrlval_get_text(int, char *, unsigned);
extern int pm_ctrlval_set_text(int, const char *);
extern void pm_gui_on_event(void (*)(int, int, int, int));
extern void pm_gui_poll(void);

extern const void *const CallTable[];
typedef struct { const char *name; const void *address; } pm_core_symbol;
#define PM_CORE_SYM(n) {#n, (const void *)&n},
static const pm_core_symbol pm_core_symbols[] = {
    {"__picomite_calltable", (const void *)CallTable},
    PM_CORE_SYM(pm_hres)
    PM_CORE_SYM(pm_vres)
    PM_CORE_SYM(pm_timer_us)
    PM_CORE_SYM(pm_abort_requested)
    PM_CORE_SYM(pm_cls)
    PM_CORE_SYM(pm_pixel)
    {"pm_line", (const void *)&pm_core_line},
    PM_CORE_SYM(pm_box)
    PM_CORE_SYM(pm_circle)
    PM_CORE_SYM(pm_triangle)
    PM_CORE_SYM(pm_refresh)
    PM_CORE_SYM(pm_inkey)
    PM_CORE_SYM(pm_pin_mode)
    PM_CORE_SYM(pm_pin_write)
    PM_CORE_SYM(pm_pin_read)
    PM_CORE_SYM(pm_gpio_on_edge)
    PM_CORE_SYM(pm_gpio_poll)
    PM_CORE_SYM(pm_gamepad_get)
    PM_CORE_SYM(pm_gamepad_on_change)
    PM_CORE_SYM(pm_gamepad_poll)
    PM_CORE_SYM(pm_touch)
    PM_CORE_SYM(pm_click)
    PM_CORE_SYM(pm_gui)
    PM_CORE_SYM(pm_ctrlval_get_number)
    PM_CORE_SYM(pm_ctrlval_set_number)
    PM_CORE_SYM(pm_ctrlval_get_text)
    PM_CORE_SYM(pm_ctrlval_set_text)
    PM_CORE_SYM(pm_gui_on_event)
    PM_CORE_SYM(pm_gui_poll)
    PM_CORE_SYM(pm_print)
    PM_CORE_SYM(pm_yield)
    PM_CORE_SYM(pm_delay_us)
    PM_CORE_SYM(pm_pause)
    PM_CORE_SYM(pm_keydown)
    PM_CORE_SYM(pm_mouse)
    PM_CORE_SYM(pm_usb_device)
    PM_CORE_SYM(pm_errno)
    PM_CORE_SYM(pm_errmsg)
    PM_CORE_SYM(pm_error_clear)
    PM_CORE_SYM(pm_get_framebuffer)
    PM_CORE_SYM(pm_fb_colour)
    PM_CORE_SYM(pm_font_width)
    PM_CORE_SYM(pm_font_height)
    PM_CORE_SYM(pm_text)
    PM_CORE_SYM(pm_draw_box)
    PM_CORE_SYM(pm_draw_rbox)
    PM_CORE_SYM(pm_blit_framebuffer)
    PM_CORE_SYM(pm_sprite_show)
    PM_CORE_SYM(pm_sprite_hide)
    PM_CORE_SYM(pm_sprite_write)
    PM_CORE_SYM(pm_sprite_close_all)
    PM_CORE_SYM(pm_framebuffer_copy)
    PM_CORE_SYM(pm_pcm_play)
    PM_CORE_SYM(pm_pcm_stop)
    PM_CORE_SYM(pm_pcm_playing)
    PM_CORE_SYM(pm_pcm_poll)
    PM_CORE_SYM(pm_colour)
    PM_CORE_SYM(pm_mode)
};
#undef PM_CORE_SYM

const void *pm_core_resolve_symbol(const char *name)
{
    unsigned i;
    for (i = 0; i < sizeof(pm_core_symbols) / sizeof(pm_core_symbols[0]); ++i)
        if (strcmp(name, pm_core_symbols[i].name) == 0)
            return pm_core_symbols[i].address;
    return pm_runtime_resolve_symbol(name);
}

/* External ELF CSUBs are linked by absolute address. On RP2040 their
 * firmware stamp must cover this table, not just the legacy CallTable. */
unsigned pm_core_firmware_id(void)
{
    uint32_t hash = 2166136261u;
    unsigned i, j;
    for (i = 0; i < sizeof(pm_core_symbols) / sizeof(pm_core_symbols[0]); ++i) {
        uint32_t value = (uint32_t)(uintptr_t)pm_core_symbols[i].address;
        for (j = 0; j < 4; ++j, value >>= 8)
            hash = (hash ^ (value & 255u)) * 16777619u;
    }
    {
        uint32_t extra = pm_runtime_firmware_id();
        for (j = 0; j < 4; ++j, extra >>= 8)
            hash = (hash ^ (extra & 255u)) * 16777619u;
    }
    return hash ? hash : 1u;
}
