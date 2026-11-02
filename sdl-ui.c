#include <stdio.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <setjmp.h>
#include <bzlib.h>

#include "globals.h"
#include "game.h"
#include "speccy.h"
#include "frame.h"

#define TOP_HEIGHT      123
#define LEFT_WIDTH      175
#define RIGHT_WIDTH     195
#define BOTTOM_HEIGHT   85
#define WINDOW_WIDTH    (512 + RIGHT_WIDTH + LEFT_WIDTH)
#define WINDOW_HEIGHT   (384 + TOP_HEIGHT + BOTTOM_HEIGHT)
#define STATUS_X        183
#define STATUS_Y        538
#define STATUS_W        500
#define STATUS_H        28

typedef struct _sdl_ui {
    SDL_Surface *surf;
    u32         prev_ticks;
    u32         time_overflow;
    u16         palette[16];
    jmp_buf     jbuf;
    u32         stick_times[4];
    u8          stick_state[4];
} sdl_ui_t;

#define toup(x)  (((x) >= 'a' && (x) <= 'z') ? ((x) - ('a' - 'A')) : (x))

/* Keyboard mapping tables */
static u8 speccy_keys[] = { KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
		    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
		    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
		    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
		    KEY_SHIFT, KEY_ALT, KEY_SPACE, KEY_ENTER, KEY_F2, KEY_F3 };

static SDLKey sdl_keys[] = { SDLK_a,SDLK_b,SDLK_c,SDLK_d,SDLK_e,SDLK_f,SDLK_g,SDLK_h,SDLK_i,
		     SDLK_j,SDLK_k,SDLK_l,SDLK_m,SDLK_n,SDLK_o,SDLK_p,SDLK_q,SDLK_r,
		     SDLK_s,SDLK_t,SDLK_u,SDLK_v,SDLK_w,SDLK_x,SDLK_y,SDLK_z,
		     SDLK_0,SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,
		     SDLK_LSHIFT, SDLK_LALT, SDLK_SPACE, SDLK_RETURN, SDLK_F2, SDLK_F3 };

static u8 stick_keys[4] = { KEY_P, KEY_L, KEY_Z, KEY_X };

static void cexpand(u8 n, u16 *p, SDL_PixelFormat *pf) {
    u8 cc = (n & 0x08) ? 0xFF : 0xBB;
    u16 b = (n & 0x01) ? cc : 0x00;
    u16 r = (n & 0x02) ? cc : 0x00;
    u16 g = (n & 0x04) ? cc : 0x00;
    *p = ((r >> pf->Rloss) << pf->Rshift) |
        ((g >> pf->Gloss) << pf->Gshift) |
        ((b >> pf->Bloss) << pf->Bshift);
}

static void reset_palette(sdl_ui_t *ui) {   
    /* Initial palette setting */
    u32 i;
    for (i = 0; i < 16; i++)
        cexpand(i & 0x0F, &ui->palette[i], ui->surf->format);
}

static void draw_rgb_line(u8 *src, u16 *dst, SDL_PixelFormat *pf, u32 cnt) {
    while (cnt > 0) {
        u16 r = src[0];
        u16 g = src[1];
        u16 b = src[2];
        *dst++ = ((r >> pf->Rloss) << pf->Rshift) |
            ((g >> pf->Gloss) << pf->Gshift) |
            ((b >> pf->Bloss) << pf->Bshift);
        src += 3;
        cnt--;
    }
}

static void draw_rgb_rect(u8 *src, u16 *dst, SDL_PixelFormat *pf, u32 w, u32 h, u32 linesize) {
    u32 cnt = h;
    while (cnt > 0) {
        draw_rgb_line(src, dst, pf, w);
        src += (w << 1) + w;
        dst = (u16 *)((u8 *)dst + linesize);
        cnt--;
    }
}

static void erase_status(sysenv_t *senv) {
    if (senv->status_background) {
        sdl_ui_t *ui = senv->ui_env;
        u16 *src = (u16 *)senv->status_background;
        u16 *dst = (u16 *)((u8 *)ui->surf->pixels + (STATUS_Y * ui->surf->pitch)) + STATUS_X;
        u32 i;

        for (i = 0; i < STATUS_H; i++) {
            memcpy(dst, src, STATUS_W << 1);
            src += STATUS_W;
            dst = (u16 *)((u8 *)dst + ui->surf->pitch);
        }
    }
}

void ui_exit(sysenv_t *senv) {
    sdl_ui_t *ui = (sdl_ui_t *)senv->ui_env;
    longjmp(ui->jbuf, 1);
}

void ui_set_status(sysenv_t *senv, char *s, u32 color) {
    char *s1, *s2;
    u32 x, y, w, h;
    u16 *fb;
    u32 slen1, slen2, *pslen;
    s1 = s;
    s2 = NULL;
    slen1 = slen2 = 0;
    pslen = &slen1;
    while (*s) {
        if (*s == '\n') {
            pslen = &slen2;
            s2 = s + 1;
        }
        else
            (*pslen)++;
        s++;
    }
    
    if (senv->status_background) {
        sdl_ui_t *ui = senv->ui_env;
        erase_status(senv);
        h = 8;
        if (s2)
            h += 12;
        if (slen1 > 0) {
            SDL_PixelFormat *fmt;
            u16 cc;
            
            y = STATUS_Y + ((STATUS_H - h) >> 1);
            
            w = slen1 << 3;
            if (w > STATUS_W) {
                slen1 = STATUS_W >> 3;
                w = slen1 << 3;
            }
            x = STATUS_X + ((STATUS_W - w) >> 1);
            fb = (u16 *)(ui->surf->pixels + y * ui->surf->pitch) + x;
            
            fmt = ui->surf->format;
            cc = (((color & 0xFF) >> fmt->Rloss) << fmt->Rshift) |
                ((((color >> 8) & 0xFF) >> fmt->Gloss) << fmt->Gshift) |
                ((((color >> 16) & 0xFF) >> fmt->Bloss) << fmt->Bshift);
            speccy_draw_text(senv, fb, ui->surf->pitch, s1, slen1, cc);

            if (slen2 > 0) {
                y += 10;
                w = slen2 << 3;
                if (w > STATUS_W) {
                    slen2 = STATUS_W >> 3;
                    w = slen2 << 3;
                }
                x = STATUS_X + ((STATUS_W - w) >> 1);
                fb = (u16 *)(ui->surf->pixels + y * ui->surf->pitch) + x;
                speccy_draw_text(senv, fb, ui->surf->pitch, s2, slen2, cc);
            }
        }
        SDL_UpdateRect(ui->surf, STATUS_X, STATUS_Y, STATUS_W, STATUS_H);
    }
}

static u32 draw_frame(sysenv_t *senv) {
    u8 *top, *bottom, *right, *left;
    unsigned len;
    u32 rv = 0;
    sdl_ui_t *ui = senv->ui_env;
    u8 *fb = ui->surf->pixels;

    senv->status_background = malloc((STATUS_W * STATUS_H) << 1);
    if (!senv->status_background)
        return 0;

    top = malloc(TOP_LEN + BOTTOM_LEN + RIGHT_LEN + LEFT_LEN);
    if (!top) {
        free(senv->status_background);
        senv->status_background = NULL;
        return 0;
    }
    bottom = top + TOP_LEN;
    right = bottom + BOTTOM_LEN;
    left = right + RIGHT_LEN;

    len = TOP_LEN;
    if (BZ2_bzBuffToBuffDecompress((char *)top, &len,
                                      (char *)frame_top, sizeof(frame_top),
                                      0, 0) != BZ_OK)
        goto fail;

    len = BOTTOM_LEN;
    if (BZ2_bzBuffToBuffDecompress((char *)bottom, &len,
                                      (char *)frame_bottom, sizeof(frame_bottom),
                                      0, 0) != BZ_OK)
        goto fail;

    len = RIGHT_LEN;
    if (BZ2_bzBuffToBuffDecompress((char *)right, &len,
                                      (char *)frame_right, sizeof(frame_right),
                                      0, 0) != BZ_OK)
        goto fail;

    len = LEFT_LEN;
    if (BZ2_bzBuffToBuffDecompress((char *)left, &len,
                                      (char *)frame_left, sizeof(frame_left),
                                      0, 0) != BZ_OK)
        goto fail;

    draw_rgb_rect(top, (u16 *)fb, ui->surf->format, WINDOW_WIDTH, TOP_HEIGHT, ui->surf->pitch);
    SDL_UpdateRect(ui->surf, 0, 0, WINDOW_WIDTH, TOP_HEIGHT);

    fb += TOP_HEIGHT * (u32)ui->surf->pitch;
    draw_rgb_rect(left, (u16 *)fb, ui->surf->format, LEFT_WIDTH, 384, ui->surf->pitch);
    SDL_UpdateRect(ui->surf, 0, TOP_HEIGHT, LEFT_WIDTH, 384);

    fb += (LEFT_WIDTH + 512) << 1;
    draw_rgb_rect(right, (u16 *)fb, ui->surf->format, RIGHT_WIDTH, 384, ui->surf->pitch);
    SDL_UpdateRect(ui->surf, LEFT_WIDTH + 512, TOP_HEIGHT, RIGHT_WIDTH, 384);

    fb = ui->surf->pixels + (TOP_HEIGHT + 384) * ui->surf->pitch;
    draw_rgb_rect(bottom, (u16 *)fb, ui->surf->format, WINDOW_WIDTH, BOTTOM_HEIGHT, ui->surf->pitch);
    if (senv->status_background) {
        u16 *dst = (u16 *)senv->status_background;
        u16 *src = (u16 *)((u8 *)ui->surf->pixels + (STATUS_Y * ui->surf->pitch)) + STATUS_X;
        u32 i;
        for (i = 0; i < STATUS_H; i++) {
            memcpy(dst, src, STATUS_W << 1);
            dst += STATUS_W;
            src = (u16 *)((u8 *)src + ui->surf->pitch);
        }
    }
    SDL_UpdateRect(ui->surf, 0, TOP_HEIGHT + 384, WINDOW_WIDTH, BOTTOM_HEIGHT);

    rv++;
fail:
    free(top);
    return rv;
}

void ui_draw_initial_screen(sysenv_t *senv) {
    sdl_ui_t *ui = senv->ui_env;
    s_rect_t rect;
    u8 *fb = ui->surf->pixels + TOP_HEIGHT * (u32)ui->surf->pitch + (LEFT_WIDTH << 1);
    
    draw_frame(senv);
    if (speccy_title(senv, fb, ui->surf->pitch, ui->palette, &rect)) {
        SDL_UpdateRect(ui->surf, rect.x + LEFT_WIDTH, rect.y + TOP_HEIGHT, rect.w, rect.h);
    }
    ui_set_status(senv, "Hit [SPACE] to start", 0x00FFFF);
}

static int startup_graphics(sdl_ui_t *ui) {
    ui->surf = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT, 16, SDL_SWSURFACE);
    if (!ui->surf)
	    return 0;

    SDL_WM_SetCaption("XOR: The Ultimate Maze Challenge", NULL);

    reset_palette(ui);
    return 1;
}

static sdl_ui_t *ui_init(void) {
    sdl_ui_t *ui = malloc(sizeof(sdl_ui_t));
    u32 i;
    
    if (!ui)
        return NULL;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        goto fail;
    
    ui->time_overflow = 0;
    ui->prev_ticks = SDL_GetTicks();

    if (!startup_graphics(ui))
        goto fail1;
    
    for (i = 0; i < 4; i++) {
        ui->stick_state[i] = 0;
        ui->stick_times[i] = 0;
    }

    return ui;

fail1:
    SDL_Quit();
fail:
    free(ui);
    return NULL;
}

static void ui_quit(sdl_ui_t *ui) {
    SDL_Quit();
    free(ui);
    exit(0);
}

u64 ui_get_ticks(void *vpui) {
    sdl_ui_t *ui = (sdl_ui_t *)vpui;
    u32 ticks = SDL_GetTicks();
    if (ticks < ui->prev_ticks)
        ui->time_overflow++;
    ui->prev_ticks = ticks;
    return ((u64)ui->time_overflow << 32) | (u64)ticks;
}

void ui_delay(sysenv_t *senv, u32 ms) {
    SDL_Delay(ms);
}

/* It's very important to Xor not to make extra occasional moves.
   So, we should implement autorepeat-like mechanism to do exactly
   one move on a short arrow key press */

/* For a first arrow key press we will send press and immediate release. */
static void stick_event(sysenv_t *senv, u32 id, u32 state) {
    sdl_ui_t *ui = senv->ui_env;

    if (state) {
        ui->stick_state[id] = 1;
        ui->stick_times[id] = SDL_GetTicks();
        game_key_event(senv, stick_keys[id], 1);
        game_key_event(senv, stick_keys[id], 0);
    } else {
        ui->stick_state[id] = 0;
        ui->stick_times[id] = 0;
        game_key_event(senv, stick_keys[id], 0);
    }
}

/* This finction will be called after every event queue poll. */
void ui_update_stick(sysenv_t *senv) {
    u32 i;
    u32 curt = 0;
    sdl_ui_t *ui = senv->ui_env;

    for (i = 0; i < 4; i++) {
        if (ui->stick_state[i]) {
            if (!curt)
                curt = SDL_GetTicks();
            if (curt - ui->stick_times[i] > 200) {
                /* Send press for arrow key again and 
                keep it pressed until the real release only
                if it is pressed for abobe 200 ms */
                game_key_event(senv, stick_keys[i], 1);
            }
        }
    }
}

static void ui_key_event(sysenv_t *senv, SDL_KeyboardEvent *ev) {
    u32 i;
    u32 state = (ev->state == SDL_PRESSED);
    switch (ev->keysym.sym) {
        case SDLK_UP:
            stick_event(senv, 0, state);
            break;
        
        case SDLK_DOWN:
            stick_event(senv, 1, state);
            break;
        
        case SDLK_LEFT:
            stick_event(senv, 2, state);
            break;

        case SDLK_RIGHT:
            stick_event(senv, 3, state);
            break;

        default:
            for (i = 0; i < sizeof(sdl_keys) / sizeof(sdl_keys[0]); i++) {
                if (ev->keysym.sym == sdl_keys[i]) {
                    game_key_event(senv, speccy_keys[i], state);
                    break;
                }
            }
            break;
    }
}

void ui_update(sysenv_t *senv) {
    sdl_ui_t *ui = (sdl_ui_t *)senv->ui_env;
    s_rect_t rect;
    u8 *fb = ui->surf->pixels + TOP_HEIGHT * (u32)ui->surf->pitch + (LEFT_WIDTH << 1);
    if (speccy_screen(senv, fb, ui->surf->pitch, ui->palette, &rect)) {
        SDL_UpdateRect(ui->surf, rect.x + LEFT_WIDTH, rect.y + TOP_HEIGHT, rect.w, rect.h);
    }
}

int ui_check_events(sysenv_t *senv) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            game_exit(senv);
        }
        else if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN)
            ui_key_event(senv, &ev.key);
    }
    return 1;
}

u32 speccy_save_ram(sysenv_t *senv, char *fn);
static void main_loop(sysenv_t *senv) {
    while (1) {
        game_tic(senv);
    }
}

int main(int argc, char *argv[]) {
    sdl_ui_t *ui;
    sysenv_t *senv;

    ui = ui_init();
    if (!ui) {
        exit(1);
    }
    
    if (setjmp(ui->jbuf) == 0) {
        senv = game_init(ui);
        if (senv)
            main_loop(senv);
    }

    game_quit(senv);
    ui_quit(ui);
    return 0;
}
