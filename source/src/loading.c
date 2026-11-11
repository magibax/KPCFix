#include "winmini.h"
#include "shell_layout.h"
#include "loading.h"

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
 * stock behaviour. There is no progress bar: the stock game has none. */
U8 *g_loading_shell;
U32 g_loading_enabled,g_loading_min_ms=5000;
static U32 art_active,art_since,close_pending,art_shown;

#define THISCALL __attribute__((thiscall))
typedef void (THISCALL *PreloadFn)(void *,const char *);
typedef U8 (THISCALL *MultiplayerFn)(void *);
typedef void (THISCALL *ShowFn)(void *,I32);
typedef void (THISCALL *CloseFn)(void *);
typedef void (THISCALL *DrawFn)(void *,U32);
typedef void (THISCALL *FrameFn)(void *);
typedef U32 (CDECL *LoadFn)(const char *);
typedef void (CDECL *FreeFn)(U32);
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
    art_active=1;close_pending=0;art_since=GetTickCount();art_shown++;
    /* Present it now, as the stock 0x46 path (0xcdc80) does, because the
     * client blocks in its world load right after this callback. */
    if(get32(shell+SHELL_STATE)==STATE_CHANGING_LEVELS){
        U8 *render=(U8 *)get32(shell+SHELL_RENDER);
        frames=render?(I32)get32(render+RENDER_FRAMES):0;
        if(frames>MAX_FRAMES)frames=MAX_FRAMES;
        for(i=0;i<frames;i++)((FrameFn)shell_rva(g_shell_layout->render_frame))(shell);
    }
}
void THISCALL kpc_preload_world(void *shell,const char *world){
    art_active=0;close_pending=0;
    if(g_loading_enabled&&shell&&kpc_campaign_world(world)&&
       ((MultiplayerFn)shell_rva(g_shell_layout->multiplayer_test))(shell))
        show_art((U8 *)shell,world);
    /* The original last: it ends music and arms the next world's music flag. */
    ((PreloadFn)shell_rva(g_shell_layout->preload_world))(shell,world);
}
static int art_due(void){return (U32)(GetTickCount()-art_since)>=g_loading_min_ms;}
/* Replaces the 0x69 handler's close (0xcbba6). ECX = screen manager. */
void THISCALL kpc_close_on_entry(void *screens){
    if(art_active&&(I32)get32((U8 *)screens+4)==0&&!art_due()){close_pending=1;return;}
    art_active=0;close_pending=0;
    ((CloseFn)shell_rva(g_shell_layout->screen_close))(screens);
}
/* Replaces the per-frame screen draw (0xcdb92). Closes a deferred picture
 * once its minimum time has passed; otherwise draws as before. */
void THISCALL kpc_draw_screen(void *screens,U32 argument){
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
