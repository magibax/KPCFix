#include "winmini.h"
#include "shell_layout.h"
#include "loading.h"
#include "camera.h"

/* Campaign loading art ([Multiplayer] CampaignLoadingArt, default 1).
 *
 * Evidence (stock cshell 703813a4, static analysis; see
 * coop/CAMPAIGN-ARCHITECTURE.md section 4):
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
 * HOLD_CAP_MS. A new world load drops a stale hold. */
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
void THISCALL kpc_preload_world(void *shell,const char *world){
    const char *why=0;
    art_active=0;close_pending=0;hold_reply=hold_initvars=0;hold_self=0;
    if(!g_loading_enabled)why=" (stock: CampaignLoadingArt=0)";
    else if(!shell)why=" (stock: no shell)";
    else if(!kpc_campaign_world(world))why=" (stock: not a campaign world)";
    else if(!((MultiplayerFn)shell_rva(g_shell_layout->multiplayer_test))(shell))why=" (stock: not multiplayer)";
    else{
        U32 before=art_shown;
        show_art((U8 *)shell,world);
        why=art_shown!=before?" (picture shown)":" (stock: loading screen unavailable)";
    }
    loading_log("Loading world: ",world?world:"?",why);
    /* The original last: it ends music and arms the next world's music flag. */
    ((PreloadFn)shell_rva(g_shell_layout->preload_world))(shell,world);
}
static int art_due(void){return (U32)(GetTickCount()-art_since)>=g_loading_min_ms;}
static int hold_wanted(void){
    U8 *screens;
    if(!g_loading_hold||!g_loading_client||!g_loading_entered||!art_active||art_due()||!art_shell)return 0;
    screens=(U8 *)get32(art_shell+SHELL_SCREENS);
    return screens&&(I32)get32(screens+4)==0;
}
static void hold_release(const char *why){
    void *self=hold_self;U32 reply=hold_reply,initvars=hold_initvars;
    hold_reply=hold_initvars=0;hold_self=0;
    if(reply)((ReplyFn)shell_rva(g_shell_layout->spawn_reply))(self);
    if(initvars)((ReplyFn)shell_rva(g_shell_layout->spawn_initvars))(self);
    loading_log("Loading picture: spawn reply sent",why,0);
}
/* Release check, run every frame from the client update (always, including
 * the menu's connect dialog, where the shell's screen draw does not run) and
 * from the screen draw. */
static void hold_tick(void){
    U8 *screens;
    if(!hold_reply)return;
    screens=art_shell?(U8 *)get32(art_shell+SHELL_SCREENS):0;
    if(screens&&(I32)get32(screens+4)!=0)hold_release(" (picture replaced)");
    else if(!art_active)hold_release(" (picture closed)");
    else if(art_due())hold_release(" (minimum reached)");
    else if((U32)(GetTickCount()-hold_since)>=HOLD_CAP_MS)hold_release(" (safety cap)");
}
/* Replaces the client update's call 0x13876 -> 0x13bb0 (thiscall, one
 * stack argument, ret 4) in client.exe: runs every client frame. */
typedef U32 (THISCALL *ClientFrameFn)(void *,U32);
U32 THISCALL kpc_client_frame(void *client,U32 argument){
    hold_tick();
    kpc_camera_tick();
    return ((ClientFrameFn)(g_loading_client+0x13bb0u))(client,argument);
}
/* Replaces 0xbeadc (call 0xbf300): the client's 0x17 reply. */
void THISCALL kpc_spawn_reply(void *self){
    if(!hold_reply&&hold_wanted()){
        hold_self=self;hold_reply=1;hold_initvars=0;hold_since=GetTickCount();holds++;
        loading_log("Loading picture: spawn reply held until the minimum time has passed",0,0);
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
    if(art_active&&(I32)get32((U8 *)screens+4)==0&&!art_due()){close_pending=1;return;}
    art_active=0;close_pending=0;
    ((CloseFn)shell_rva(g_shell_layout->screen_close))(screens);
}
/* Replaces the per-frame screen draw (0xcdb92). Closes a deferred picture
 * once its minimum time has passed; otherwise draws as before. */
void THISCALL kpc_draw_screen(void *screens,U32 argument){
    hold_tick();
    if(close_pending){
        if((I32)get32((U8 *)screens+4)!=0){close_pending=0;art_active=0;}
        else if(art_due()){
            close_pending=0;art_active=0;
            ((CloseFn)shell_rva(g_shell_layout->screen_close))(screens);
            return;
        }
    }
    ((DrawFn)shell_rva(g_shell_layout->screen_draw))(screens,argument);
}
U32 kpc_loading_shown(void){return art_shown;}
U32 kpc_loading_holds(void){return holds;}
