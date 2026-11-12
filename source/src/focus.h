#ifndef KPC_FOCUS_H
#define KPC_FOCUS_H
#include "winmini.h"
extern U8 *g_focus_client;
extern U32 g_focus_enabled,g_focus_resync;
extern void (*g_focus_log)(const char *);
int kpc_remote_multiplayer(void);
U32 __attribute__((thiscall)) kpc_focus_release(void *sound);
U32 __attribute__((thiscall)) kpc_focus_restore(void *sound);
U32 __attribute__((thiscall)) kpc_sound_update(void *sound);
U32 kpc_focus_resyncs(void);
#endif
