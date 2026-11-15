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
/* Focus timing diagnostic (2026-11-14 playtest: hundreds of ms of ping right
 * after restoring the window). Measured on the per-frame sound update: the
 * longest gap between two frames while the window was away, the gap before
 * the first frame after it returned, and the time the resync took. Logged
 * for the first FOCUS_REPORTS returns of a session. */
#define FOCUS_REPORTS 8u
static U32 diag_last,diag_away,diag_max_gap,diag_pending,diag_resync_ms,diag_reports;
static U32 diag_lost_at,diag_back_at,diag_frames_away,diag_last_away_frame,diag_before,diag_after;
static void diag_dec(char *b,U32 *at,U32 n){
    char tmp[11];U32 k=0;do{tmp[k++]=(char)('0'+n%10u);n/=10u;}while(n);while(k)b[(*at)++]=tmp[--k];
}
static void diag_text(char *b,U32 *at,const char *s){while(*s)b[(*at)++]=*s++;}
static void diag_lose(void){
    U32 now=GetTickCount();
    diag_away=1;diag_max_gap=0;diag_last=now;diag_lost_at=now;diag_frames_away=0;diag_last_away_frame=now;
}
static void diag_return(void){
    U32 now=GetTickCount();
    if(!diag_away)return;
    diag_away=0;diag_pending=1;diag_resync_ms=0;diag_back_at=now;diag_after=0;
    diag_before=now-diag_last_away_frame;
}
static void diag_frame(void){
    U32 now=GetTickCount(),gap=diag_last?now-diag_last:0;
    if(diag_away){if(gap>diag_max_gap)diag_max_gap=gap;diag_frames_away++;diag_last_away_frame=now;}
    else if(diag_pending&&!diag_after)diag_after=(now-diag_back_at)|0x80000000u;   /* first frame after the return */
    diag_last=now;
    if(diag_pending&&!diag_away&&!resync_pending){
        diag_pending=0;
        if(g_focus_log&&diag_reports<FOCUS_REPORTS){
            char line[240];U32 at=0;diag_reports++;
            diag_text(line,&at,"Focus timing: away ");diag_dec(line,&at,diag_back_at-diag_lost_at);
            diag_text(line,&at," ms, ");diag_dec(line,&at,diag_frames_away);
            diag_text(line,&at," frames (longest gap ");diag_dec(line,&at,diag_max_gap);
            diag_text(line,&at," ms); last frame ");diag_dec(line,&at,diag_before);
            diag_text(line,&at," ms before the window returned, first frame ");diag_dec(line,&at,diag_after&0x7fffffffu);
            diag_text(line,&at," ms after; resync ");diag_dec(line,&at,diag_resync_ms);
            diag_text(line,&at," ms.\r\n");line[at]=0;g_focus_log(line);
        }
    }
}
/* Full speed in the background ([Multiplayer] BackgroundFullSpeed, default 1).
 * The focus timing log (2026-11-14) showed frames slowing to 47-63 ms while
 * the window was away: Windows 11 throttles a minimised or occluded process
 * (EcoQoS, coarse timer resolution, so each Sleep(1) in the client's frame
 * pacing and DDrawCompat's timers takes ~15.6 ms). The process opts out of
 * execution-speed and timer-resolution throttling (SetProcessInformation,
 * ProcessPowerThrottling) and asks for 1 ms timer resolution
 * (timeBeginPeriod(1), kept for the process lifetime). Both are resolved at
 * run time; on older Windows a missing function is skipped. */
typedef struct {U32 version,control,state;} PowerThrottling;
typedef int (WINAPI *SetProcessInfoFn)(void *,int,void *,U32);
typedef U32 (WINAPI *TimePeriodFn)(U32);
typedef void *(WINAPI *CurrentProcessFn)(void);
void kpc_focus_full_speed(void){
    HMODULE kernel=GetModuleHandleW(L"kernel32.dll"),winmm=GetModuleHandleW(L"winmm.dll");
    SetProcessInfoFn set=kernel?(SetProcessInfoFn)GetProcAddress(kernel,"SetProcessInformation"):0;
    TimePeriodFn period;int throttling=0,timer=0;
    if(set){PowerThrottling s={1u,0x1u|0x4u,0u};throttling=set(GetCurrentProcess(),4,&s,sizeof s)!=0;}
    period=winmm?(TimePeriodFn)GetProcAddress(winmm,"timeBeginPeriod"):0;
    if(period)timer=period(1u)==0;
    if(g_focus_log){
        g_focus_log(throttling?"Background: Windows power throttling off for this process (full speed when minimised).\r\n":
                               "Background: power throttling opt-out unavailable on this Windows.\r\n");
        g_focus_log(timer?"Background: 1 ms timer resolution requested.\r\n":"Background: timer resolution unchanged.\r\n");
    }
}

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
static U32 dispatch_mark_handler(void);
U32 __attribute__((thiscall)) kpc_focus_release(void *sound){
    dispatch_mark_handler();
    diag_lose();
    if(kpc_keep_running()){skipped=1;return 1;}
    skipped=0;
    return ((SoundFn)(g_focus_client+0x74b90u))(sound);
}
U32 __attribute__((thiscall)) kpc_focus_restore(void *sound){
    dispatch_mark_handler();
    diag_return();
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
        U32 started=GetTickCount();
        resync_pending=0;
        ((SoundFn)(g_focus_client+0x74c90u))(sound);
        ((SoundFn)(g_focus_client+0x74ce0u))(sound);
        diag_resync_ms=GetTickCount()-started;
        resyncs++;
        if(g_focus_log)g_focus_log("Focus regained: sounds re-synchronised to real time.\r\n");
    }
    diag_frame();
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
 * pause the shutdown triggers as a focus loss.
 *
 * Only while the client is in a world (client shell state shell+0x1398 == 1,
 * the state netwatch.c uses; shell = [cshell + client_shell_global]). At
 * startup, in movies, menus, loading screens and level changes the stock
 * shutdown/restart pair runs: playtest 2026-11-13, a focus change during the
 * startup movies left the screen black until the next minimise/restore,
 * which is the stock restart showing the menu. */
U32 g_focus_keep_render=1;
/* [Multiplayer] KeepRendererEverywhere (default 1, round 6 RF1: "the entire
 * game should act like borderless fullscreen games"): the renderer is kept in
 * every state, also at startup, in the movies, menus and loading screens.
 * With DDrawCompat's borderless mode and AltTabFix = noactivateapp no surface
 * is lost on a focus change. 0 = kept only in a world (the black-screen
 * workaround from 2026-11-13, when the stock surfaces could be lost). */
U32 g_focus_keep_everywhere=1;
U8 *g_focus_shell_slot;        /* cshell global holding the client shell object */
static U32 render_kept,renders_kept;
#define FOCUS_SHELL_STATE 0x1398u
#define FOCUS_STATE_IN_WORLD 1u
static int focus_in_world(void){
    U8 *shell;
    if(!g_focus_shell_slot||!(shell=*(U8 **)g_focus_shell_slot))return 0;
    return *(const U32 *)(shell+FOCUS_SHELL_STATE)==FOCUS_STATE_IN_WORLD;
}
U32 CDECL kpc_focus_term_render(void *cm,U32 flag){
    typedef U32 (CDECL *TermFn)(void *,U32);U32 result;
    if(g_focus_keep_render&&kpc_keep_running()&&(g_focus_keep_everywhere||focus_in_world())){
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

/* Restore stall (two-PC test RC4, 2026-11-14 round 3): the timing line showed
 * no frame for ~400 ms before the window was reactivated and 200-390 ms
 * after, i.e. the main thread is held inside window messages during the
 * restore. The main loop's DispatchMessageA (client.exe 0x69cb, call
 * [0x1d37254]) is wrapped: any message taking >= 50 ms is logged with its id
 * (first DISPATCH_REPORTS of a session). With [Multiplayer] NoWindowAnimation
 * (default 1) the window's minimise/restore animation is turned off once the
 * window exists (DwmSetWindowAttribute DWMWA_TRANSITIONS_FORCEDISABLED = 3;
 * dwmapi.dll is already loaded by DDrawCompat; skipped if it is not). */
#define DISPATCH_REPORTS 16u
#define DISPATCH_SLOW_MS 50u
U32 g_focus_no_animation=1;
static U32 dispatch_reports,animation_done;
static U32 activate_dispatch,activate_handler_at;   /* set by the focus sound hooks during a 0x1c dispatch */
static U32 dispatch_mark_handler(void){
    if(activate_dispatch&&!activate_handler_at)activate_handler_at=GetTickCount()|1u;
    return 0;
}
typedef I32 (WINAPI *DispatchFn)(const void *);
typedef I32 (WINAPI *DwmAttrFn)(void *,U32,const void *,U32);
I32 WINAPI kpc_dispatch(const void *msg){
    U32 started=GetTickCount(),took,id;I32 result;
    if(g_focus_no_animation&&!animation_done&&g_focus_client){
        /* The message's own window, raised to its top-level window (the
         * game's main window; client.exe stores it at [esi+0x44] after
         * CreateWindowExA at 0x68ab, not in a fixed global). */
        void *hwnd=msg?*(void *const *)msg:0;
        if(hwnd){
            typedef void *(WINAPI *AncestorFn)(void *,U32);
            HMODULE user=GetModuleHandleW(L"user32.dll");
            AncestorFn ancestor=user?(AncestorFn)GetProcAddress(user,"GetAncestor"):0;
            void *root=ancestor?ancestor(hwnd,2u):0;      /* GA_ROOT */
            if(root)hwnd=root;
        }
        if(hwnd){
            HMODULE dwm=GetModuleHandleW(L"dwmapi.dll");
            DwmAttrFn set=dwm?(DwmAttrFn)GetProcAddress(dwm,"DwmSetWindowAttribute"):0;
            U32 on=1;animation_done=1;
            if(set&&set(hwnd,3u,&on,4u)==0){if(g_focus_log)g_focus_log("Window: minimise/restore animation off.\r\n");}
            else if(g_focus_log)g_focus_log("Window: animation setting unavailable (dwmapi not loaded).\r\n");
        }
    }
    id=msg?*((const U32 *)msg+1):0;
    if(id==0x1cu){activate_dispatch=1;activate_handler_at=0;}
    result=(*(DispatchFn *)(g_focus_client+0x97254u))(msg);
    took=GetTickCount()-started;
    if(id==0x1cu){
        activate_dispatch=0;
        /* Split of a slow activation: time before the game's own handler
         * reached its sound call (DirectDraw/DDrawCompat activation work)
         * and the rest (the game's handler and anything after it). */
        if(took>=DISPATCH_SLOW_MS&&activate_handler_at&&g_focus_log&&dispatch_reports<DISPATCH_REPORTS){
            char line[160];U32 at=0,before=activate_handler_at-started;
            diag_text(line,&at,"Window activation: ");diag_dec(line,&at,before);
            diag_text(line,&at," ms before the game's handler, ");diag_dec(line,&at,took-before);
            diag_text(line,&at," ms in and after it.\r\n");line[at]=0;g_focus_log(line);
        }
    }
    if(took>=DISPATCH_SLOW_MS&&g_focus_log&&dispatch_reports<DISPATCH_REPORTS){
        char line[96];U32 at=0;dispatch_reports++;
        diag_text(line,&at,"Window message 0x");
        {static const char hex[]="0123456789abcdef";int k;for(k=12;k>=0;k-=4)line[at++]=hex[(id>>k)&15u];}
        diag_text(line,&at," took ");diag_dec(line,&at,took);diag_text(line,&at," ms.\r\n");line[at]=0;g_focus_log(line);
    }
    return result;
}
