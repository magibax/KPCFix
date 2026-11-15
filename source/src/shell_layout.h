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
    /* Campaign loading art (loading.c); zero = not verified for this edition. */
    U32 preload_slot, preload_world, load_background, screen_show, screen_close;
    U32 close_call, screen_draw, draw_call, render_frame, multiplayer_test;
    /* Spawn hold (loading.c): the 0x17 handler's reply calls; zero = off. */
    U32 spawn_reply_call, spawn_reply, initvars_call, spawn_initvars;
    /* Synchronised load (loading.c): OnMessage vtable slot and original; zero = off. */
    U32 message_slot, on_message;
    /* Clean music on level changes (loading.c): the 0x46 handler's call, the
     * multiplayer entry wipe call, the music manager global and its stop,
     * hold, apply and unload methods; zero = off. */
    U32 change_level_call, change_level, enter_wipe_call, music_unload, music_global, music_stop, music_hold, music_apply;
    /* Chat/message lifetime: MOV [ESP+0x10], 3.0f in the message handler; zero = off. */
    U32 chat_duration;
    /* Chat typing (console.c): OnCommandOn vtable slot and original, chat
     * editor offset in the shell and its "typing" byte; zero = off. */
    U32 command_slot, command_on, chat_editor, chat_editing;
    /* Scoreboard key held (console.c): OnCommandOff vtable slot and original
     * (an empty stub in 1.13); zero = off. Uses screen_close above. */
    U32 command_off_slot, command_off;
    /* Chat editor keys (console.c): the editor's OnKeyDown vtable slot and
     * original, and its layout refresh; zero = off. */
    U32 chat_key_slot, chat_key, chat_layout;
    /* Chat scrollback (console.c): the 0x45 handler's add call, the add
     * routine and the message list object; zero = off. */
    U32 chat_add_call, chat_add, chat_list;
    /* Longer chat lines and highlight (console.c): editor Draw and Esc slots,
     * the insert call, key-to-character, the scroll cap and width sites, the
     * list's per-frame update call; zero = off. */
    U32 chat_draw_slot, chat_draw, chat_escape_slot, chat_escape, chat_insert_call, chat_insert, key_char;
    U32 chat_scroll_cap, chat_width, list_update_call, list_update;
    /* Spawn lightning (kpcfix.c): the bolt branch in Respawn Player; level
     * change picture (loading.c): the frame clear; zero = off. */
    U32 spawn_bolt, render_clear;
    /* Chat history, hiding while loading, chat in cinematics, label hint
     * (console.c): editor Init slot, shell OnKeyDown slot, the list draw call,
     * the input-lock masks; zero = off. */
    U32 chat_init_slot, chat_init, shell_key_slot, shell_key, list_draw_call, list_draw, input_locks;
    /* Chat bar font with real lowercase (console.c): font term / init, the
     * engine interface global, the "interface/arial.pcf" and
     * "interface/menu_normal.pcf" / "menu_bold.pcf" strings; zero = off. */
    U32 font_term, font_init, interface_api, font_arial, font_menu_normal, font_menu_bold, font_menu_mo;
    /* Music kept on death (loading.c): the player-state 3 intensity call and
     * its target (music manager SetIntensity, thiscall(mgr, level)); zero = off. */
    U32 death_music_call, music_intensity;
    /* Single-player chat (console.c): the Message action's multiplayer test
     * call, the done-callback store (mov [esi+0x1374], imm32) and its
     * original target, the console object global; zero = off. */
    U32 chat_gate_call, chat_done_store, chat_done, console_global;
    /* Disconnect from the main menu (console.c): the main screen's Quit call
     * and its target (the quit dialog, thiscall(menus, type)); zero = off. */
    U32 main_quit_call, quit_dialog;
    /* Single-player loading picture hold and 4:3 loading pictures (loading.c):
     * the 0x19 Show(5) call, the OnEnterWorld close, the unpause in c6f30, the
     * pause request cd7b0, the two picture blits; zero = off. */
    U32 sp_show_call, enter_close_call, sp_unpause_call, pause_request, blit_sp_call, blit_mp_call;
    /* Smooth scripted cameras (camera.c): the sample store call, its target
     * and the global holding the camera object; zero = off. */
    U32 camera_set_call, camera_set, camera_global;
    /* Server-connection timeout (netwatch.c): global holding the client shell. */
    U32 client_shell_global;
    /* Keep running on focus loss (focus.c): OnEvent 4/3 music pause and
     * resume calls and their targets; zero = music stock. */
    U32 music_pause_call, music_pause, music_resume_call, music_resume;
    /* Setup > Audio keep-running checkbox (ui.c): the Audio screen vtable
     * (slots +8 init, +0xc term, +0x10 draw, +0x14 mouse move, +0x18 event)
     * and originals, the checkbox class ctor, image loader and image names;
     * zero = no checkbox. */
    U32 audio_vtable, audio_init, audio_term, audio_draw, audio_move, audio_event;
    U32 checkbox_ctor, checkbox_images, checkbox_checked, checkbox_unchecked;
    U32 checkbox_hit_box, checkbox_hit_label;   /* thiscall hit tests (AL = 1 under the mouse) */
    /* Window-shard diagnostic (console.c): manager vtable slot and handler. */
    U32 window_break_slot, window_break;
} ShellLayout;

extern const ShellLayout *g_shell_layout;
#endif
