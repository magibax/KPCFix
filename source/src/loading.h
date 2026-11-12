#ifndef KPC_LOADING_H
#define KPC_LOADING_H
#include "winmini.h"
extern U8 *g_loading_shell;
extern U32 g_loading_enabled,g_loading_min_ms,g_loading_hold;
extern void (*g_loading_log)(const char *);
extern U8 *g_loading_client;
extern U32 g_loading_entered,g_id_print_target;
void kpc_new_connection_print(void);
int kpc_campaign_world(const char *world);
void __attribute__((thiscall)) kpc_preload_world(void *shell,const char *world);
void __attribute__((thiscall)) kpc_close_on_entry(void *screens);
void __attribute__((thiscall)) kpc_draw_screen(void *screens,U32 argument);
void __attribute__((thiscall)) kpc_spawn_reply(void *self);
void __attribute__((thiscall)) kpc_spawn_initvars(void *self);
U32 __attribute__((thiscall)) kpc_client_frame(void *client,U32 argument);
U32 kpc_loading_shown(void);
U32 kpc_loading_holds(void);
#endif
