#ifndef __GAME_H
#define __GAME_H

#include "globals.h"

sysenv_t *game_init(void *ui);
void game_quit(sysenv_t *senv);
void game_tic(sysenv_t *senv);
void game_key_event(sysenv_t *senv, u32 key, u32 state);
void game_exit(sysenv_t *senv);

char *path_combine(char *s1, char *s2);
char *path_append(char *s1, char *s2);

#define SCREEN_OTHER    0
#define SCREEN_MENU     1
#define SCREEN_INTRO    2
#define SCREEN_SELECT   3
#define SCREEN_TITLE    4
#define SCREEN_UNKNOWN  5

#endif
