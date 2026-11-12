#ifndef KPC_CAMERA_H
#define KPC_CAMERA_H
#include "winmini.h"
extern U32 g_camera_smooth;
extern U8 *g_camera_shell;
extern U32 (WINAPI *g_camera_clock)(void);
void __attribute__((thiscall)) kpc_camera_set(void *camera,const float *pos,const float *rot);
void kpc_camera_tick(void);
U32 kpc_camera_samples(void);
U32 kpc_camera_snaps(void);
#endif
