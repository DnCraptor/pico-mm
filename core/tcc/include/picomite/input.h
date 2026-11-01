#ifndef _PICOMITE_INPUT_H
#define _PICOMITE_INPUT_H

#ifndef PM_DEFAULT
#define PM_DEFAULT (-2)
#endif

int pm_inkey(void);

int pm_keydown(int n);

int pm_mouse(int channel, int what);

int pm_usb_device(int port);

#include <stdint.h>
#define PM_GAMEPAD_PS4       128u
#define PM_GAMEPAD_PS3       129u
#define PM_GAMEPAD_GENERIC   130u
#define PM_GAMEPAD_XBOX      131u
#define PM_GAMEPAD_SWITCHPRO 135u
#define PM_GAMEPAD_BLE       0x4254u

typedef struct pm_gamepad_state {
    unsigned size;
    unsigned type;
    unsigned vid, pid;
    uint16_t buttons;
    uint8_t hat;
    uint8_t reserved;
    int lx, ly, rx, ry, l, r;
    int16_t gyro[3], accel[3];
} pm_gamepad_state;

int pm_gamepad_get(int channel, pm_gamepad_state *out);
void pm_gamepad_on_change(int channel, unsigned mask,
                          void (*callback)(int channel, unsigned buttons));
void pm_gamepad_poll(void);

#define PM_GPIO_RISE 1
#define PM_GPIO_FALL 2
#define PM_GPIO_BOTH 3
#define PM_GPIO_NOPULL 0
#define PM_GPIO_PULLUP 1
#define PM_GPIO_PULLDOWN 2
void pm_gpio_on_edge(int,int,int,void (*)(int,int));
void pm_gpio_poll(void);

void pm_cursor_on(int no, int x, int y, int colour);
void pm_cursor_off(void);
void pm_cursor_hide(void);
void pm_cursor_show(void);
void pm_cursor_colour(int colour);
void pm_cursor_link(int link);
void pm_cursor_load(const char *file);
void pm_cursor_move(int x, int y);

#include "gui.h"

#endif
