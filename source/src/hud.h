#ifndef KPC_HUD_H
#define KPC_HUD_H
#include "winmini.h"
extern U32 g_hud_percent;
extern U8 *g_hud_shell;
extern void hud_panel_entry(void),hud_post_entry(void),hud_rect_entry(void);
extern void hud_solid_ecx(void),hud_scale_eax(void),hud_scale_edx(void);
extern void hud_cross_ecx(void),hud_cross_eax(void);
extern void hud_carousel_entry(void);
void CDECL kpc_hud_observe(U32 width,U32 height,U32 screen);
#endif
