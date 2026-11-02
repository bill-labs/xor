#ifndef __SPECCY_H
#define __SPECCY_H

#include "globals.h"
#include "zcore.h"

typedef struct _s_rect {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} s_rect_t;

typedef struct _zxstate {
    z80cpu_t		cpustate;
	i32				work_time;
	i32				trap;
    u8              ram[0xC000];
} zxstate_t;

u32 speccy_screen(sysenv_t *senv, u8 *fb, u32 linesize, u16 *palette, s_rect_t *rect);
u32 speccy_title(sysenv_t *senv, u8 *fb, u32 linesize, u16 *palette, s_rect_t *rect);
void speccy_set_key_state(sysenv_t *senv, char key, int state);
void speccy_clear_input(sysenv_t *senv);
u32 speccy_init(sysenv_t *senv);
void speccy_frame(sysenv_t *senv);
u32 speccy_identify_screen(sysenv_t *senv);
void speccy_draw_text(sysenv_t *senv, u16 *fb, u32 linesize, char *s, u32 slen, u16 color);
u32 speccy_save_diff(sysenv_t *senv);
u32 speccy_load_diff(sysenv_t *senv);

#endif

