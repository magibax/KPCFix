#ifndef KPC_CONSOLE_H
#define KPC_CONSOLE_H
#include "winmini.h"
extern U32 g_console_key_fix;
void kpc_console_key_entry(void);
void kpc_console_char_entry(void);
U32 CDECL kpc_console_key_down(void);
U32 CDECL kpc_console_char_drop(void);
#endif
