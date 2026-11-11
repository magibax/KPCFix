#include "winmini.h"
#include "focus.h"

/* Keep server-owned audio running on focus loss in remote multiplayer
 * ([Multiplayer] KeepAudioOnFocusLoss, default 1).
 *
 * Evidence (client.exe 16131ccf, static analysis):
 *  - WM_ACTIVATEAPP (window procedure 0x6be0): on deactivate 0x6d5c calls
 *    the sound manager's 0x74b90 (thiscall, ECX = cm+0x284), which pauses
 *    every instance (PauseAll 0x74c90) or releases the Miles driver; on
 *    activate 0x6cdb calls 0x74c10, which resumes them from their old
 *    position (ResumeAll / reacquire);
 *  - the main loop never blocks: network dispatch (0x6dfa0) and server
 *    sound-set updates keep running while inactive; only output stalls;
 *  - a single-player or hosting client also pauses its in-process server
 *    ([sh+0x50], 0x157a7), so pausing its audio stays consistent. A remote
 *    client has no local server: the dedicated server keeps its clock, so a
 *    resumed narration lags the server and overlaps the next stream until the
 *    server's removal cuts it.
 * In remote multiplayer the pause and its matching resume are skipped (the
 * flag keeps the pair balanced even if the mode changes while away).
 * Single player, a hosting client and menus keep the stock behaviour. */
U8 *g_focus_client;
U32 g_focus_enabled;
static U32 skipped;

#define CM_GLOBAL 0xaacd0u
#define CM_SESSION 0x1004u
#define SESSION_LOCAL_SERVER 0x50u
#define SESSION_MODE 0xa8u
typedef U32 (__attribute__((thiscall)) *SoundFn)(void *);

/* Same game-mode rule as the shell's multiplayer test (cshell 0xcab80 via
 * GetGameMode 0xbdf0: mode 5 = no game, 4 = not multiplayer), plus no local
 * (in-process) server. */
int kpc_remote_multiplayer(void){
    U8 *cm,*session;U32 mode;
    if(!g_focus_client)return 0;
    cm=*(U8 **)(g_focus_client+CM_GLOBAL);
    if(!cm||!(session=*(U8 **)(cm+CM_SESSION)))return 0;
    mode=*(const U32 *)(session+SESSION_MODE);
    return mode!=4u&&mode!=5u&&*(const U32 *)(session+SESSION_LOCAL_SERVER)==0;
}
U32 __attribute__((thiscall)) kpc_focus_release(void *sound){
    if(g_focus_enabled&&kpc_remote_multiplayer()){skipped=1;return 1;}
    skipped=0;
    return ((SoundFn)(g_focus_client+0x74b90u))(sound);
}
U32 __attribute__((thiscall)) kpc_focus_restore(void *sound){
    if(skipped){skipped=0;return 1;}
    return ((SoundFn)(g_focus_client+0x74c10u))(sound);
}
