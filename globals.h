#ifndef __GLOBALS_H
#define __GLOBALS_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

#include <setjmp.h>
#include "zcore.h"
#include "keys.h"

typedef struct _sysenv {
    void        *ui_env;        /* UI environment */
    z80env_t    *speccy_env;    /* ZX emulator environment */
    char        *homedir;
    u16         mode;
    u16         screen_id;
    void        *shadow_buffer;
    void        *status_background;
    char        *current_status;
    char        *savepath;
    u64         start_time;
    u64         status_time;
    u64         virt_time;
    u32         flags;
} sysenv_t;

#define INTRO_ERASE_GAME    0x00000001

#endif
