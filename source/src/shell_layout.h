#ifndef KPC_SHELL_LAYOUT_H
#define KPC_SHELL_LAYOUT_H
#include "winmini.h"

/* RVAs relative to the loaded cshell.dll. Only a SHA-256 verified profile
 * may supply these. The game can relocate this DLL at load time. */
typedef struct {
    U32 ground_move, ground_guard, ground_guard_target;
    U32 sensitivity_sites[3], sensitivity_constants[3], sensitivity_min_store;
    U32 enemy_draw, enemy_call, enemy_api, enemy_label_y_bias;
    U32 hud_sites[13], panel_position, panel_number;
    U32 crosshair_flag, crosshair_x, crosshair_y;
    U32 cursor_dims, cursor_draw, tooltip_font, tooltip_lift[3];
    U32 rate_setter, rate_stack, menu_ready_call, menu_ready, menu_key_slot, menu_key;
    U32 widget_y[4];
    U8 widget_y_modrm; /* MOV [widget+0x2c], imm32: EBP or ESI */
} ShellLayout;

extern const ShellLayout *g_shell_layout;
#endif
