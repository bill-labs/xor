#ifndef __UI_H
#define __UI_H

#include "globals.h"

void ui_exit(sysenv_t *senv);
void ui_delay(sysenv_t *senv, u32 ms);
void ui_update(sysenv_t *senv);
u64 ui_get_ticks(void *vpui);
int ui_check_events(sysenv_t *senv);
void ui_draw_initial_screen(sysenv_t *senv);
void ui_set_status(sysenv_t *senv, char *s, u32 color);
void ui_update_stick(sysenv_t *senv);

#endif
