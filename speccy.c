#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <bzlib.h>

#include "globals.h"
#include "game.h"
#include "speccy.h"
#include "binaries.h"
#include "ui.h"
#include "istate.h"

#define ZVMENV (*env)

extern u8 rom48_data[16384];

static u32 speccy_screen1(sysenv_t *senv, u8 *fb, u8 *screen, u32 linesize, u16 *palette, s_rect_t *rect) {
    i32 i, j, k, l, m;
    u16 left, right, top, bottom;
    u8 a, b, sa, sb, paper, ink;
    u8 *pixels = screen;
    u8 *attrs = screen + 6144;
    u16 *dst1 = (u16 *)fb;
    u16 *dst2;
    u8 *shadow, *spixels, *sattrs;
    u32 rv = 0;
    u32 lpos, lupd;

    if (senv->shadow_buffer) {
        shadow = (u8 *)senv->shadow_buffer;
    } else {
        shadow = malloc(6912);
        if (!shadow) {
            fprintf(stderr, "FATAL: Out of memory allocating shadow buffer\n");
            return 0;
        }
        /* Make sure all pixels are inequal to the current screen */
        for (i = 0; i < 6912; i++)
            shadow[i] = ~screen[i];
        senv->shadow_buffer = shadow;
    }
    spixels = shadow;
    sattrs = shadow + 6144;
    right = 0;
    left = 32;
    top = 192;
    bottom = 0;
    lpos = 0;
    for (i = 0; i < 3; i++) {
    	for (j = 0; j < 8; j++) {
    	    for (k = 0; k < 8; k++) {
                lupd = 0;
                dst2 = (u16 *)((u8 *)dst1 + linesize);
        		for (l = 0; l < 32; l++) {
        		    b = *pixels;
        		    a = *attrs;
                    sb = *spixels;
                    sa = *sattrs;
                    if ((b != sb) || (a != sa)) {
                        *spixels = b;
                        if (l < left)
                            left = l;
                        if (l > right)
                            right = l;
                        lupd = 1;
                        if (a & 0x80) {
                            ink = (a >> 3) & 7;
                            paper = a & 7;
                            if (a & 0x40)
                                paper |= 8;
                        } else {
                            paper = (a >> 3) & 7;
                            ink = a & 7;
                            if (a & 0x40)
                                ink |= 8;
                        }
                        for (m = 128; m; m >>= 1) {
                            dst1[0] = dst1[1] = dst2[0] = dst2[1] = palette[(b & m) ? ink : paper];
                            dst1 += 2;
                            dst2 += 2;
                        }
                    } else {
                        dst1 += 16;
                        dst2 += 16;
                    }
                    pixels++;
                    attrs++;
                    spixels++;
                    sattrs++;
		        }

                rv |= lupd;
                if (lupd) {
                    if (lpos < top)
                        top = lpos;
                    if (lpos > bottom)
                        bottom = lpos;
                }
                lpos++;
        		pixels += 224;
        		attrs -= 32;
                spixels += 224;
        		sattrs -= 32;
                dst1 -= 512;
                dst1 = (u16 *)((u8 *)dst1 + (linesize << 1));
    	    }
    	    attrs += 32;
    	    pixels -= 2016;
            sattrs += 32;
    	    spixels -= 2016;
    	}
    	pixels += 2048 - 256;
        spixels += 2048 - 256;
    }

    if (rv) {
        memcpy(shadow + 6144, screen + 6144, 768);
        rect->x = left << 4;
        rect->y = top << 1;
        rect->w = (right + 1 - left) << 4;
        rect->h = (bottom + 1 - top) << 1;
    }
    
    return rv;
}

u32 speccy_screen(sysenv_t *senv, u8 *fb, u32 linesize, u16 *palette, s_rect_t *rect) {
    z80env_t *env = senv->speccy_env;
    return speccy_screen1(senv, fb, env->ram, linesize, palette, rect);
}

u32 speccy_title(sysenv_t *senv, u8 *fb, u32 linesize, u16 *palette, s_rect_t *rect) {
    return speccy_screen1(senv, fb, title_screen, linesize, palette, rect);
}

static void press_key(z80env_t *env, int row, int key) {
    env->io[row] &= ~(1 << key);
    env->io[row + 8] &= ~(1 << key);
}

static void release_key(z80env_t *env, int row, int key) {
    // env->io[row] |= (1 << key);
    env->io[row + 8] |= (1 << key);
}

static void set_raw_key_state(z80env_t *env, int row, int key, int state) {
    if (state)
	    press_key(env, row, key);
    else
	    release_key(env, row, key);
}

void speccy_set_key_state(sysenv_t *senv, char key, int state) {
    z80env_t *env = senv->speccy_env;
    set_raw_key_state(env, key >> 4, key & 0x0F, state);
}

void speccy_clear_input(sysenv_t *senv) {
    z80env_t *env = senv->speccy_env;
    u32 i;
    for (i = 0; i < 16; i++)
        env->io[i] = 0xFF;
}

static u8 *linestart(z80env_t *env, u32 line) {
    u32 pos;

    pos = (line / 64) * 2048;
    line %= 64;
    pos += (line / 8) * 32 + (line % 8) * 256;
    return (u8 *)env->ram + pos;
}

static int speccy_read_screen_char(sysenv_t *senv, u32 row, u32 col, u32 game) {
    u8 *cptr, *cptr1;
    u32 ch;
    u8 *sptr, *sptr1;
    u32 i;
    z80env_t *env = senv->speccy_env;

    if (game) {
        cptr = cptr1 = (u8 *)env->ram + 0xE100 - 0x4000;
    } else {
        u32 addr = *(u16 *)((u8 *)env->ram + (23606 - 16384)) + 256;
        if (addr < 0x4000)
            cptr = cptr1 = (u8 *)env->rom + addr;
        else
            cptr = cptr1 = (u8 *)env->ram + addr - 0x4000;
    }
    sptr = sptr1 = linestart(env, row * 8) + col;
    ch = 32;
    i = 0;
    while (i < 8) {
    	if (*sptr == *cptr) {
    	    i++;
    	    sptr += 256;
    	    cptr++;
    	} else {
    	    i = 0;
    	    ch++;
    	    if (ch > 128)
    		      return 0;
    	    sptr = sptr1;
    	    cptr = cptr1 + (ch - 32) * 8;
    	}
    }
    return ch;
}

static char *speccy_read_screen_string(sysenv_t *senv, char *s, u32 row, u32 col, u32 len, u32 game) {
    int i;
    for (i = 0; i < len; i++)
	   s[i] = speccy_read_screen_char(senv, row, col + i, game);
    s[i] = '\0';
    return s;
}

static void speccy_draw_char(sysenv_t *senv, u16 *fb, u32 linesize, char c, u16 color) {
    u8 *cptr;
    u32 i;
    u8 b, m;
    z80env_t *env = senv->speccy_env;

    cptr = env->ram + 0xE100 - 0x4000 + ((u32)(u8)(c - 32) << 3);
    for (i = 0; i < 8; i++) {
        b = *cptr++;
        for (m = 0x80; m; m >>= 1) {
            if (b & m)
                *fb = color;
            fb++;
        }
        fb -= 8;
        fb = (u16 *)((u8 *)fb + linesize);
    }
}

void speccy_draw_text(sysenv_t *senv, u16 *fb, u32 linesize, char *s, u32 slen, u16 color) {
    while (slen > 0) {
        speccy_draw_char(senv, fb, linesize, *s, color);
        fb += 8;
        s++;
        slen--;
    }
}

u32 speccy_identify_screen(sysenv_t *senv) {
    char s[32];

    if (!strcmp(speccy_read_screen_string(senv, s, 7, 5, 14, 1), "Dots and Waves"))
        return SCREEN_MENU;
    else if (!strcmp(speccy_read_screen_string(senv, s, 7, 2, 22, 1), "XOR must be revealed !"))
    	return SCREEN_INTRO;
    else if (!strcmp(speccy_read_screen_string(senv, s, 20, 8, 16, 1), "Number to Select") &&
    	    !strcmp(speccy_read_screen_string(senv, s, 21, 8, 16, 1), "Enter to Confirm"))
    	        return SCREEN_SELECT;
    else
        return SCREEN_OTHER;
}

i32 __attribute__((regparm(3))) zcore_run(z80env_t *env);
/* Main emulation frame */
void speccy_frame(sysenv_t *senv) {
    z80env_t *env = senv->speccy_env;
    i32 trap;

    if (env->work_time <= 0) {
        env->work_time = 71680;
        SF |= SYSFLAG_INTERRUPTED;
    }

    while (env->work_time > 0) {
        trap = zcore_run(env);
        switch (trap) {

            case -2: {  /* Pending EI */
                while (SF & SYSFLAG_PENDING_EI) {
                    i32 swt = env->work_time;
                    env->work_time = 1;     /* Guaranteed to execute only one inxtruction */
                    SF &= ~SYSFLAG_PENDING_EI;
                    zcore_run(env);
                    env->work_time += swt - 1;
                    if (env->work_time >= 71600) {
                        SF |= SYSFLAG_INTERRUPTED;
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void scramble(u8 *p, u32 len) {
    u32 i, j;
    u8 b1, b2;
    for (i = 0; i < len; i++) {
        b1 = p[i];
        b2 = 0;
        for (j = 0; j < 8; j++) {
            b2 >>= 1;
            if (b1 & 0x80)
                b2 |= 0x80;
            b1 <<= 1;
        }
        p[i] = ~b2;
    }
}

u32 speccy_save_diff(sysenv_t *senv) {
    zxstate_t *izxst, *czxst;
    u8 *buf;
    u8 *p1, *p2;
    z80env_t *env = senv->speccy_env;
    unsigned len;
    char *tpath;
    u32 i;
    int fd;
    u32 rv = 0;
    
    izxst = malloc(sizeof(zxstate_t) << 2);
    if (!izxst)
        return 0;
    
    czxst = izxst + 1;
    buf = (u8 *)(izxst + 2);
    
    len = sizeof(zxstate_t);
    if (BZ2_bzBuffToBuffDecompress((char *)izxst, &len,
        (char *)initial_state, sizeof(initial_state), 0, 0) != BZ_OK)
            goto fail;
    if (len != sizeof(zxstate_t))
        goto fail;

    memcpy(&czxst->cpustate, &env->cpustate, sizeof(z80cpu_t));
    czxst->work_time = env->work_time;
    czxst->trap = env->trap;
    memcpy(czxst->ram, env->ram, 0xC000);

    p1 = (u8 *)izxst;
    p2 = (u8 *)czxst;
    for (i = 0; i < sizeof(zxstate_t); i++)
        p2[i] ^= p1[i];
    
    len = sizeof(zxstate_t) << 1;
    if (BZ2_bzBuffToBuffCompress((char *)buf, &len,
        (char *)p2, sizeof(zxstate_t), 9, 0, 0) != BZ_OK)
            goto fail;

    tpath = path_append(senv->savepath, "tmp");
    if (!tpath)
        goto fail;

    scramble(buf, len);
    if ((fd = open(tpath, O_WRONLY | O_CREAT | O_TRUNC, 0644)) >= 0) {
        if (write(fd, buf, len) == len)
            rv++;
        close(fd);
    }

    if (rv) {
        if (unlink(senv->savepath) < 0) {
            if (errno != ENOENT)
                rv = 0;
        }
    }
    
    if (rv) {
        if (rename(tpath, senv->savepath) < 0)
            rv = 0;
    }

    free(tpath);
fail:
    free(izxst);
    return rv;
}

u32 speccy_load_diff(sysenv_t *senv) {
    zxstate_t *izxst, *czxst;
    u8 *buf;
    u8 *p1, *p2;
    z80env_t *env = senv->speccy_env;
    unsigned len;
    u32 i;
    int fd = -1;
    int bzrv;
    u32 rv = 0;
    size_t max_len = sizeof(zxstate_t) << 1;

    izxst = malloc(sizeof(zxstate_t) << 2);
    if (!izxst)
        return 0;
    
    czxst = izxst + 1;
    buf = (u8 *)(izxst + 2);

    if ((fd = open(senv->savepath, O_RDONLY)) >= 0) {
        off_t flen;
        ssize_t rlen;
        if ((flen = lseek(fd, 0, SEEK_END)) == (off_t)-1)
            goto fail;
        if (flen <= 0 || flen > (off_t)max_len)
            goto fail;
        if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
            goto fail;
        rlen = read(fd, buf, (size_t)flen);
        if (rlen != flen)
            goto fail;
        close(fd);
        fd = -1;

        scramble(buf, (u32)flen);
        len = sizeof(zxstate_t);
        if (BZ2_bzBuffToBuffDecompress((char *)czxst, &len,
            (char *)buf, flen, 0, 0) != BZ_OK)
                goto fail;
        if (len != sizeof(zxstate_t))
            goto fail;
    } else 
        goto fail;

    len = sizeof(zxstate_t);
    bzrv = BZ2_bzBuffToBuffDecompress((char *)izxst, &len,
        (char *)initial_state, sizeof(initial_state), 0, 0);
    if ((bzrv == BZ_DATA_ERROR_MAGIC) || (bzrv == BZ_UNEXPECTED_EOF) || (bzrv == BZ_DATA_ERROR))
        goto corrupted;
    if (bzrv != BZ_OK)
        goto fail;
    if (len != sizeof(zxstate_t))
        goto corrupted;

    p1 = (u8 *)izxst;
    p2 = (u8 *)czxst;
    for (i = 0; i < sizeof(zxstate_t); i++)
        p2[i] ^= p1[i];

    memcpy(&env->cpustate, &czxst->cpustate, sizeof(z80cpu_t));
    env->work_time = czxst->work_time;
    env->trap = czxst->trap;
    memcpy(env->ram, czxst->ram, 0xC000);
    rv++;

fail:
    if (fd >= 0)
        close(fd);
    free(izxst);
    return rv;
corrupted:
    unlink(senv->savepath);
    goto fail;
}

u32 speccy_init(sysenv_t *senv) {
    z80env_t *env = malloc(sizeof(z80env_t));
    zxstate_t *zxst;
    unsigned len;
    char str[32];

    if (!env)
        return 0;

    env->ram = malloc(0xC000);
    if (!env->ram)
        goto fail;

    zxst = malloc(sizeof(zxstate_t));
    if (!zxst)
        goto fail1;

    len = sizeof(zxstate_t);
    if (BZ2_bzBuffToBuffDecompress((char *)zxst, &len,
        (char *)initial_state, sizeof(initial_state), 0, 0) != BZ_OK)
            goto fail2;
    if (len != sizeof(zxstate_t))
        goto fail2;


    memcpy(&env->cpustate, &zxst->cpustate, sizeof(z80cpu_t));
    env->work_time = zxst->work_time;
    env->trap = zxst->trap;
    memcpy(env->ram, zxst->ram, 0xC000);
    free(zxst);

    env->rom = rom48_data;
    senv->speccy_env = env;
    speccy_clear_input(senv);

    /* Wait for game's title screen to appear */
    do {
        speccy_frame(senv);
        speccy_read_screen_string(senv, str, 7, 2, 22, 1);
    } while (strcmp(str, "XOR must be revealed !"));

    return 1;

fail2:
    free(zxst);
fail1:
    free(env->ram);
fail:
    free(env);
    return 0;
}
