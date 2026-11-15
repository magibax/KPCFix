#include "winmini.h"
#include "netwatch.h"

/* Multiplayer server-connection timeout ([Multiplayer] ServerTimeoutSeconds,
 * default 15, 0 = stock).
 *
 * Evidence (client.exe 16131ccf, static analysis):
 *  - every packet resets the connection's idle time conn+0x74 (0x4cb03);
 *  - CNetMgr::Update 0x4b240 adds the clamped frame time (<= 0.3 s) to it
 *    (0x4b465) and, when it exceeds the driver's timeout driver+0x4c,
 *    disconnects (0x4b4ec-0x4b538: Disconnect(conn, 3) -> DisconnectNotify ->
 *    CClientShell::OnDisconnect -> cshell OnEvent(1): menu mode and the stock
 *    message 503 "You were disconnected from the server.");
 *  - the remote TCP driver's timeout is 120.0 s (ctor 0x76a27), the local
 *    single-player driver's 1e6 s.
 * So a dead server is noticed only after two minutes of walking around a
 * stale world. KPCFix keeps the whole native path and only shortens the
 * threshold while the client is in a multiplayer world (shell state 1).
 * During level changes (state 5), menus, the scoreboard and connecting the
 * stock 120 s applies. Idle time accrues per frame, so a stalled frame counts
 * at most 0.3 s: the check can only err on the late side. */
U8 *g_net_shell_slot;          /* cshell global holding the client shell object */
U32 g_net_timeout_enabled;
float g_net_timeout_seconds=15.0f;

#define SHELL_STATE 0x1398u
#define STATE_IN_WORLD 1u

U32 CDECL kpc_net_armed(void){
    U8 *shell;
    if(!g_net_timeout_enabled||!g_net_shell_slot)return 0;
    shell=*(U8 **)g_net_shell_slot;
    return shell&&*(const U32 *)(shell+SHELL_STATE)==STATE_IN_WORLD;
}
