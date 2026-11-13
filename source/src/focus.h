#ifndef KPC_FOCUS_H
#define KPC_FOCUS_H
#include "winmini.h"
extern U8 *g_focus_client,*g_focus_shell;
extern U32 g_focus_enabled,g_focus_resync,g_focus_default,g_focus_keep_render;
extern void (*g_focus_log)(const char *);
int kpc_remote_multiplayer(void);
int kpc_keep_running(void);
void kpc_set_keep_running(int on);
U32 __attribute__((thiscall)) kpc_focus_release(void *sound);
U32 __attribute__((thiscall)) kpc_focus_restore(void *sound);
U32 __attribute__((thiscall)) kpc_sound_update(void *sound);
U32 kpc_focus_resyncs(void);
U32 CDECL kpc_focus_term_render(void *cm,U32 flag);
U32 CDECL kpc_focus_init_render(void *cm,void *mode);
U32 CDECL kpc_focus_world_active(void);
U32 kpc_focus_renders_kept(void);
void __attribute__((thiscall)) kpc_music_pause(void *music);
void __attribute__((thiscall)) kpc_music_resume(void *music);
U32 CDECL kpc_focus_active(void);
U32 kpc_focus_kept_running(void);
#endif
