#ifndef KPC_CONSOLE_H
#define KPC_CONSOLE_H
#include "winmini.h"
extern U32 g_console_key_fix,g_chat_blocks_actions;
extern U8 *g_command_shell;
void __attribute__((thiscall)) kpc_command_on(void *shell,U32 action);
U32 kpc_blocked_actions(void);
void kpc_console_key_entry(void);
void kpc_console_char_entry(void);
U32 CDECL kpc_console_key_down(void);
U32 CDECL kpc_console_char_drop(void);
void kpc_input_state_entry(void);
U32 CDECL kpc_input_enabled(U32 state);
U32 kpc_blocked_input_frames(void);
#endif
