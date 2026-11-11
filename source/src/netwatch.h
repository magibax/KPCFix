#ifndef KPC_NETWATCH_H
#define KPC_NETWATCH_H
#include "winmini.h"
extern U8 *g_net_shell_slot;
extern U32 g_net_timeout_enabled;
extern float g_net_timeout_seconds;
U32 CDECL kpc_net_armed(void);
void net_timeout_compare(void);
#endif
