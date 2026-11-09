#ifndef KPC_UI_H
#define KPC_UI_H
#include "winmini.h"
extern U32 g_cursor_percent,g_lock_input_rate,g_hide_input_rate;
extern U32 g_tooltip_percent;
extern U8 *g_ui_shell;
extern void *g_rate_setter_tail;
extern void ui_cursor_dims_entry(void),ui_cursor_draw_entry(void),ui_tooltip_font_entry(void),ui_rate_setter_entry(void),ui_rate_setter_italian(void);
U32 CDECL kpc_rate_value(void *,void *,const char *);
U32 __attribute__((thiscall)) kpc_mouse_menu_ready(void *);
U32 __attribute__((thiscall)) kpc_mouse_menu_key(void *,U32,U32);
void CDECL kpc_normalize_configs(const WCHAR *);
void CDECL kpc_config_notice(const WCHAR *,const char *);
#endif
