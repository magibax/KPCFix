#include "winmini.h"
#include "focus.h"
#include "shell_layout.h"

/* Keep running when the window loses focus ([Multiplayer] KeepAudioOnFocusLoss,
 * default 1; console variable of the same name, also set by the Setup > Audio
 * checkbox). While it is on, minimising pauses nothing, in every mode: sounds,
 * music and a single-player or hosting client's in-process server keep running.
 *
 * Evidence (client.exe 16131ccf, static analysis):
 *  - WM_ACTIVATEAPP (window procedure 0x6be0): on deactivate 0x6d1d calls
 *    r_TermRender 0x53980 (cshell OnEvent 4: the music manager's pause 0xd9ad0
 *    at cshell 0xc80e2), then 0x6d5c calls the sound manager's 0x74b90
 *    (thiscall, ECX = cm+0x284), which pauses every instance (PauseAll 0x74c90)
 *    or releases the Miles driver; on activate 0x6c97 r_InitRender (OnEvent 3:
 *    music resume 0xd9b00 at cshell 0xc80ba) and 0x6cdb 0x74c10 resumes them;
 *  - the main loop never blocks: network dispatch (0x6dfa0) and sound updates
 *    keep running while inactive;
 *  - a client with an in-process server ([sh+0x50]) pauses it while inactive:
 *    0x157c5 calls the active-window test 0x287b0 (returns [0xa9f40]); the
 *    stock console variable "alwaysfocused" = 1 overrides it (read only there).
 * With the setting on: the sound pause and its matching resume are skipped (a
 * flag keeps the pair balanced if the setting changes while away), the music
 * pause during the focus-loss render shutdown and its matching resume are
 * skipped, and the local server's active-window test reports active, so
 * single-player narration, scripts and sounds stay in step with each other.
 *
 * Resync on focus regain ([Multiplayer] ResyncAudioOnFocus, default 1).
 * Streamed narration is fed by the main thread, which the window procedure
 * blocks while it tears the renderer down on focus loss (0x53980) and
 * restarts it on regain (0x534f0): the stream starves and ends up behind its
 * server by about the stall. Each instance's remaining time (+0x28) keeps
 * counting in real time (AIL_ms_count) because the pause was skipped, and the
 * engine's sample start 0x70db0 places a sound at duration - remaining
 * (AIL_set_sample_ms_position / 3D offset / stream byte offset). So
 * RESYNC_DELAY_MS after regain, once, the per-frame sound update call
 * (0x13bcd -> 0x73bb0, thiscall ECX = cm+0x284) first runs the engine's own
 * PauseAll 0x74c90 and ResumeAll 0x74ce0: every instance is stopped with its
 * remaining time kept (0x70a30 / 0x70a60), and the update that follows
 * restarts each one at its real-time position. */
U8 *g_focus_client,*g_focus_shell;
U32 g_focus_enabled,g_focus_resync,g_focus_default=1;
void (*g_focus_log)(const char *);
static U32 skipped,resync_pending,resync_due,resyncs,losing,music_skipped,kept_running;
#define RESYNC_DELAY_MS 300u

#define CM_GLOBAL 0xaacd0u
#define CM_SESSION 0x1004u
#define SESSION_LOCAL_SERVER 0x50u
#define SESSION_MODE 0xa8u
typedef U32 (__attribute__((thiscall)) *SoundFn)(void *);
typedef void (__attribute__((thiscall)) *MusicFn)(void *);

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
/* The console variable KeepAudioOnFocusLoss ("KeepAudioOnFocusLoss 0" or "1"
 * in the console). It is created with the INI value the first time it is
 * needed (the console exists by then) through the console's own lookup
 * 0x1b010 and setter 0x1abd0 on its table 0xab3d8, the same calls the
 * InputRate lock uses; its float value is at variable+0x18 (as read by the
 * window procedure, 0x6fa5). A value already defined (for example by the
 * configuration) is found by the lookup and wins over the INI default. */
typedef void *(CDECL *CvarLookup)(void *,const char *);
typedef U32 (CDECL *CvarSet)(void *,const char *,const char *,U32,U32,U32,U32);
static const char cvar_name[]="KeepAudioOnFocusLoss";
static U8 *focus_cvar(void){
    U8 *var;
    if(!g_focus_client)return NULL;
    var=(U8 *)((CvarLookup)(g_focus_client+0x1b010u))(g_focus_client+0xab3d8u,cvar_name);
    if(!var){
        ((CvarSet)(g_focus_client+0x1abd0u))(g_focus_client+0xab3d8u,cvar_name,g_focus_default?"1":"0",0,0,0,0);
        var=(U8 *)((CvarLookup)(g_focus_client+0x1b010u))(g_focus_client+0xab3d8u,cvar_name);
    }
    return var;
}
int kpc_keep_running(void){
    U8 *var;
    if(!g_focus_enabled)return 0;
    if(!(var=focus_cvar()))return g_focus_default!=0;
    return *(const float *)(var+0x18)!=0.0f;
}
void kpc_set_keep_running(int on){
    if(!g_focus_enabled||!g_focus_client)return;
    focus_cvar();
    ((CvarSet)(g_focus_client+0x1abd0u))(g_focus_client+0xab3d8u,cvar_name,on?"1":"0",0,0,0,0);
}
U32 __attribute__((thiscall)) kpc_focus_release(void *sound){
    if(kpc_keep_running()){skipped=1;return 1;}
    skipped=0;
    return ((SoundFn)(g_focus_client+0x74b90u))(sound);
}
U32 __attribute__((thiscall)) kpc_focus_restore(void *sound){
    if(skipped){
        skipped=0;
        if(g_focus_resync){resync_pending=1;resync_due=GetTickCount()+RESYNC_DELAY_MS;}
        return 1;
    }
    return ((SoundFn)(g_focus_client+0x74c10u))(sound);
}
/* Replaces the per-frame sound update call at 0x13bcd. */
U32 __attribute__((thiscall)) kpc_sound_update(void *sound){
    if(resync_pending&&(I32)(GetTickCount()-resync_due)>=0){
        resync_pending=0;
        ((SoundFn)(g_focus_client+0x74c90u))(sound);
        ((SoundFn)(g_focus_client+0x74ce0u))(sound);
        resyncs++;
        if(g_focus_log)g_focus_log("Focus regained: sounds re-synchronised to real time.\r\n");
    }
    return ((SoundFn)(g_focus_client+0x73bb0u))(sound);
}
U32 kpc_focus_resyncs(void){return resyncs;}
/* Replaces "call 0x53980" (r_TermRender, cdecl) at 0x6d1d, the focus-loss
 * renderer shutdown. With the setting on and [Multiplayer]
 * KeepRendererOnFocusLoss=1 (default) the renderer is not shut down: the game
 * keeps drawing and animating exactly as when focused (the matching restart,
 * r_InitRender at 0x6c97, is skipped as a pair). Input still stops: the
 * engine's input poll (0x1374c, 0x137fc) and mouse centring (0x696c) keep
 * their stock window tests, and 0x13a20 still resets input on deactivate.
 * If a fullscreen surface is lost anyway, d3d.ren restores it on its next
 * flip (IsLost/Restore at d3d.ren 0x1d542). Otherwise this marks the music
 * pause the shutdown triggers as a focus loss. */
U32 g_focus_keep_render=1;
static U32 render_kept,renders_kept;
U32 CDECL kpc_focus_term_render(void *cm,U32 flag){
    typedef U32 (CDECL *TermFn)(void *,U32);U32 result;
    if(g_focus_keep_render&&kpc_keep_running()){
        if(!render_kept&&g_focus_log)g_focus_log("Focus loss: renderer kept running (no shutdown); input released.\r\n");
        render_kept=1;renders_kept++;return 0;
    }
    render_kept=0;
    losing=1;
    result=((TermFn)(g_focus_client+0x53980u))(cm,flag);
    losing=0;
    return result;
}
/* Replaces "call 0x534f0" (r_InitRender, cdecl(cm, mode)) at 0x6c97, the
 * focus-regain restart. 0 = success (the caller shows an error otherwise). */
U32 CDECL kpc_focus_init_render(void *cm,void *mode){
    typedef U32 (CDECL *InitFn)(void *,void *);
    if(render_kept){render_kept=0;return 0;}
    return ((InitFn)(g_focus_client+0x534f0u))(cm,mode);
}
/* Replaces "call 0x287b0" at 0x158e3 and 0x15922: the client world's
 * active-window tests (client object animation 0x52500 and the client
 * object update 0x46e0). Active while the renderer is kept. */
U32 CDECL kpc_focus_world_active(void){
    typedef U32 (CDECL *ActiveFn)(void);
    U32 active=((ActiveFn)(g_focus_client+0x287b0u))();
    return active||render_kept;
}
U32 kpc_focus_renders_kept(void){return renders_kept;}
/* cshell OnEvent 4 / 3 music calls (thiscall, ECX = music manager). */
void __attribute__((thiscall)) kpc_music_pause(void *music){
    if(losing&&kpc_keep_running()){music_skipped=1;return;}
    ((MusicFn)(g_focus_shell+g_shell_layout->music_pause))(music);
}
void __attribute__((thiscall)) kpc_music_resume(void *music){
    if(music_skipped){music_skipped=0;return;}
    ((MusicFn)(g_focus_shell+g_shell_layout->music_resume))(music);
}
/* Replaces "call 0x287b0" at 0x157c5: the local server's active-window test. */
U32 CDECL kpc_focus_active(void){
    typedef U32 (CDECL *ActiveFn)(void);
    U32 active=((ActiveFn)(g_focus_client+0x287b0u))();
    if(!active&&kpc_keep_running()){kept_running++;return 1;}
    return active;
}
U32 kpc_focus_kept_running(void){return kept_running;}
