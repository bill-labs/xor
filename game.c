#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "globals.h"
#include "game.h"
#include "speccy.h"
#include "ui.h"

void *alloca(size_t);

static char *path_combine1(char *s1, char *s2, char sep) {
    u32 len1 = strlen(s1);
    u32 len2 = strlen(s2);
    char *rv = malloc(len1 + len2 + 2);

    if (!rv)
        return NULL;
    memcpy(rv, s1, len1);
    rv[len1] = sep;
    strcpy(rv + len1 + 1, s2);
    return rv;
}

char *path_combine(char *s1, char *s2) {
    return path_combine1(s1, s2, '/');
}

char *path_append(char *s1, char *s2) {
    return path_combine1(s1, s2, '.');
}

static void skip_screen(sysenv_t *senv, u32 scrid) {
    u32 state = 1;
    u32 id;
    u32 i;

    if (speccy_identify_screen(senv) != scrid)
        return;

    /* Keep pressing and releasing ENTER key until we get off the given screen */
    while (1) {
        speccy_set_key_state(senv, KEY_ENTER, state);
        /* Wait about 1/2 s */
        for (i = 0; i < 25; i++) {
            speccy_frame(senv);
            id = speccy_identify_screen(senv);
            if (id != scrid) {
                speccy_set_key_state(senv, KEY_ENTER, 0);
                return;
            }
        }
        state ^= 1;
    }
}

static void start_game(sysenv_t *senv) {
    u32 id;
    
    if (!speccy_load_diff(senv)) {
        /* Skip the intro screen */
        skip_screen(senv, SCREEN_INTRO);

        /* Wait for the select screen to appear */
        do {
            speccy_frame(senv);
            id = speccy_identify_screen(senv);
        } while (id != SCREEN_SELECT);

        /* Skip the select screen */
        skip_screen(senv, SCREEN_SELECT);

        /* Wait for the menu screen to appear */
        do {
            speccy_frame(senv);
            id = speccy_identify_screen(senv);
        } while (id != SCREEN_MENU);
    }
}

static char *screen_status(sysenv_t *senv, u32 scrid) {
    switch (scrid) {
        case SCREEN_MENU:
            return "Up/Down arrows to move, [SPACE] or [ENTER] to select";

        case SCREEN_INTRO:
            if (senv->flags & INTRO_ERASE_GAME) {
                return "This will erase all your progress. Sure (Y/N)?";
            } else {
                if (access(senv->savepath, R_OK) >= 0)
                    return "[SPACE] - continue game\n[E] erase current progress and start from scratch";
                else
                    return "Hit [SPACE] to enter the game";
            }

        case SCREEN_OTHER:
            return "Arrows - move, [E] - exit, [ENTER] - Magus/Questor switch,\n[M] - map/maze switch, [F2] - save, [F3] - load";

        default:
            return "";
    }
}

static void update_status(sysenv_t *senv) {
    u32 t;
    if (!senv->current_status)
        return;
    t = ui_get_ticks(senv->ui_env);
    if (t >= senv->status_time) {
        senv->current_status = NULL;
        senv->status_time = 0;
        ui_set_status(senv, screen_status(senv, senv->screen_id), 0x00FFFF);
    }
}

void game_set_status(sysenv_t *senv, char *status, u32 color) {
    senv->current_status = status;
    senv->status_time = ui_get_ticks(senv->ui_env) + 3000;
    ui_set_status(senv, status, color);
}

static void check_events(sysenv_t *senv) {
    u32 old_id = senv->screen_id;
    
    if (old_id != SCREEN_TITLE)
        senv->screen_id = speccy_identify_screen(senv);
    
    if (senv->screen_id != old_id) {
        ui_set_status(senv, screen_status(senv, senv->screen_id), 0x00FFFF);
        speccy_clear_input(senv);
    }
   
    ui_check_events(senv);
    ui_update_stick(senv);
    update_status(senv);
}

void game_tic(sysenv_t *senv) {
    u64 r_time;

    check_events(senv);
    if (senv->screen_id != SCREEN_TITLE) {
        speccy_frame(senv);
        ui_update(senv);
    }

    r_time = ui_get_ticks(senv->ui_env) - senv->start_time;
    senv->virt_time += 20;

    if (senv->virt_time > r_time) {
        ui_delay(senv, (u32)(senv->virt_time - r_time));
    }
}

void game_key_event(sysenv_t *senv, u32 key, u32 state) {
    switch (senv->screen_id) {
        case SCREEN_TITLE: {
            if ((key == KEY_SPACE) && state) {
                senv->screen_id = SCREEN_UNKNOWN;
            }
            break;
        }

        case SCREEN_UNKNOWN: {
            break;
        }

        case SCREEN_INTRO: {
            if (senv->flags & INTRO_ERASE_GAME) {
                if (state) {
                    if (key == KEY_Y) {
                        senv->flags &= ~INTRO_ERASE_GAME;
                        unlink(senv->savepath);
                        start_game(senv);
                    } else if (key == KEY_N) {
                        senv->flags &= ~INTRO_ERASE_GAME;
                        ui_set_status(senv, screen_status(senv, senv->screen_id), 0x00FFFF);
                    }
                }
            } else {
                if ((key == KEY_E) && state) {
                    if (access(senv->savepath, R_OK) >= 0) {
                        senv->flags |= INTRO_ERASE_GAME;
                        ui_set_status(senv, screen_status(senv, senv->screen_id), 0x00FFFF);
                    }
                } else if ((key == KEY_SPACE) && state)
                    start_game(senv);
            }
            break;
        }

        case SCREEN_MENU: {
            if (key == KEY_SPACE) {
                if (state)
                    skip_screen(senv, SCREEN_MENU);
            } else {
                speccy_set_key_state(senv, key, state);
            }
            break;
        }

        default: {
            switch (key) {
                case KEY_F2: {
                    if (state) {
                        if (speccy_save_diff(senv)) {
                            game_set_status(senv, "Game saved.", 0x00FFFFFF);
                        } else {
                            game_set_status(senv, "Couldn't save game!", 0x0000FF);
                        }
                    }
                    break;
                }

                case KEY_F3: {
                    if (state) {
                        if (speccy_load_diff(senv)) {
                            game_set_status(senv, "Game loaded.", 0x00FFFFFF);
                        } else {
                            game_set_status(senv, "Couldn't load game!", 0x0000FF);
                        }
                    }
                    break;
                }

                default:
                    speccy_set_key_state(senv, key, state);
                    break;
                }   
        }
    }
}

void game_exit(sysenv_t *senv) {
    u32 scrid = speccy_identify_screen(senv);
    if ((scrid == SCREEN_MENU) || (scrid == SCREEN_OTHER))
        speccy_save_diff(senv);
    ui_exit(senv);
}

void game_quit(sysenv_t *senv) {
    if (!senv)
        return;
    if (senv->shadow_buffer)
        free(senv->shadow_buffer);
    if (senv->status_background)
        free(senv->status_background);
    if (senv->savepath)
        free(senv->savepath);
    if (senv->homedir)
        free(senv->homedir);
    if (senv->speccy_env) {
        if (senv->speccy_env->ram)
            free(senv->speccy_env->ram);
        free(senv->speccy_env);
    }
    free(senv);
}

sysenv_t *game_init(void *ui) {
    sysenv_t *senv;
    struct stat st;
    char *defhome = "/tmp";
    char *h, *p;
    u32 len;

    senv = malloc(sizeof(sysenv_t));
    if (!senv)
        return NULL;

    senv->ui_env = ui;
    senv->screen_id = SCREEN_TITLE;
    senv->shadow_buffer = NULL;
    senv->status_background = NULL;
    senv->current_status = NULL;
    senv->start_time = ui_get_ticks(ui);
    senv->virt_time = 0;
    senv->status_time = 0;
    senv->flags = 0;

    h = getenv("HOME");
    if (!h)
        h = defhome;
    else {
        p = alloca(strlen(h) + 1);
        strcpy(p, h);
        h = p;
        p = h;
        while (*p)
            p++;
        if (p == h) {
            h = defhome;
        } else {
            p--;
            while ((p > h) && (*p == '/'))
                p--;
            p++;
            if (p != h)
                *p = 0;
        }
    }

    len = strlen(h);
    p = malloc(len + 6);
    if (!p)
        goto fail;
    memcpy(p, h, len);
    strcpy(p + len, "/.xor");
    senv->homedir = p;

    if (stat(p, &st) >= 0) {
        if (!S_ISDIR(st.st_mode))
            fprintf(stderr, "WARNING: %s is not a directory\n", p);
    } else {
        if (errno == ENOENT) {
            if (mkdir(p, 0755) < 0)
                fprintf(stderr, "WARNING: Can't create %s: %s\n", p, strerror(errno));
        } else {
            fprintf(stderr, "WARNING: Can't stat %s: %s\n", p, strerror(errno));
        }
    }

    senv->savepath = path_combine(senv->homedir, "game.sav");
    if (!senv->savepath)
        goto fail;

    if (!speccy_init(senv))
        goto fail;
    
    ui_draw_initial_screen(senv);

    /* Reset the start time before entering the main loop */
    senv->start_time = ui_get_ticks(ui);
    return senv;

fail:
    game_quit(senv);
    return NULL;
}
