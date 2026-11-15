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
extern U32 g_loading_sync;
void __attribute__((thiscall)) kpc_shell_message(void *shell,U32 id,void *message);
extern U32 g_clean_music,g_level_picture;
void __attribute__((thiscall)) kpc_change_levels(void *shell,U32 arg);
void __attribute__((thiscall)) kpc_death_music(void *mgr,U32 level);
extern U32 g_keep_music_on_death,g_stop_sounds_on_change,g_intro_picture_hold;
void __attribute__((thiscall)) kpc_enter_music(void *mgr,U32 flag);
extern U32 g_loading_43,g_sp_loading_ms,g_min_update_rate,g_loading_filter;
unsigned int __attribute__((cdecl)) kpc_loading_blit(void *dest,void *src,const void *dr,const void *sr);
void __attribute__((thiscall)) kpc_sp_show(void *mgr,int type);
void __attribute__((thiscall)) kpc_sp_enter_close(void *mgr);
void __attribute__((thiscall)) kpc_sp_unpause(void *shell,U32 pause,U32 extra);
#endif
