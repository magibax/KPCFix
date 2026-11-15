#include "winmini.h"
#include "shell_layout.h"
#include "loading.h"
#include "camera.h"

/* Campaign loading art ([Multiplayer] CampaignLoadingArt, default 1).
 *
 * Evidence (stock cshell 703813a4, static analysis; see
 * coop/ENGINE-NOTES.md, "Loading and connection"):
 *  - single player draws the original per-stage picture through loader
 *    0xad7b0 (char *world -> HSURFACE; keeps the R<d>M<digits> prefix of the
 *    file name, "screens/LoadingBackgrounds/%s.pcx", falls back to
 *    menu/titlescreen.pcx). Multiplayer never reaches that screen: after
 *    0x46 MID_CHANGING_LEVELS the client sits in state 5 drawing only chat;
 *  - the engine calls the shell's PreLoadWorld (vtbl slot 7, 0xcd360,
 *    thiscall, char *world, ret 4) on the remote-client load path just
 *    before its blocking world load;
 *  - loading screen type 0 (ctor 0xac520, Init 0xac5c0) keeps its picture
 *    surface at screen+0xc and frees it in Term (ILTClient+0x158). The
 *    screen manager (shell+0x138c) holds the screen at +0 and its type at +4;
 *    Show 0xad1b0(mgr, type) and Close 0xad3c0(mgr) are thiscall;
 *  - 0x69 MID_CAN_RENDER (0xcbb86) closes a type 0 or 2 screen on entry
 *    (call 0xad3c0 at 0xcbba6). 0x1a closes only types -1 and 2;
 *  - the shell draws an active screen every frame through 0xad380(mgr, arg)
 *    (call at 0xcdb92), in every client state.
 *
 * In multiplayer, for a campaign world (file name R<d>M<d>...), PreLoadWorld
 * switches to loading screen type 0 with the world's original picture (the
 * shell's own loader and renderer; nothing is shipped), renders the frames
 * the stock level-change path renders, and then calls the original. The stock
 * close on entry is deferred until the picture has been visible for at least
 * CampaignLoadingMinimumMs of monotonic real time (GetTickCount); a slower
 * load adds nothing. Deathmatch worlds (PM01...) and single player keep the
 * stock behaviour. There is no progress bar: the stock game has none.
 *
 * Spawn hold ([Multiplayer] CampaignLoadingHoldSpawn, default 1). The server
 * creates the player only when the client answers its 0x17 with the client's
 * own 0x17 (name, settings) and 0x15 INITVARS (shell handler 0xbea70: calls
 * 0xbeadc -> 0xbf300 and 0xbeae3 -> 0xbf400; both thiscall on the handler's
 * ESI, no stack arguments, reading only shell globals). Without the hold a
 * fast load put the player in the world, controllable and attackable, behind
 * the picture. While the picture is up and its minimum has not passed, both
 * replies are recorded and sent, in order, from the per-frame draw hook once
 * the minimum has passed, the picture was closed or replaced, or after
 * HOLD_CAP_MS. A new world load drops a stale hold, and so does a new
 * connection (g_loading_entered cleared by the "Got ID packet" print): the
 * held reply belonged to the server that is gone and is not sent.
 *
 * Synchronised load ([Multiplayer] CampaignLoadingSync, default 1). Server
 * Tools (CoopSyncLoad) sends message 0x70 before a co-op change to a campaign
 * world and 0x71 once every member's client has entered it. Both ids fall to
 * the stock OnMessage default (0xcbe35, "unhandled message"), so the shell's
 * OnMessage (vtable slot 0xff734 -> 0xcad30, thiscall, id, message, ret 8) is
 * wrapped to take them. For such a load the spawn reply is held until 0x71
 * (not just the minimum time; SYNC_CAP_MS at most), so every player enters
 * the world together; the picture then closes on the server's 0x69 at once,
 * and the world's sounds are paused while the reply is held (the client is
 * already in the world behind the picture) and resumed when it is sent. */
U8 *g_loading_shell;
U32 g_loading_enabled,g_loading_min_ms=5000,g_loading_hold;
void (*g_loading_log)(const char *);
static U32 art_active,art_since,close_pending,art_shown;
static void *hold_self;static U8 *art_shell;static U32 hold_reply,hold_initvars,hold_since,holds;
U8 *g_loading_client;
/* The hold applies only to world changes after this connection has entered a
 * world (0x69). On the first load from the server browser the menu's
 * "Connecting..." dialog stays up and the new world's ambient sound is heard
 * behind it for the whole hold, so the first entry is never held. A new
 * connection is recognised at the client's "Got ID packet" print
 * (client.exe 0x6986e -> console print 0x1c8d0). */
U32 g_loading_entered,g_id_print_target;
void kpc_new_connection_print(void);
__asm__(".global kpc_new_connection_print\n"
        "kpc_new_connection_print:\n"
        " movl $0, g_loading_entered\n"
        " jmp *g_id_print_target\n");
#define HOLD_CAP_MS 30000u
static U8 *shell_rva_ptr(U32 rva);
/* Clean music on multiplayer level changes ([Multiplayer] CleanMusic, default 1).
 * cshell 703813a4 (music manager [0x201d2788]): a music start or intensity
 * change stops the song and plays a newly composed segment from bar 1
 * (d9730); 0x46 MID_CHANGING_LEVELS (handler call 0xcb47d -> 0xcdc80) leaves
 * the old song playing through the load; on entering a world the multiplayer
 * client unloads every server-loaded song (0xc8622 -> d96f0(mgr, 1)) while
 * its stop still waits for the bar line. Each server start in between was a
 * new random composition: bars mashed together around every load. Now the
 * single-player sequence (StartGame cd030 and entry c8636) is reproduced:
 *  - at 0x46: d96f0(mgr, 1), the stop-and-unload single player does at
 *    cd088 (the stop waits for the bar while the instruments are unloaded at
 *    once: the note that hangs through the load), then the hold d9d00
 *    (cd093), so the world-start READY / INTENSITY the server sends during
 *    the load are stored, not played;
 *  - at entry the stock multiplayer wipe (0xc8622) is skipped after such a
 *    change (it would unload what arrived during the load and drop the held
 *    values); an entry without 0x46 keeps it, then holds;
 *  - when the loading picture closes (at once if there is none, which is the
 *    single-player moment, c8636) the held state is applied (d9d20). */
U32 g_clean_music=1;
U32 g_min_update_rate=60;
int kpc_remote_multiplayer(void);
static U32 music_held,music_wiped;
static U32 intro_sounds;   /* R?M0: sounds kept paused until the picture closes (below) */
/* Level change picture ([Multiplayer] LevelChangePicture, default 1). After
 * 0x46 MID_CHANGING_LEVELS the stock handler (0xcdc80) only flips the two
 * back buffers, which still hold the last frames, during the server's
 * blocking load: a skipped intro (no fade to black) showed two different
 * frames alternating for 1-2 s (two-PC test, 2026-11-14). Server Tools'
 * 0x70 now carries the next world's path; for a campaign world its loading
 * picture is shown at once (show_art renders it in state 5), otherwise the
 * frames are cleared to black (0xcd910 clear + 0xcd990 render). */
U32 g_level_picture=1;
static char next_world[64];
/* 4:3 loading pictures (with [AspectRatio] Cinematics4x3): the loading
 * screens draw their picture with ScaleSurfaceToSurface (api+0x18c, cdecl
 * (dest, src, destRect, srcRect)) at 0xad978 (single player, type 5) and
 * 0xac6a8 (multiplayer, type 0) over the whole screen. The call is replaced:
 * the screen is filled black (an 8x8 black surface scaled over it: api+0x154
 * create, +0x1ac fill, +0x158 delete) and the picture scaled into the centred
 * 4:3 rectangle (kpc_fit43). */
U32 g_loading_43;
typedef struct {I32 left,top,right,bottom;} LoadRect;
U32 CDECL kpc_fit43(U32 width,U32 height,LoadRect *out);
typedef U32 (CDECL *ScaleFn)(void *,void *,const LoadRect *,const LoadRect *);
typedef void *(CDECL *CreateSurfFn)(U32,U32);
typedef U32 (CDECL *FillFn)(void *,const LoadRect *,U32);
typedef U32 (CDECL *DeleteSurfFn)(void *);
typedef U32 (CDECL *SurfDimsFn)(void *,U32 *,U32 *);
/* Smooth loading pictures ([AspectRatio] LoadingPictureFilter, default 1):
 * the stock stretch is a point-sampled DirectDraw Blt from the picture's
 * system-memory surface to the back buffer. Instead the picture is resampled
 * once with the GDI halftone filter the movies use (cinematic.c) into an
 * engine surface of the target size (api+0x154 CreateSurface; the
 * IDirectDrawSurface is the first field of the renderer surface at
 * HSURFACE+0xc), which is then drawn 1:1 every frame. Only the loading
 * pictures go through here; anything that fails falls back to the stock
 * stretch. The cached copy is rebuilt when the picture surface changes or
 * after a pause of more than a second in drawing it. */
U32 g_loading_filter=1;
typedef struct {I32 left,top,right,bottom;} HalfRect;
U32 CDECL kpc_halftone(void *dest,const HalfRect *dst,void *src,const HalfRect *r);
static struct {void *surf,*src;U32 handle,w,h,last;LoadRect from;} smooth;
static U32 smooth_built,smooth_failed;
static void *dd_surface(void *hsurf){
    U8 *ren=hsurf?*(U8 **)((U8 *)hsurf+0xcu):0;
    return ren?*(void **)ren:0;
}
static void *smooth_picture(const U8 *api,void *src,const LoadRect *sr,U32 tw,U32 th){
    LoadRect from;U32 sw=0,sh=0,handle=src?*(const U32 *)((const U8 *)src+0xcu):0,now=GetTickCount();
    if(!g_loading_filter||!src||!handle||!tw||!th||tw>4096u||th>4096u)return 0;
    if(sr)from=*sr;
    else{((SurfDimsFn)*(const U32 *)(api+0x174u))(src,&sw,&sh);from.left=0;from.top=0;from.right=(I32)sw;from.bottom=(I32)sh;}
    if(from.right<=from.left||from.bottom<=from.top)return 0;
    if((U32)(from.right-from.left)==tw&&(U32)(from.bottom-from.top)==th)return 0;      /* no scaling: nothing to smooth */
    if(smooth.surf&&smooth.src==src&&smooth.handle==handle&&smooth.w==tw&&smooth.h==th&&
       smooth.from.left==from.left&&smooth.from.top==from.top&&smooth.from.right==from.right&&smooth.from.bottom==from.bottom&&
       (U32)(now-smooth.last)<=1000u){smooth.last=now;return smooth.surf;}
    if(smooth.surf&&(smooth.w!=tw||smooth.h!=th)){((DeleteSurfFn)*(const U32 *)(api+0x158u))(smooth.surf);smooth.surf=0;}
    if(!smooth.surf)smooth.surf=((CreateSurfFn)*(const U32 *)(api+0x154u))(tw,th);
    smooth.src=0;
    if(!smooth.surf)return 0;
    {
        void *to=dd_surface(smooth.surf),*in=dd_surface(src);HalfRect d,s;
        d.left=0;d.top=0;d.right=(I32)tw;d.bottom=(I32)th;s.left=from.left;s.top=from.top;s.right=from.right;s.bottom=from.bottom;
        if(!to||!in||!kpc_halftone(to,&d,in,&s)){
            if(!smooth_failed++&&g_loading_log)g_loading_log("Loading pictures: smooth scaling unavailable (GDI on the surfaces failed); stock scaling.\r\n");
            ((DeleteSurfFn)*(const U32 *)(api+0x158u))(smooth.surf);smooth.surf=0;return 0;
        }
    }
    smooth.src=src;smooth.handle=handle;smooth.w=tw;smooth.h=th;smooth.from=from;smooth.last=now;
    if(!smooth_built++&&g_loading_log)g_loading_log("Loading pictures: smooth scaling (halftone) in use.\r\n");
    return smooth.surf;
}
U32 CDECL kpc_loading_blit(void *dest,void *src,const void *drv,const void *srv){
    const LoadRect *dr=(const LoadRect *)drv,*sr=(const LoadRect *)srv;
    const U8 *api=*(U8 *const *)(g_loading_shell+g_shell_layout->enemy_api);
    ScaleFn scale;LoadRect full,fit;U32 w=0,h=0;void *smoothed;
    if(!api)return 0;
    scale=(ScaleFn)*(const U32 *)(api+0x18cu);
    if(!g_loading_43){
        LoadRect all;
        if(dr)all=*dr;
        else{((SurfDimsFn)*(const U32 *)(api+0x174u))(dest,&w,&h);all.left=0;all.top=0;all.right=(I32)w;all.bottom=(I32)h;}
        if(all.right>all.left&&all.bottom>all.top&&(smoothed=smooth_picture(api,src,sr,(U32)(all.right-all.left),(U32)(all.bottom-all.top)))!=0)
            return scale(dest,smoothed,&all,0);
        return scale(dest,src,dr,sr);
    }
    if(dr)full=*dr;
    else{((SurfDimsFn)*(const U32 *)(api+0x174u))(dest,&w,&h);full.left=0;full.top=0;full.right=(I32)w;full.bottom=(I32)h;}
    if(full.right<=full.left||full.bottom<=full.top||!kpc_fit43((U32)(full.right-full.left),(U32)(full.bottom-full.top),&fit))return scale(dest,src,dr,sr);
    fit.left+=full.left;fit.right+=full.left;fit.top+=full.top;fit.bottom+=full.top;
    if(fit.left==full.left&&fit.right==full.right&&fit.top==full.top&&fit.bottom==full.bottom)return scale(dest,src,dr,sr);
    {
        void *black=((CreateSurfFn)*(const U32 *)(api+0x154u))(8u,8u);
        if(black){
            ((FillFn)*(const U32 *)(api+0x1acu))(black,0,0u);
            scale(dest,black,&full,0);
            ((DeleteSurfFn)*(const U32 *)(api+0x158u))(black);
        }
    }
    if((smoothed=smooth_picture(api,src,sr,(U32)(fit.right-fit.left),(U32)(fit.bottom-fit.top)))!=0)return scale(dest,smoothed,&fit,0);
    return scale(dest,src,&fit,sr);
}
/* Single-player loading picture minimum ([KPCFix] SinglePlayerLoadingMs,
 * default 5000, 0 = stock). Single player shows the per-stage picture (screen
 * type 5, Show at 0xcb8d7 in the 0x19 MID_PLAYER_EXITLEVEL handler, picture
 * screens/LoadingBackgrounds/R#M#.pcx) and OnEnterWorld closes it (0xc7d8a)
 * as soon as the fast load ends, so it flashed by. Now:
 *  - after Show(5) the frames are presented at once (the load blocks next);
 *  - the close at entry is skipped until the minimum has passed, the picture
 *    is redrawn every frame (render+0x218 dirty), and the unpause OnEnterWorld
 *    sends (c6f30 -> cd7b0(0,0) at 0xc6f89) becomes a pause request instead,
 *    so the world waits behind the picture; the server's 0x41 "paused" answer
 *    (which would open the in-game menu) is swallowed;
 *  - at the minimum the picture closes and the game is unpaused. */
U32 g_sp_loading_ms=5000;
static U32 sp_art,sp_since,sp_hold,sp_paused,sp_echo41;
typedef void (__attribute__((thiscall)) *PauseFn)(void *,U32,U32);
static U8 *client_shell(void){
    return g_loading_shell&&g_shell_layout->client_shell_global?*(U8 **)(g_loading_shell+g_shell_layout->client_shell_global):0;
}
static void sp_release(const char *why){
    U8 *shell=client_shell(),*mgr;
    if(!sp_art)return;
    sp_art=0;
    if(shell&&(mgr=*(U8 **)(shell+0x138cu))!=0&&*(const I32 *)(mgr+4u)==5)
        ((void (__attribute__((thiscall)) *)(void *))(g_loading_shell+g_shell_layout->screen_close))(mgr);
    if(sp_paused&&shell){sp_paused=0;((PauseFn)(g_loading_shell+g_shell_layout->pause_request))(shell,0,0);}
    sp_hold=0;
    if(g_loading_log){g_loading_log("Single-player loading picture closed");g_loading_log(why);g_loading_log("\r\n");}
}
static void sp_tick(void){
    U8 *shell,*mgr;
    if(!sp_art)return;
    shell=client_shell();mgr=shell?*(U8 **)(shell+0x138cu):0;
    if(!mgr||*(const I32 *)(mgr+4u)!=5){sp_art=0;sp_hold=0;if(sp_paused&&shell){sp_paused=0;((PauseFn)(g_loading_shell+g_shell_layout->pause_request))(shell,0,0);}return;}
    if(sp_hold&&(U32)(GetTickCount()-sp_since)>=g_sp_loading_ms)sp_release(" (minimum reached)");
    else if((U32)(GetTickCount()-sp_since)>=30000u)sp_release(" (safety cap)");
}
void __attribute__((thiscall)) kpc_sp_show(void *mgr,I32 type){
    U8 *shell=client_shell();
    ((void (__attribute__((thiscall)) *)(void *,I32))(g_loading_shell+g_shell_layout->screen_show))(mgr,type);
    if(!g_sp_loading_ms||type!=5||!shell||((U8 (__attribute__((thiscall)) *)(void *))(g_loading_shell+g_shell_layout->multiplayer_test))(shell))return;
    sp_art=1;sp_since=GetTickCount();sp_hold=0;sp_paused=0;
    {U8 *render=*(U8 **)(shell+0x1388u);I32 frames=render?*(const I32 *)(render+0x21cu):0,i;if(frames>8)frames=8;
     for(i=0;i<frames;i++)((void (__attribute__((thiscall)) *)(void *))(g_loading_shell+g_shell_layout->render_frame))(shell);}
}
void __attribute__((thiscall)) kpc_sp_enter_close(void *mgr){
    if(sp_art&&mgr&&*(const I32 *)((U8 *)mgr+4u)==5&&(U32)(GetTickCount()-sp_since)<g_sp_loading_ms){
        sp_hold=1;
        if(g_loading_log)g_loading_log("Single-player loading picture: kept up until its minimum time (game paused meanwhile)\r\n");
        return;
    }
    sp_art=0;
    ((void (__attribute__((thiscall)) *)(void *))(g_loading_shell+g_shell_layout->screen_close))(mgr);
}
void __attribute__((thiscall)) kpc_sp_unpause(void *shell,U32 pause,U32 extra){
    if(sp_hold&&!pause){
        if(!sp_paused){sp_paused=1;sp_echo41++;((PauseFn)(g_loading_shell+g_shell_layout->pause_request))(shell,1,0);}
        return;
    }
    if(sp_hold&&pause)sp_release(" (menu)");
    ((PauseFn)(g_loading_shell+g_shell_layout->pause_request))(shell,pause,extra);
}
static void show_art(U8 *shell,const char *world);
int kpc_campaign_world(const char *world);
typedef void (__attribute__((thiscall)) *MusicFn0)(void *);
typedef void (__attribute__((thiscall)) *MusicFn1)(void *,U32);
static void *music_mgr(void){
    return g_loading_shell&&g_shell_layout->music_global?*(void **)shell_rva_ptr(g_shell_layout->music_global):0;
}
static void music_apply(const char *why){
    void *mgr;
    if(!music_held)return;
    music_held=0;
    if((mgr=music_mgr())!=0)((MusicFn0)shell_rva_ptr(g_shell_layout->music_apply))(mgr);
    if(g_loading_log){g_loading_log("Music: started for the world");g_loading_log(why);g_loading_log("\r\n");}
}
/* Music kept on death ([Multiplayer] KeepMusicOnDeath, default 1). The
 * player-state handler (cshell 0xcac40) sets the music intensity to 1 itself
 * when the local player enters state 3 (shell+0x1394; call 0xcacb1 ->
 * 0xd99a0, thiscall(mgr, 1)), so dying in a boss fight (R1M4C) dropped the
 * boss music for that player. In multiplayer the call is skipped: the music
 * follows the server (an everyone-dead restart reloads the world and its
 * music); single player keeps the stock behaviour. */
U32 g_keep_music_on_death=1;
static U32 death_music_skips;
void __attribute__((thiscall)) kpc_death_music(void *mgr,U32 level){
    U8 *shell=client_shell();
    if(g_keep_music_on_death&&shell&&((U8 (__attribute__((thiscall)) *)(void *))shell_rva_ptr(g_shell_layout->multiplayer_test))(shell)){
        if(!death_music_skips++&&g_loading_log)g_loading_log("Music: kept on death (multiplayer; the stock intensity drop skipped).\r\n");
        return;
    }
    ((MusicFn1)shell_rva_ptr(g_shell_layout->music_intensity))(mgr,level);
}
/* Replaces the 0x46 handler's call (0xcb47d -> 0xcdc80, thiscall, 1 argument). */
/* Voice lines stop at a level change ([Multiplayer] StopSoundsOnLevelChange,
 * default 1). A server-side kill made in the frame of the world change never
 * reaches the clients (the server frees the world's sounds silently), so
 * narration that was playing (Madam Raven in a skipped R?M0 intro, Pablo at
 * the start of R1M2B) went on through the loading picture until the new
 * world's preload. At 0x46 every server-created sound instance is stopped:
 * the sound manager (client.exe [0xaacd0]+0x284; instances +0x330, count
 * +0x530, active +0x120) kills each instance without the client-made flag
 * ([inst+0x14] & 0x100) through 0x73a20 (thiscall(sm, inst), which moves the
 * last instance into the freed slot and clears the server-ID entry). The
 * shell's own sounds and the music (DirectMusic, CleanMusic) are untouched.
 * Multiplayer only. */
U32 g_stop_sounds_on_change=1;
typedef void (__attribute__((thiscall)) *KillInstFn)(void *,void *);
static U32 stop_server_sounds(void){
    U8 *cm,*sm;U32 i=0,killed=0,guard=0;
    if(!g_loading_client||!(cm=*(U8 **)(g_loading_client+0xaacd0u)))return 0;
    sm=cm+0x284u;
    if(!*(const U32 *)(sm+0x120u))return 0;
    while(i<*(const U32 *)(sm+0x530u)&&i<128u&&guard++<256u){
        U8 *inst=*(U8 **)(sm+0x330u+4u*i);
        if(inst&&!(*(const U32 *)(inst+0x14u)&0x100u)){((KillInstFn)(g_loading_client+0x73a20u))(sm,inst);killed++;continue;}   /* slot refilled */
        i++;
    }
    return killed;
}
void __attribute__((thiscall)) kpc_change_levels(void *shell,U32 arg){
    void *mgr;
    ((MusicFn1)shell_rva_ptr(g_shell_layout->change_level))(shell,arg);
    if(g_stop_sounds_on_change&&shell&&((U8 (__attribute__((thiscall)) *)(void *))shell_rva_ptr(g_shell_layout->multiplayer_test))(shell)){
        U32 killed=stop_server_sounds();
        if(killed&&g_loading_log)g_loading_log("Level change: the old world's sounds stopped (voice lines do not run on into the loading picture).\r\n");
    }
    if(g_level_picture&&shell){
        U8 *s=(U8 *)shell;
        if(g_loading_enabled&&next_world[0]&&kpc_campaign_world(next_world)&&
           ((U8 (__attribute__((thiscall)) *)(void *))shell_rva_ptr(g_shell_layout->multiplayer_test))(shell)){
            show_art(s,next_world);
            if(g_loading_log){g_loading_log("Level change: loading picture shown at once for ");g_loading_log(next_world);g_loading_log("\r\n");}
        }else if(g_shell_layout->render_clear){
            U8 *render=*(U8 **)(s+0x1388u);I32 frames=render?*(const I32 *)(render+0x21cu):0,i;
            if(frames>8)frames=8;
            for(i=0;i<frames;i++){
                ((void (__attribute__((thiscall)) *)(void *))shell_rva_ptr(g_shell_layout->render_clear))(shell);
                ((void (__attribute__((thiscall)) *)(void *))shell_rva_ptr(g_shell_layout->render_frame))(shell);
            }
        }
    }
    next_world[0]=0;
    if(!g_clean_music||!(mgr=music_mgr()))return;
    ((MusicFn1)shell_rva_ptr(g_shell_layout->music_unload))(mgr,1);
    ((MusicFn0)shell_rva_ptr(g_shell_layout->music_hold))(mgr);
    music_held=1;music_wiped=1;
}
#define SYNC_CAP_MS 75000u   /* the server goes on its own after 60 s */
U32 g_loading_sync=1;
static U32 sync_armed,sync_load,sync_go,sounds_paused;

#define THISCALL __attribute__((thiscall))
typedef void (THISCALL *PreloadFn)(void *,const char *);
typedef U8 (THISCALL *MultiplayerFn)(void *);
typedef void (THISCALL *ShowFn)(void *,I32);
typedef void (THISCALL *CloseFn)(void *);
typedef void (THISCALL *DrawFn)(void *,U32);
typedef void (THISCALL *FrameFn)(void *);
typedef U32 (CDECL *LoadFn)(const char *);
typedef void (CDECL *FreeFn)(U32);
typedef void (THISCALL *ReplyFn)(void *);
#define SHELL_SCREENS 0x138cu
#define SHELL_RENDER 0x1388u
#define SHELL_STATE 0x1398u
#define RENDER_FRAMES 0x21cu
#define STATE_CHANGING_LEVELS 5u
#define MAX_FRAMES 8

static U32 get32(const U8 *p){return *(const U32 *)p;}
static U8 *shell_rva(U32 rva){return g_loading_shell+rva;}
static U8 *shell_rva_ptr(U32 rva){return g_loading_shell+rva;}

/* The stock name rule (0xad710): file name starts r/R, digit, m/M, digit. */
int kpc_campaign_world(const char *world){
    const char *name=world;
    if(!world)return 0;
    for(;*world;world++)if(*world=='\\'||*world=='/'||*world==':')name=world+1;
    return (name[0]=='r'||name[0]=='R')&&name[1]>='0'&&name[1]<='9'&&
           (name[2]=='m'||name[2]=='M')&&name[3]>='0'&&name[3]<='9';
}
static void show_art(U8 *shell,const char *world){
    U8 *screens=(U8 *)get32(shell+SHELL_SCREENS),*screen,*api;U32 picture,old;I32 frames,i;
    if(!screens)return;
    if((I32)get32(screens+4)!=0)((ShowFn)shell_rva(g_shell_layout->screen_show))(screens,0);
    if((I32)get32(screens+4)!=0||!(screen=(U8 *)get32(screens)))return;
    picture=((LoadFn)shell_rva(g_shell_layout->load_background))(world);
    api=(U8 *)get32(shell_rva(g_shell_layout->enemy_api));
    if(picture&&api){
        old=get32(screen+0xc);*(U32 *)(screen+0xc)=picture;
        if(old&&old!=picture)((FreeFn)get32(api+0x158))(old);
    }
    art_active=1;close_pending=0;art_since=GetTickCount();art_shown++;art_shell=shell;
    /* Present it now, as the stock 0x46 path (0xcdc80) does, because the
     * client blocks in its world load right after this callback. */
    if(get32(shell+SHELL_STATE)==STATE_CHANGING_LEVELS){
        U8 *render=(U8 *)get32(shell+SHELL_RENDER);
        frames=render?(I32)get32(render+RENDER_FRAMES):0;
        if(frames>MAX_FRAMES)frames=MAX_FRAMES;
        for(i=0;i<frames;i++)((FrameFn)shell_rva(g_shell_layout->render_frame))(shell);
    }
}
static void loading_log(const char *a,const char *b,const char *c){
    if(!g_loading_log)return;
    g_loading_log(a);if(b)g_loading_log(b);if(c)g_loading_log(c);g_loading_log("\r\n");
}
/* World sounds while a synchronised hold keeps this player behind the picture:
 * the engine's PauseAll / ResumeAll on the sound manager (client.exe 0x74c90 /
 * 0x74ce0, thiscall, ECX = [0xaacd0]+0x284), the pair focus.c also uses. */
typedef U32 (THISCALL *SoundAllFn)(void *);
static void hold_sounds(int pause){
    U8 *cm;
    if(!g_loading_client||(pause?sounds_paused:!sounds_paused))return;
    if(!(cm=*(U8 **)(g_loading_client+0xaacd0u)))return;
    ((SoundAllFn)(g_loading_client+(pause?0x74c90u:0x74ce0u)))(cm+0x284u);
    sounds_paused=pause!=0;
}
void THISCALL kpc_preload_world(void *shell,const char *world){
    const char *why=0;
    art_active=0;close_pending=0;hold_reply=hold_initvars=0;hold_self=0;
    sync_load=0;sync_go=0;intro_sounds=0;hold_sounds(0);
    if(!g_loading_enabled)why=" (stock: CampaignLoadingArt=0)";
    else if(!shell)why=" (stock: no shell)";
    else if(!kpc_campaign_world(world))why=" (stock: not a campaign world)";
    else if(!((MultiplayerFn)shell_rva(g_shell_layout->multiplayer_test))(shell))why=" (stock: not multiplayer)";
    else{
        U32 before=art_shown;
        show_art((U8 *)shell,world);
        why=art_shown!=before?" (picture shown)":" (stock: loading screen unavailable)";
    }
    if(art_active&&sync_armed){sync_load=1;why=" (picture shown; waiting for every player)";}
    sync_armed=0;
    loading_log("Loading world: ",world?world:"?",why);
    /* The original last (it does not touch music: see CleanMusic above). */
    ((PreloadFn)shell_rva(g_shell_layout->preload_world))(shell,world);
}
/* R?M0 intro worlds ([Multiplayer] IntroPictureHold, default 1): the loading
 * picture stays up after entry until Server Tools reports that the intro
 * started (message 0x74, sent right after the intro script was started), so
 * no part of the world shows before Madam Raven's narration (R3M0 showed the
 * spawn area for under a second). Safety cap INTRO_HOLD_CAP_MS after the go;
 * only for a synchronised load to a world whose name ends in M0. */
U32 g_intro_picture_hold=1;
#define INTRO_HOLD_CAP_MS 20000u
static U32 intro_hold,intro_go,intro_hold_since,intro_sounds;
/* Round 6 (RF2): the server starts the intro ~50 ms after every client's
 * server-side entry, while the client still needs about a second to enter
 * the world, so the narration was heard over the loading picture. For an R?M0
 * world the client's sound output stays paused (PauseAll, as during the spawn
 * hold) until the picture actually closes; the narration then starts with the
 * first frame of the intro. */
static void hold_sounds(int pause);
static void intro_sound_release(void){
    if(!intro_sounds)return;
    intro_sounds=0;hold_sounds(0);
    loading_log("Loading picture: closed; sounds resume with the intro",0,0);
}
static int intro_world(const char *w){
    U32 n=0;while(w[n])n++;
    return n>=4u&&(w[n-4u]=='R'||w[n-4u]=='r')&&w[n-3u]>='1'&&w[n-3u]<='5'&&(w[n-2u]=='M'||w[n-2u]=='m')&&w[n-1u]=='0';
}
static int intro_waiting(void){
    if(!intro_hold||intro_go)return 0;
    if(!intro_hold_since)return 1;
    if((U32)(GetTickCount()-intro_hold_since)<INTRO_HOLD_CAP_MS)return 1;
    intro_hold=0;loading_log("Loading picture: closed after the intro wait cap (no intro start from the server)",0,0);
    return 0;
}
static int art_due(void){return (U32)(GetTickCount()-art_since)>=g_loading_min_ms;}
static int hold_wanted(void){
    U8 *screens;
    if(!g_loading_hold||!g_loading_client||!g_loading_entered||!art_active||!art_shell)return 0;
    if(sync_load?sync_go!=0:art_due())return 0;
    screens=(U8 *)get32(art_shell+SHELL_SCREENS);
    return screens&&(I32)get32(screens+4)==0;
}
static void hold_release(const char *why){
    void *self=hold_self;U32 reply=hold_reply,initvars=hold_initvars;
    hold_reply=hold_initvars=0;hold_self=0;
    if(reply)((ReplyFn)shell_rva(g_shell_layout->spawn_reply))(self);
    if(initvars)((ReplyFn)shell_rva(g_shell_layout->spawn_initvars))(self);
    if(intro_hold&&art_active&&sounds_paused)intro_sounds=1;   /* R?M0: paused until the picture closes */
    else hold_sounds(0);
    loading_log("Loading picture: spawn reply sent",why,0);
}
/* Release check, run every frame from the client update (always, including
 * the menu's connect dialog, where the shell's screen draw does not run) and
 * from the screen draw. */
static void hold_tick(void){
    U8 *screens;
    if(!hold_reply)return;
    if(!g_loading_entered){   /* a hold only starts after entry, so this is a new connection */
        hold_reply=hold_initvars=0;hold_self=0;
        loading_log("Loading picture: spawn reply dropped (new connection)",0,0);
        return;
    }
    screens=art_shell?(U8 *)get32(art_shell+SHELL_SCREENS):0;
    if(screens&&(I32)get32(screens+4)!=0)hold_release(" (picture replaced)");
    else if(!art_active)hold_release(" (picture closed)");
    else if(sync_load){
        if(sync_go)hold_release(" (server: every player has loaded)");
        else if((U32)(GetTickCount()-hold_since)>=SYNC_CAP_MS)hold_release(" (safety cap; no go from the server)");
    }
    else if(art_due())hold_release(" (minimum reached)");
    else if((U32)(GetTickCount()-hold_since)>=HOLD_CAP_MS)hold_release(" (safety cap)");
}
/* Replaces the client update's call 0x13876 -> 0x13bb0 (thiscall, one
 * stack argument, ret 4) in client.exe: runs every client frame. */
typedef U32 (THISCALL *ClientFrameFn)(void *,U32);
U32 THISCALL kpc_client_frame(void *client,U32 argument){
    hold_tick();
    sp_tick();
    if(g_min_update_rate&&kpc_remote_multiplayer()){
        /* Object updates per second the client asks the server for (client.exe
         * byte 0x1d41adc, "UpdateRate", default 10, clamped 2..60 at 0x432a). */
        U8 *rate=g_loading_client+0xa1adcu;
        if(*rate<g_min_update_rate)*rate=(U8)g_min_update_rate;
    }
    kpc_camera_tick();
    return ((ClientFrameFn)(g_loading_client+0x13bb0u))(client,argument);
}
/* Replaces 0xbeadc (call 0xbf300): the client's 0x17 reply. */
void THISCALL kpc_spawn_reply(void *self){
    if(!hold_reply&&hold_wanted()){
        hold_self=self;hold_reply=1;hold_initvars=0;hold_since=GetTickCount();holds++;
        if(sync_load){hold_sounds(1);loading_log("Loading picture: spawn reply held until every player has loaded (sounds paused)",0,0);}
        else loading_log("Loading picture: spawn reply held until the minimum time has passed",0,0);
        return;
    }
    ((ReplyFn)shell_rva(g_shell_layout->spawn_reply))(self);
}
/* Replaces 0xbeae3 (call 0xbf400): INITVARS follows the held reply. */
void THISCALL kpc_spawn_initvars(void *self){
    if(hold_reply&&self==hold_self){hold_initvars=1;return;}
    ((ReplyFn)shell_rva(g_shell_layout->spawn_initvars))(self);
}
/* Replaces the 0x69 handler's close (0xcbba6). ECX = screen manager. */
void THISCALL kpc_close_on_entry(void *screens){
    g_loading_entered=1;
    if(art_active&&(I32)get32((U8 *)screens+4)==0&&!art_due()&&!(sync_load&&sync_go)){close_pending=1;return;}
    if(art_active&&(I32)get32((U8 *)screens+4)==0&&intro_waiting()){
        if(!intro_hold_since)intro_hold_since=GetTickCount()|1u;
        if(!sounds_paused)hold_sounds(1);
        intro_sounds=1;
        close_pending=1;loading_log("Loading picture: kept until the intro starts (sounds paused)",0,0);return;
    }
    sync_load=0;
    art_active=0;close_pending=0;intro_hold=0;
    ((CloseFn)shell_rva(g_shell_layout->screen_close))(screens);
    intro_sound_release();
    music_apply(" (picture closed)");
}
/* Replaces the per-frame screen draw (0xcdb92). Closes a deferred picture
 * once its minimum time has passed; otherwise draws as before. */
void THISCALL kpc_draw_screen(void *screens,U32 argument){
    hold_tick();
    sp_tick();
    if(sp_art&&(I32)get32((U8 *)screens+4)==5){U8 *shell=client_shell();U8 *render=shell?*(U8 **)(shell+0x1388u):0;if(render)render[0x218]=1;}
    if(close_pending){
        if((I32)get32((U8 *)screens+4)!=0){close_pending=0;art_active=0;intro_sound_release();music_apply(" (picture replaced)");}
        else if(art_due()&&!intro_waiting()){
            close_pending=0;art_active=0;intro_hold=0;sync_load=0;
            ((CloseFn)shell_rva(g_shell_layout->screen_close))(screens);
            intro_sound_release();
            music_apply(" (picture closed)");
            return;
        }
    }
    ((DrawFn)shell_rva(g_shell_layout->screen_draw))(screens,argument);
}
/* Replaces the entry wipe call (0xc8622 -> d96f0, thiscall, 1 argument). */
void THISCALL kpc_enter_music(void *mgr,U32 flag){
    if(g_clean_music&&mgr&&music_wiped){music_wiped=0;music_held=1;}   /* wiped at 0x46 as single player does */
    else{
        ((MusicFn1)shell_rva_ptr(g_shell_layout->music_unload))(mgr,flag);
        if(!g_clean_music||!mgr)return;
        ((MusicFn0)shell_rva_ptr(g_shell_layout->music_hold))(mgr);
        music_held=1;
    }
    if(!art_active)music_apply(" (no loading picture)");
}
/* Shell OnMessage wrapper (slot 0xff734): the synchronised-load messages. */
typedef void (THISCALL *MessageFn)(void *,U32,void *);
void THISCALL kpc_shell_message(void *shell,U32 id,void *message){
    if((id&0xffu)==0x41u&&sp_echo41){sp_echo41--;return;}   /* our own pause behind the loading picture */
    if(g_loading_sync&&(id&0xffu)==0x70u){
        /* Payload (Server Tools since 2026-11-14 round 3): the next world's path. */
        const U8 *api=*(U8 *const *)shell_rva(g_shell_layout->enemy_api);U32 i;
        next_world[0]=0;
        if(api&&message&&*(const U32 *)api&&*(const U32 *)(*(const U8 *const *)api+0x50u)){
            typedef const char *(__attribute__((thiscall)) *ReadStrFn)(const void *,void *);
            const char *w=((ReadStrFn)*(const U32 *)(*(const U8 *const *)api+0x50u))(api,message);
            for(i=0;w&&i+1<sizeof next_world&&w[i]>=32&&w[i]<127;i++)next_world[i]=w[i];
            next_world[i]=0;
        }
        sync_armed=1;sync_go=0;
        intro_hold=g_intro_picture_hold&&intro_world(next_world);intro_go=0;intro_hold_since=0;
        loading_log("Synchronised load: the server asks every player to enter the next world together",0,0);
        return;
    }
    if((id&0xffu)==0x75u){
        /* Server Tools music stop (round 6; the stock FADEOUT 0x53 is a no-op):
         * drop any held READY/INTENSITY (so the picture close does not start
         * it again) and stop the song. */
        void *mgr=music_mgr();
        if(mgr){
            if(music_held)((MusicFn0)shell_rva_ptr(g_shell_layout->music_hold))(mgr);
            ((MusicFn0)shell_rva_ptr(g_shell_layout->music_stop))(mgr);
            loading_log("Music: stopped by the server",0,0);
        }
        return;
    }
    if(g_loading_sync&&(id&0xffu)==0x74u){
        if(intro_hold&&!intro_go)loading_log("Loading picture: the intro started; closing",0,0);
        intro_go=1;
        return;
    }
    if(g_loading_sync&&(id&0xffu)==0x71u){
        sync_go=1;sync_armed=0;
        loading_log("Synchronised load: go (every player has loaded)",0,0);
        hold_tick();
        return;
    }
    ((MessageFn)shell_rva(g_shell_layout->on_message))(shell,id,message);
}
U32 kpc_loading_shown(void){return art_shown;}
U32 kpc_loading_holds(void){return holds;}
