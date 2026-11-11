#ifndef KPC_LOADING_H
#define KPC_LOADING_H
#include "winmini.h"
extern U8 *g_loading_shell;
extern U32 g_loading_enabled,g_loading_min_ms;
int kpc_campaign_world(const char *world);
void __attribute__((thiscall)) kpc_preload_world(void *shell,const char *world);
void __attribute__((thiscall)) kpc_close_on_entry(void *screens);
void __attribute__((thiscall)) kpc_draw_screen(void *screens,U32 argument);
U32 kpc_loading_shown(void);
#endif
