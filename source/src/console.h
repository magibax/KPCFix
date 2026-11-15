#ifndef KPC_CONSOLE_H
#define KPC_CONSOLE_H
#include "winmini.h"
extern U32 g_console_key_fix,g_chat_blocks_actions;
extern U8 *g_command_shell;
void __attribute__((thiscall)) kpc_command_on(void *shell,U32 action);
U8 __attribute__((thiscall)) kpc_chat_gate(void *shell);
void __attribute__((cdecl)) kpc_chat_done(void);
extern U32 g_sp_chat,g_menu_disconnect;
void __attribute__((thiscall)) kpc_main_quit(void *menus,U32 type);
void __attribute__((thiscall)) kpc_command_off(void *shell,U32 action);
U32 kpc_scoreboard_closes(void);
extern U32 g_scoreboard_hold;
U32 kpc_blocked_actions(void);
void kpc_console_key_entry(void);
void kpc_console_char_entry(void);
U32 CDECL kpc_console_key_down(void);
U32 CDECL kpc_console_char_drop(void);
void kpc_input_state_entry(void);
U32 CDECL kpc_input_enabled(U32 state,unsigned char *commands);
U32 kpc_blocked_input_frames(void);
extern U32 g_window_watch,g_window_hide;
extern U8 *g_window_shell;
void __attribute__((thiscall)) kpc_window_break(void *,void *,U32,U32);
U32 kpc_window_breaks(void);
U32 kpc_window_missing(void);
extern U32 g_chat_clipboard,g_chat_scrollback,g_chat_long,g_chat_history,g_chat_hide_loading,g_chat_in_cinematics,g_chat_lower_font;
void __attribute__((thiscall)) kpc_list_draw(void *list,void *surface);
void __attribute__((thiscall)) kpc_chat_init(void *editor,const char *label,const char *text,U32 width,U32 charset,U32 own_line);
void __attribute__((thiscall)) kpc_shell_key(void *shell,U32 vk,U32 repeat);
void __attribute__((thiscall)) kpc_chat_insert(void *editor,U32 vk);
void __attribute__((thiscall)) kpc_chat_escape(void *editor);
void __attribute__((thiscall)) kpc_list_update(void *list);
void __attribute__((thiscall)) kpc_chat_draw(void *editor,void *surface);
void __attribute__((thiscall)) kpc_chat_add(void *list,const char *text,float seconds,U32 flags,U32 colour);
unsigned char __attribute__((thiscall)) kpc_chat_key(void *editor,U32 vk,U32 flag);
#endif
