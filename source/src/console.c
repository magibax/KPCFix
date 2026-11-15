#include "winmini.h"
#include "console.h"
#include "shell_layout.h"
#include "focus.h"

/* Window-shard diagnostic ([Multiplayer] LogWindowBreaks, default 1).
 * Playtest: windows throw no shards in multiplayer. cshell 703813a4: the
 * server's CWindow break (object.lto 0x483cc) first clears the window's
 * FLAG_VISIBLE (0x483fd..0x48413), then sends message 0x39 (object, effect
 * type 0x4b, shatter vectors, shard_size...) to every client. The shell's
 * OnMessage case 0x39 (0xcb194) passes it to the effect registry (0x80020,
 * table 0x201bf94c); type 0x4b is the window-shard manager 0x2019e6c0 whose
 * handler (vtable 0xfe184 -> 0x5bd30, thiscall, ret 0xc) builds the shards
 * from the window's world-model polygons. No game-type test is on that path;
 * its only early exit is a null window object ("Tried to break a window that
 * is not in the PVS!", 0x5bdaf, on-screen debug text only). The handler is
 * wrapped to log, for the first breaks of a session, whether the window
 * object was present, so the next playtest shows which case it is.
 *
 * HideBrokenWindow ([Multiplayer], default 1): Server Tools' CoopWindowShards
 * keeps a broken window visible on the server for 200 ms so that remote
 * clients still have it when the shard message arrives; the window then stayed
 * on screen while its shards flew (two-PC test RT8). Once the stock handler
 * has built the shards, the window is hidden on this client at once: the
 * client API's GetObjectFlags [api+0x290] / SetObjectFlags [api+0x294]
 * (cdecl; api = [cshell+0x1c8dfc], as cshell's own flag updates 0x59ab5 /
 * 0x59adc) with FLAG_VISIBLE (bit 0) cleared. The server's own hide follows. */
U32 g_window_watch,g_window_hide;
U8 *g_window_shell;
static U32 window_breaks,window_missing;
typedef void (__attribute__((thiscall)) *WindowFn)(void *,void *,U32,U32);
static void watch_decimal(char *b,U32 *at,U32 n){
    char t[11];U32 k=0;do{t[k++]=(char)('0'+n%10u);n/=10u;}while(n);while(k)b[(*at)++]=t[--k];
}
void __attribute__((thiscall)) kpc_window_break(void *manager,void *object,U32 type,U32 message){
    U32 present=object&&*(const U32 *)object;
    window_breaks++;if(!present)window_missing++;
    if(g_window_watch&&g_focus_log&&window_breaks<=32){
        char line[96];U32 at=0;const char *p="Window break message ";
        while(*p)line[at++]=*p++;
        watch_decimal(line,&at,window_breaks);
        p=present?": window object present, shards built.\r\n":": window object MISSING on this client (no shards; \"not in the PVS\").\r\n";
        while(*p&&at<sizeof line-1)line[at++]=*p++;
        line[at]=0;g_focus_log(line);
    }
    ((WindowFn)(g_window_shell+g_shell_layout->window_break))(manager,object,type,message);
    if(g_window_hide&&present&&g_shell_layout->enemy_api){
        typedef U32 (__attribute__((cdecl)) *GetFlagsFn)(U32);
        typedef void (__attribute__((cdecl)) *SetFlagsFn)(U32,U32);
        const U8 *api=*(U8 *const *)(g_window_shell+g_shell_layout->enemy_api);
        U32 h=*(const U32 *)object;
        if(api&&*(const U32 *)(api+0x290)&&*(const U32 *)(api+0x294))
            ((SetFlagsFn)*(const U32 *)(api+0x294))(h,((GetFlagsFn)*(const U32 *)(api+0x290))(h)&~1u);
    }
}
U32 kpc_window_breaks(void){return window_breaks;}
U32 kpc_window_missing(void){return window_missing;}

/* Console toggle key character ([KPCFix] ConsoleKeyFix, default 1).
 *
 * client.exe 16131ccf (fixed base 0x1ca0000), window procedure 0x6be0:
 *  - WM_KEYDOWN (0x6db6): virtual key 0xC0 (VK_OEM_3) toggles the console
 *    (0x6dc9 calls 0x287a0, which returns [0x1d4a60c]). VK_OEM_3 is the
 *    backtick on US layouts but the apostrophe on UK layouts.
 *  - WM_CHAR (0x6ffa): while the console is open ([0x1d4a608], 0x28760) every
 *    character except 0x60 (backtick, 0x700a) is appended to the input line.
 * So on a UK keyboard the apostrophe that follows the toggle key-down is
 * typed into the console. The key-down is timestamped; the next character
 * within CONSOLE_KEY_WINDOW_MS is dropped, whatever the layout produces. The
 * backtick test is kept. */
U32 g_console_key_fix;

/* Chat typing ([Multiplayer] ChatBlocksActions, default 1).
 * cshell 703813a4: key bindings reach the shell as actions through
 * OnCommandOn (vtable slot 0xff724 -> 0xc9d80, thiscall, int action, ret 4),
 * including developer actions such as NoClip (90) and Invuln (93) that send
 * Server Tools requests, while the multiplayer message editor at shell+0x8c is
 * open (its typing byte +0x12ed, read by 0xd85f0, set by 0xd8600). Typed
 * characters reach the editor through key events, not actions. While the
 * editor is typing, every action is dropped, so letters typed in a message
 * no longer toggle noclip, move or fire. */
U32 g_chat_blocks_actions;
U8 *g_command_shell;
static U32 blocked_actions;
typedef void (__attribute__((thiscall)) *CommandFn)(void *,U32);

/* Scoreboard while the key is held ([Multiplayer] ScoreboardHold, default 1).
 * cshell 703813a4: the Scoreboard action (41, autoexec "AddAction Scoreboard
 * 41", Tab) is a toggle in OnCommandOn (case 0xca64c): in multiplayer and in
 * a world, unless the server's intermission scoreboard (screen type 2) is up,
 * it closes the screen manager's current screen ([shell+0x138c], type at +4)
 * when it is type 4, the key scoreboard (close 0xad3c0), else shows type 4
 * (0xad1b0(4)). Nothing else closes type 4 (0x69 and 0x1a close only types
 * 0/2), so it stayed up until the key was pressed again. OnCommandOff
 * (vtable slot 0xff728 -> 0xcab60, an empty "ret 4") is called by the engine
 * once when a command turns off (client.exe 0x5060). With the setting on,
 * a press never closes it (OnCommandOn returns when type 4 is up) and the
 * release closes it with the stock close. Type 4 is created only by this key
 * (0xca6ac is the only caller passing 4), and its close touches no HUD flag,
 * so the server-driven scoreboard (type 2) is never affected. */
U32 g_scoreboard_hold;
#define ACTION_SCOREBOARD 41u
#define SHELL_SCREENS 0x138cu
#define SCREEN_KEY_SCOREBOARD 4
static U32 scoreboard_closes;
static U8 *key_scoreboard_screens(void *shell){
    U8 *mgr;
    if(!g_scoreboard_hold||!shell||!(mgr=*(U8 **)((U8 *)shell+SHELL_SCREENS)))return 0;
    return *(const I32 *)(mgr+4)==SCREEN_KEY_SCOREBOARD?mgr:0;
}
/* Single-player chat and cheat messages ([Multiplayer] SinglePlayerChat,
 * default 1).
 *  - The chat bar opens in single player too: the Message action's
 *    multiplayer test (call 0xcab80 at 0xca8e8, thiscall(shell), AL) also
 *    passes while the player is in a world (shell+0x1398 == 1).
 *  - Lines starting with "!" are commands, run locally through the game's own
 *    console cheats ([cshell+0x1c8de0] vtable +0x28, thiscall(text); single
 *    player allows cheats): !gimmie / !gimme, !nextarmor, !prevarmor, !class
 *    (next class), !kill, !noclip,
 *    !help. Multiplayer co-op commands (!campaign, !changelevel, !next,
 *    !reset, !goto, ...) are refused with a reply. The editor's done callback
 *    (stored at 0xca9c0, original 0xce110: closes the bar and sends message
 *    0x6f with shell+0x204) is wrapped; a command line is not sent.
 *  - A cheat key used in single player (actions 90-104) adds a chat line
 *    naming it, as multiplayer announces cheats. */
U32 g_sp_chat=1;
static U32 sp_chat_commands;
static U8 *chat_client_shell(void){
    const ShellLayout *L=g_shell_layout;
    return g_command_shell&&L->client_shell_global?*(U8 **)(g_command_shell+L->client_shell_global):0;
}
static int shell_multiplayer(void *shell){
    return ((U8 (__attribute__((thiscall)) *)(void *))(g_command_shell+g_shell_layout->multiplayer_test))(shell)!=0;
}
static void chat_say(const char *text){
    U8 *list=g_command_shell+g_shell_layout->chat_list;
    kpc_chat_add(list,text,8.0f,4u,0u);
}
static void console_run(const char *command){
    U8 *console=*(U8 **)(g_command_shell+g_shell_layout->console_global);
    if(console)((void (__attribute__((thiscall)) *)(void *,const char *))(*(U32 **)console)[0x28u/4u])(console,command);
}
/* Disconnect from the main menu ([Multiplayer] MenuDisconnect, default 1).
 * The stock main menu has no way to leave a server (Escape in a world shows
 * the same menu; there is no "disconnect" console command). While the client
 * is connected to a remote server, the main screen's Quit (click, Enter or Q:
 * all reach the call at 0x21dfb -> quit dialog 0x2b800) disconnects instead
 * through the engine's own ClientDE Disconnect ([cshell+0x1c8dfc] +0x14,
 * cdecl, client.exe 0xbe80: deletes the engine's client shell, whose Term
 * disconnects; cshell OnEvent(1) then shows the stock "You were disconnected
 * from the server." box). Quit again then quits as before. */
U32 g_menu_disconnect=1;
int kpc_remote_multiplayer(void);
void __attribute__((thiscall)) kpc_main_quit(void *menus,U32 type){
    const ShellLayout *L=g_shell_layout;
    if(g_menu_disconnect&&kpc_remote_multiplayer()){
        U8 *api=*(U8 **)(g_command_shell+L->enemy_api);
        void (__attribute__((cdecl)) *disconnect)(void)=api?(void (__attribute__((cdecl)) *)(void))*(U32 *)(api+0x14u):0;
        if(disconnect){
            if(g_focus_log)g_focus_log("Main menu: Quit while connected - disconnecting from the server.\r\n");
            disconnect();
            return;
        }
    }
    ((void (__attribute__((thiscall)) *)(void *,U32))(g_command_shell+L->quit_dialog))(menus,type);
}
U8 __attribute__((thiscall)) kpc_chat_gate(void *shell){
    if(shell_multiplayer(shell))return 1;
    return (U8)(g_sp_chat&&shell&&*(const U32 *)((const U8 *)shell+0x1398u)==1u);
}
static int word_is(const char *a,U32 n,const char *w){
    U32 i;for(i=0;i<n&&w[i];i++){char c=a[i];if(c>='A'&&c<='Z')c=(char)(c+32);if(c!=w[i])return 0;}
    return i==n&&!w[i];
}
static void sp_command(const char *text){
    static const struct {const char *word;const char *console;const char *said;} cmds[]={
        {"gimmie","Gimme","[Cheat] Gimmie: ammo and the next weapon"},{"gimme","Gimme","[Cheat] Gimmie: ammo and the next weapon"},
        {"nextarmor","NextArmor","[Cheat] Next armour piece"},{"nextarmour","NextArmor","[Cheat] Next armour piece"},
        {"prevarmor","PrevArmor","[Cheat] Previous armour piece"},{"prevarmour","PrevArmor","[Cheat] Previous armour piece"},
        {"class","NextPlayerClass","[Cheat] Next class"},{"kill","Kill","You killed yourself"},
        {"noclip","noclip","[Cheat] No clipping toggled"},
    };
    static const char *const mp_only[]={"campaign","changelevel","next","previous","reset","goto","back","coop","status",
        "ready","unready","start","join","leave","difficulty","skill","abort","all","op"};
    const char *p=text+1;U32 n=0,i;
    while(p[n]&&p[n]!=' '&&p[n]!='\t')n++;
    sp_chat_commands++;
    if(word_is(p,n,"help")){chat_say("[Single player] !gimmie !nextarmor !prevarmor !class !kill !noclip");return;}
    for(i=0;i<sizeof cmds/sizeof cmds[0];i++)if(word_is(p,n,cmds[i].word)){console_run(cmds[i].console);chat_say(cmds[i].said);return;}
    for(i=0;i<sizeof mp_only/sizeof mp_only[0];i++)if(word_is(p,n,mp_only[i])){chat_say("[Single player] That command works only on a co-op server (!help lists the single-player ones)");return;}
    chat_say("[Single player] Unknown command (!help)");
}
void __attribute__((cdecl)) kpc_chat_done(void){
    U8 *shell=chat_client_shell();char *text=shell?(char *)(shell+0x204u):0;
    if(g_sp_chat&&text&&text[0]=='!'&&!shell_multiplayer(shell)){
        char line[128];U32 i;
        for(i=0;i+1<sizeof line&&text[i];i++)line[i]=text[i];
        line[i]=0;text[0]=0;                                   /* nothing is sent */
        ((void (__attribute__((cdecl)) *)(void))(g_command_shell+g_shell_layout->chat_done))();
        sp_command(line);
        return;
    }
    ((void (__attribute__((cdecl)) *)(void))(g_command_shell+g_shell_layout->chat_done))();
}
static void sp_cheat_key(void *shell,U32 action){
    static const char *const names[15]={"No clipping","No target","Spectator","Invulnerability","Chase camera",0,0,
        "Next class","Gimmie","Next armour piece","Next armour piece","Previous armour piece","Next monster","Previous monster","Restart level"};
    char line[64];U32 n=0;const char *s;
    if(!g_sp_chat||action<90u||action>104u||!(s=names[action-90u])||shell_multiplayer(shell))return;
    if(action==99u)return;                                     /* unused slot */
    {const char *pre="[Cheat] ";while(*pre)line[n++]=*pre++;}
    while(*s&&n+1<sizeof line)line[n++]=*s++;
    line[n]=0;chat_say(line);
}
void __attribute__((thiscall)) kpc_command_on(void *shell,U32 action){
    const ShellLayout *L=g_shell_layout;
    if(g_chat_blocks_actions&&shell&&*((const U8 *)shell+L->chat_editor+L->chat_editing)){blocked_actions++;return;}
    sp_cheat_key(shell,action);
    if(action==ACTION_SCOREBOARD&&key_scoreboard_screens(shell))return;   /* held: already showing */
    ((CommandFn)(g_command_shell+L->command_on))(shell,action);
}
void __attribute__((thiscall)) kpc_command_off(void *shell,U32 action){
    const ShellLayout *L=g_shell_layout;U8 *mgr;
    typedef void (__attribute__((thiscall)) *CloseFn)(void *);
    if(action==ACTION_SCOREBOARD&&(mgr=key_scoreboard_screens(shell))!=0){
        ((CloseFn)(g_command_shell+L->screen_close))(mgr);scoreboard_closes++;
    }
    ((CommandFn)(g_command_shell+L->command_off))(shell,action);
}
U32 kpc_scoreboard_closes(void){return scoreboard_closes;}

/* Chat editor keys ([Multiplayer] ChatClipboard, default 1).
 * cshell 703813a4: the multiplayer message editor (shell+0x8c, vtable
 * 0x1004c0, used by no other object) receives keys as virtual-key codes
 * through its OnKeyDown (slot 0x1004ec -> 0xd8040, thiscall(vk, flag),
 * ret 8, AL = 0 when handled); WM_CHAR never reaches it, so Ctrl+A arrived as
 * VK 'A' and typed "a". Editor fields: text +0x178 (0x80 bytes, at most 127
 * characters), cursor +0x1244, first visible character +0x1248, "fresh"
 * flag +0x1264 (Backspace clears the line), editing +0x1265, insert mode
 * +0x1266, allowed characters +0x1267 (NUL-terminated); 0xd7aa0 (thiscall)
 * redoes the layout after a change. With Ctrl held (and Alt up, so AltGr
 * still types): Ctrl+A selects the line (the next typed key, Backspace or
 * Delete replaces or clears it), Ctrl+C copies it, Ctrl+X cuts it, Ctrl+V
 * pastes the clipboard's first line (allowed characters only, up to the
 * 127-character limit); other Ctrl+letters type nothing. Clipboard and key
 * state come from USER32 / KERNEL32 by GetProcAddress (no new imports). */
U32 g_chat_clipboard,g_chat_scrollback,g_chat_long,g_chat_history,g_chat_hide_loading;
/* Chat hidden while a loading picture (screen type 0) or a level change
 * (shell state 5) is up ([Multiplayer] ChatHideOnLoading, default 1): the
 * shell draws the list (0xcdba8 -> 0xd5d80) and the bar (0xcdbcf) over the
 * loading picture. The bar stays open with its text and keys are ignored;
 * the list is not drawn, which also pauses its expiry (its update 0xd50c0
 * runs only from that draw); when it shows again the oldest line's timer
 * restarts (+0x14 = now + its duration +0x10) and fading lines are reset. */
static int chat_hidden(void){
    U8 *shell,*mgr;
    if(!g_command_shell||!g_shell_layout->client_shell_global)return 0;
    if(!(shell=*(U8 **)(g_command_shell+g_shell_layout->client_shell_global)))return 0;
    if(*(const U32 *)(shell+0x1398u)==5u)return 1;
    mgr=*(U8 **)(shell+0x138cu);
    return mgr&&*(const I32 *)(mgr+4u)==0;
}
static U32 list_was_hidden;
typedef float (CDECL *TimeFn)(void);
typedef void (__attribute__((thiscall)) *ListDrawFn)(void *,void *);
void __attribute__((thiscall)) kpc_list_draw(void *list,void *surface){
    U8 *l=(U8 *)list;
    if(g_chat_hide_loading&&chat_hidden()){list_was_hidden=1;return;}
    if(list_was_hidden){
        const U8 *api=*(U8 *const *)(g_command_shell+g_shell_layout->enemy_api);U8 *e;U32 guard=0;
        list_was_hidden=0;
        if(api&&*(const U32 *)(api+0x210u)&&(e=*(U8 **)(l+0xcu))!=0){
            float now=((TimeFn)*(const U32 *)(api+0x210u))();
            *(float *)(e+0x14u)=now+*(const float *)(e+0x10u);
            for(;e&&guard<64u;e=*(U8 **)(e+0x138u),guard++)if(e[0x121]){e[0x121]=0;*(float *)(e+0x12cu)=1.0f;}
        }
    }
    ((ListDrawFn)(g_command_shell+g_shell_layout->list_draw))(list,surface);
}
/* Terminal-style history ([Multiplayer] ChatHistory, default 1): Enter keeps
 * the sent line (last CHAT_SENT_MAX, no repeats); Up / Down in the bar step
 * back / forward through them, past the newest the line being typed returns.
 * Stock Up/Down typed '&' and '(' (key_char returns the VK). */
#define CHAT_SENT_MAX 16u
static char chat_sent[CHAT_SENT_MAX][204],chat_draft[204];
static U32 chat_sent_total;static I32 chat_sent_at=-1;
static void chat_history_enter(U8 *ed,int send);
static void chat_history_move(U8 *ed,int back);
/* Chat scrollback ([Multiplayer] ChatScrollback, default 1). The on-screen
 * list (object [cshell+0x1d24b8], 24 entries) deletes the oldest line once
 * it expires or once newer lines no longer fit, so a missed line was gone.
 * Every server message the 0x45 handler adds (call 0xcb441 -> 0xd56d0,
 * thiscall(text, float seconds, flags, colour), ret 0x10) is also kept here
 * (CHAT_HISTORY lines). While the chat box is open, Page Up shows the
 * previous CHAT_PAGE lines again in the normal message area (the stock add,
 * CHAT_REPLAY_SECONDS each) and Page Down the next ones; a new message resets
 * the view. "##" is broken up in the kept copy (the stock add expands
 * "##word" into a 32-byte buffer). */
#define CHAT_HISTORY 64u
#define CHAT_PAGE 6u
#define CHAT_TEXT 256u   /* kept copy; lines are cut at 254 (the list's _snprintf 0xff) */
#define CHAT_REPLAY_SECONDS 8.0f
static struct {char text[CHAT_TEXT];U32 colour;} chat_hist[CHAT_HISTORY];
static U32 chat_hist_total,chat_hist_view,chat_browsing,chat_replays;
typedef void (__attribute__((thiscall)) *ChatAddFn)(void *,const char *,float,U32,U32);
void __attribute__((thiscall)) kpc_chat_add(void *list,const char *text,float seconds,U32 flags,U32 colour){
    if(g_chat_scrollback&&text&&!(flags&0x10u)){
        U32 slot=chat_hist_total%CHAT_HISTORY,i;char prev=0;
        for(i=0;i<254u&&text[i];i++){char c=text[i];if(c=='#'&&prev=='#')c=' ';chat_hist[slot].text[i]=c;prev=c;}
        chat_hist[slot].text[i]=0;chat_hist[slot].colour=colour;
        chat_hist_total++;chat_hist_view=0;chat_browsing=0;
    }
    /* Every client copies a line into a 0x200-byte stack buffer (0xd6600) and
     * expands "##word" into 32 bytes (0xd6520) without checks, and keeps at
     * most 254 characters (_snprintf 0xff, no NUL at 255): a line from any
     * server is passed on at most 254 characters long with "##" broken up. */
    if(text&&(g_chat_long||g_chat_scrollback)){
        static char safe[256];U32 i;char prev=0;int changed=0;
        for(i=0;i<254u&&text[i];i++){char c=text[i];if(c=='#'&&prev=='#'){c=' ';changed=1;}safe[i]=c;prev=c;}
        if(text[i])changed=1;
        safe[i]=0;
        if(changed)text=safe;
    }
    ((ChatAddFn)(g_command_shell+g_shell_layout->chat_add))(list,text,seconds,flags,colour);
}
/* Shows the page that ends chat_hist_view lines before the newest. */
static void chat_replay(void){
    U32 kept=chat_hist_total<CHAT_HISTORY?chat_hist_total:CHAT_HISTORY,end,start,i;
    void *list=g_command_shell+g_shell_layout->chat_list;
    if(!kept||chat_hist_view>=kept)return;
    end=chat_hist_total-chat_hist_view;
    start=end>=CHAT_PAGE?end-CHAT_PAGE:0;
    if(start<chat_hist_total-kept)start=chat_hist_total-kept;
    for(i=start;i<end;i++)((ChatAddFn)(g_command_shell+g_shell_layout->chat_add))(list,chat_hist[i%CHAT_HISTORY].text,CHAT_REPLAY_SECONDS,0,chat_hist[i%CHAT_HISTORY].colour);
    chat_replays++;
}
static int chat_scroll_key(U32 vk){
    U32 kept=chat_hist_total<CHAT_HISTORY?chat_hist_total:CHAT_HISTORY;
    if(!g_chat_scrollback||(vk!=0x21u&&vk!=0x22u))return 0;
    /* The first Page Up shows the newest page (lines that just disappeared),
     * each further one the page before; Page Down goes back towards the newest. */
    if(vk==0x21u){if(!chat_browsing)chat_browsing=1;else if(chat_hist_view+CHAT_PAGE<kept)chat_hist_view+=CHAT_PAGE;}
    else{chat_browsing=1;chat_hist_view=chat_hist_view>=CHAT_PAGE?chat_hist_view-CHAT_PAGE:0;}
    chat_replay();
    return 1;
}
U32 kpc_chat_replays(void){return chat_replays;}
#define ED_TEXT 0x178u
#define ED_CURSOR 0x1244u
#define ED_SCROLL 0x1248u
#define ED_FRESH 0x1264u
#define ED_EDITING 0x1265u
#define ED_INSERT 0x1266u
#define ED_ALLOWED 0x1267u
#define ED_MAX_STOCK 127u
#define ED_MAX_LONG 200u
#define ED_MAX (g_chat_long?ED_MAX_LONG:ED_MAX_STOCK)
typedef short (WINAPI *KeyStateFn)(int);
typedef int (WINAPI *ClipOpenFn)(void *);
typedef int (WINAPI *ClipVoidFn)(void);
typedef void *(WINAPI *ClipGetFn)(U32);
typedef void *(WINAPI *ClipSetFn)(U32,void *);
typedef void *(WINAPI *GlobalAllocFn)(U32,U32);
typedef void *(WINAPI *GlobalLockFn)(void *);
typedef int (WINAPI *GlobalUnlockFn)(void *);
static struct {int ready;KeyStateFn key;ClipOpenFn open;ClipVoidFn close,empty;ClipGetFn get;ClipSetFn set;GlobalAllocFn galloc;GlobalLockFn glock;GlobalUnlockFn gunlock;} clip;
static U32 chat_selected,chat_pastes;
static void clip_resolve(void){
    HMODULE user,kernel;
    if(clip.ready)return;
    clip.ready=1;
    user=GetModuleHandleW(L"user32.dll");kernel=GetModuleHandleW(L"kernel32.dll");
    if(!user||!kernel)return;
    clip.key=(KeyStateFn)GetProcAddress(user,"GetKeyState");
    clip.open=(ClipOpenFn)GetProcAddress(user,"OpenClipboard");
    clip.close=(ClipVoidFn)GetProcAddress(user,"CloseClipboard");
    clip.empty=(ClipVoidFn)GetProcAddress(user,"EmptyClipboard");
    clip.get=(ClipGetFn)GetProcAddress(user,"GetClipboardData");
    clip.set=(ClipSetFn)GetProcAddress(user,"SetClipboardData");
    clip.galloc=(GlobalAllocFn)GetProcAddress(kernel,"GlobalAlloc");
    clip.glock=(GlobalLockFn)GetProcAddress(kernel,"GlobalLock");
    clip.gunlock=(GlobalUnlockFn)GetProcAddress(kernel,"GlobalUnlock");
}
static int key_down(int vk){return clip.key&&(clip.key(vk)&0x8000)!=0;}
typedef void (__attribute__((thiscall)) *EditorFn)(void *);
typedef U8 (__attribute__((thiscall)) *EditorKeyFn)(void *,U32,U32);
static void editor_refresh(U8 *ed){((EditorFn)(g_command_shell+g_shell_layout->chat_layout))(ed);}
static void editor_clear(U8 *ed){
    ed[ED_TEXT]=0;*(U32 *)(ed+ED_CURSOR)=0;*(U32 *)(ed+ED_SCROLL)=0;ed[ED_FRESH]=0;
}
static U32 editor_len(const U8 *ed){U32 n=0;while(n<ED_MAX&&ed[ED_TEXT+n])n++;return n;}
static int editor_allowed(const U8 *ed,char c){
    const char *a=(const char *)(ed+ED_ALLOWED);U32 i;
    if(!a[0])return (unsigned char)c>=32u&&(unsigned char)c<127u;
    for(i=0;i<0x80u&&a[i];i++)if(a[i]==c)return 1;
    return 0;
}
static void chat_copy(const U8 *ed){
    U32 n=editor_len(ed),i;void *mem;char *dst;
    if(!clip.open||!clip.galloc||!clip.glock||!clip.set||!clip.open(0))return;
    if(clip.empty)clip.empty();
    if((mem=clip.galloc(2u,n+1u))!=0&&(dst=(char *)clip.glock(mem))!=0){   /* GMEM_MOVEABLE */
        for(i=0;i<n;i++)dst[i]=(char)ed[ED_TEXT+i];
        dst[n]=0;clip.gunlock(mem);clip.set(1u,mem);                       /* CF_TEXT */
    }
    clip.close();
}
static void chat_paste(U8 *ed){
    void *mem;const char *src;U32 len,cursor,added=0,i;
    if(!clip.open||!clip.get||!clip.glock||!clip.open(0))return;
    if((mem=clip.get(1u))!=0&&(src=(const char *)clip.glock(mem))!=0){
        len=editor_len(ed);cursor=*(U32 *)(ed+ED_CURSOR);if(cursor>len)cursor=len;
        for(i=0;src[i]&&src[i]!='\r'&&src[i]!='\n'&&i<4096u;i++){
            char c=src[i]=='\t'?' ':src[i];U32 k;
            if(!editor_allowed(ed,c))continue;
            if(ed[ED_INSERT]||cursor>=len){
                if(len>=ED_MAX)break;
                for(k=len+1u;k>cursor;k--)ed[ED_TEXT+k]=ed[ED_TEXT+k-1u];
                len++;
            }
            ed[ED_TEXT+cursor++]=(U8)c;added++;
        }
        ed[ED_TEXT+len]=0;*(U32 *)(ed+ED_CURSOR)=cursor;ed[ED_FRESH]=0;
        clip.gunlock(mem);
        if(added){chat_pastes++;editor_refresh(ed);}
    }
    clip.close();
}
static int printable_key(U32 vk){
    return (vk>=0x20u&&vk<=0x5au)||(vk>=0x60u&&vk<=0x6fu)||(vk>=0xbau&&vk<=0xe2u);
}
U8 __attribute__((thiscall)) kpc_chat_key(void *editor,U32 vk,U32 flag){
    U8 *ed=(U8 *)editor;
    if(ed&&flag!=1u&&ed[ED_EDITING]){
        if(g_chat_hide_loading&&chat_hidden())return 0;                /* nothing is typed blind */
        if(vk==0x0du||vk==0x1bu)chat_history_enter(ed,vk==0x0du);
        else if((vk==0x26u||vk==0x28u)&&g_chat_history){chat_history_move(ed,vk==0x26u);return 0;}
    }
    if(ed&&flag!=1u&&ed[ED_EDITING]&&chat_scroll_key(vk))return 0;   /* Page Up / Page Down */
    if(g_chat_clipboard&&ed&&flag!=1u&&ed[ED_EDITING]){
        clip_resolve();
        if(key_down(0x11)&&!key_down(0x12)){                                /* Ctrl, not AltGr */
            switch(vk){
            case 'A':chat_selected=1;ed[ED_FRESH]=1;return 0;
            case 'C':chat_copy(ed);return 0;
            case 'X':chat_copy(ed);editor_clear(ed);chat_selected=0;editor_refresh(ed);return 0;
            case 'V':if(chat_selected){editor_clear(ed);chat_selected=0;}chat_paste(ed);return 0;
            default:if(vk>=0x41u&&vk<=0x5au)return 0;                       /* no stray letters */
            }
        }else if(chat_selected&&vk!=0x10u&&vk!=0x11u&&vk!=0x12u){
            chat_selected=0;
            if(vk==0x08u||vk==0x2eu){editor_clear(ed);editor_refresh(ed);return 0;}   /* Backspace / Delete */
            if(printable_key(vk))editor_clear(ed);                          /* typing replaces the line */
        }
    }
    return ((EditorKeyFn)(g_command_shell+g_shell_layout->chat_key))(editor,vk,flag);
}
U32 kpc_chat_pastes(void){return chat_pastes;}
static void chat_line_set(U8 *ed,const char *s){
    U32 i;
    for(i=0;i<ED_MAX&&s[i];i++)ed[ED_TEXT+i]=(U8)s[i];
    ed[ED_TEXT+i]=0;*(U32 *)(ed+ED_CURSOR)=i;*(U32 *)(ed+ED_SCROLL)=0;ed[ED_FRESH]=0;chat_selected=0;
    editor_refresh(ed);
}
static void chat_history_enter(U8 *ed,int send){
    chat_sent_at=-1;
    if(send&&g_chat_history&&ed[ED_TEXT]){
        const char *last=chat_sent_total?chat_sent[(chat_sent_total-1u)%CHAT_SENT_MAX]:"";U32 i,same=1;
        for(i=0;i<ED_MAX&&(ed[ED_TEXT+i]||last[i]);i++)if((char)ed[ED_TEXT+i]!=last[i]){same=0;break;}
        if(!same){char *dst=chat_sent[chat_sent_total%CHAT_SENT_MAX];for(i=0;i<ED_MAX&&ed[ED_TEXT+i];i++)dst[i]=(char)ed[ED_TEXT+i];dst[i]=0;chat_sent_total++;}
    }
}
static void chat_history_move(U8 *ed,int back){
    U32 kept=chat_sent_total<CHAT_SENT_MAX?chat_sent_total:CHAT_SENT_MAX,i;
    if(!kept)return;
    if(back){
        if(chat_sent_at<0){for(i=0;i<ED_MAX&&ed[ED_TEXT+i];i++)chat_draft[i]=(char)ed[ED_TEXT+i];chat_draft[i]=0;chat_sent_at=0;}
        else if((U32)chat_sent_at+1u<kept)chat_sent_at++;
        chat_line_set(ed,chat_sent[(chat_sent_total-1u-(U32)chat_sent_at)%CHAT_SENT_MAX]);
    }else{
        if(chat_sent_at<0)return;
        if(--chat_sent_at<0)chat_line_set(ed,chat_draft);
        else chat_line_set(ed,chat_sent[(chat_sent_total-1u-(U32)chat_sent_at)%CHAT_SENT_MAX]);
    }
}
/* Label hint: the editor's Init (slot 0x100500 -> 0xd7620, thiscall(label,
 * text, width, charset, own_line), ret 0x14; label = string 0x271 "MSG:") gets
 * the stock label without its ':' plus the keys, and an empty initial text
 * (the stock argument points at an unwritten stack byte). */
typedef void (__attribute__((thiscall)) *InitFn)(void *,const char *,const char *,U32,U32,U32);
/* Chat bar font with real lowercase ([Multiplayer] ChatLowercaseFont, default
 * 1). The bar's menu fonts (menu_normal / menu_bold / menu_mo, editor +0x27c /
 * +0x7b8 / +0xcf4, pointers +0x1230 / +0x1234 / +0x1238) draw a-z as small
 * capitals, so everything looks upper case; nothing in the game converts the
 * text. After the stock Init the three are reloaded from the stock Arial 7
 * fonts the chat messages use (real lowercase, 11 px, identical widths in
 * every colour): white for the text and label, yellow for the Ctrl+A
 * selection (round 6: with only two replaced, the highlight drew in the menu
 * font). Font term 0xb24a0 (thiscall), font init 0xb2550 (thiscall(font, api,
 * [ed+0x278], path, 1), al = 1 on success); if any of them fails, all three
 * go back to the menu fonts so measuring and drawing stay consistent. Then
 * the bar's surfaces are rebuilt (0xd7aa0, which frees the old ones). */
U32 g_chat_lower_font=1;
#define ED_FONT_NORMAL 0x27cu
#define ED_FONT_BOLD 0x7b8u
#define ED_FONT_MO 0xcf4u
#define ED_INTERFACE 0x278u
typedef void (__attribute__((thiscall)) *FontTermFn)(void *);
typedef U8 (__attribute__((thiscall)) *FontInitFn)(void *,U32,U32,const char *,U32);
static U32 chat_font_swaps;
static const char chat_font_white[]="interface/arial7_white.pcf",chat_font_yellow[]="interface/arial7_yellow.pcf";
static void chat_font_swap(U8 *ed){
    const ShellLayout *L=g_shell_layout;U32 api,iface,k,ok=1;
    static const U32 fonts[3]={ED_FONT_NORMAL,ED_FONT_BOLD,ED_FONT_MO};
    const char *const lower[3]={chat_font_white,chat_font_white,chat_font_yellow};
    U32 menu[3];
    if(!g_chat_lower_font||!L->font_init||!L->font_term||!L->chat_layout||!L->font_menu_mo)return;
    menu[0]=L->font_menu_normal;menu[1]=L->font_menu_bold;menu[2]=L->font_menu_mo;
    api=*(const U32 *)(g_command_shell+L->interface_api);iface=*(const U32 *)(ed+ED_INTERFACE);
    if(!api)return;
    for(k=0;k<3u&&ok;k++){
        U8 *font=ed+fonts[k];
        ((FontTermFn)(g_command_shell+L->font_term))(font);
        if(!((FontInitFn)(g_command_shell+L->font_init))(font,api,iface,lower[k],1u))ok=0;
    }
    if(!ok){
        for(k=0;k<3u;k++){
            U8 *font=ed+fonts[k];
            ((FontTermFn)(g_command_shell+L->font_term))(font);
            ((FontInitFn)(g_command_shell+L->font_init))(font,api,iface,(const char *)(g_command_shell+menu[k]),1u);
        }
    }
    ((EditorFn)(g_command_shell+L->chat_layout))(ed);
    if(!chat_font_swaps++&&g_focus_log)g_focus_log(ok?"Chat bar: lowercase fonts (Arial 7 white, yellow selection) loaded.\r\n":"Chat bar: lowercase fonts unavailable; menu fonts kept.\r\n");
}
void __attribute__((thiscall)) kpc_chat_init(void *editor,const char *label,const char *text,U32 width,U32 charset,U32 own_line){
    char hint[96];U32 n=0,i;const char *add=g_chat_history?" (PgUp/Dn, Up/Dn):":g_chat_scrollback?" (PgUp/Dn):":":";
    (void)text;
    for(i=0;label&&label[i]&&n<40u;i++)hint[n++]=label[i];
    while(n&&(hint[n-1u]==':'||hint[n-1u]==' '))n--;
    for(i=0;add[i]&&n+1u<sizeof hint;i++)hint[n++]=add[i];
    hint[n]=0;chat_sent_at=-1;
    ((InitFn)(g_command_shell+g_shell_layout->chat_init))(editor,hint,"",width,charset,own_line);
    chat_font_swap((U8 *)editor);
}
/* Space while the bar is open goes only to the bar: the shell's OnKeyDown
 * (slot 0xff72c -> 0xccab0) also sends a cinematic skip (0x3d / 0x81 at
 * 0xccc3f) for Space after the editor has typed it. */
typedef void (__attribute__((thiscall)) *ShellKeyFn)(void *,U32,U32);
void __attribute__((thiscall)) kpc_shell_key(void *shell,U32 vk,U32 repeat){
    const ShellLayout *L=g_shell_layout;U8 *ed;
    if(vk==0x20u&&shell&&*((const U8 *)shell+L->chat_editor+L->chat_editing)){
        ed=(U8 *)shell+L->chat_editor;
        ((EditorKeyFn)(*(U32 **)ed)[0x2cu/4u])(ed,vk,0);
        return;
    }
    ((ShellKeyFn)(g_command_shell+L->shell_key))(shell,vk,repeat);
}

/* Longer chat lines ([Multiplayer] ChatLongMessages, default 1). The editor's
 * text (+0x178, 0x80 bytes) is followed by its Esc backup (+0x1f8, 0x80
 * bytes) and then a pointer (+0x278); Init zeroes +0x277. The text may now use
 * both (ED_MAX_LONG = 200 characters: a relayed "[name]  text" line with a
 * 32-character name stays under the 254 the receiving clients keep):
 *  - the insert call 0xd81da -> 0xd7eb0 (127 cap, 0x80-byte stack copy,
 *    terminates at +0x1f7) is replaced by kpc_chat_insert;
 *  - the layout's scroll cap at 0xd7ce1 ("cmp edi,0x7f; jge") is removed (the
 *    loop also stops at the NUL);
 *  - Esc (slot 0x1004f0 -> 0xd7550, which copied the backup back) clears the
 *    line instead, so nothing is restored from the overwritten backup;
 *  - the editor's visible width (Init argument, 0xca916 "mov edi,[eax+0x210]",
 *    the screen width in pixels, then drawn scaled by W/640) is 640, so the
 *    line scrolls before it runs off the right edge on wide screens.
 * Selection highlight (with ChatClipboard): while Ctrl+A's selection is
 * active the editor's Draw (slot 0x1004cc -> 0xd8200) shows its text surface
 * in the menu highlight font (state flag +0x24: surface +0x1c, pale yellow). */
#define ED_HILITE 0x24u
typedef U8 (CDECL *KeyCharFn)(U32);
void __attribute__((thiscall)) kpc_chat_insert(void *editor,U32 vk){
    U8 *ed=(U8 *)editor;char c=(char)((KeyCharFn)(g_command_shell+g_shell_layout->key_char))(vk);
    U32 len,cursor,k;
    if(!ed||!c||!editor_allowed(ed,c))return;
    len=editor_len(ed);cursor=*(U32 *)(ed+ED_CURSOR);if(cursor>len)cursor=len;
    if(ed[ED_INSERT]||cursor>=len){
        if(len>=ED_MAX)return;
        for(k=len+1u;k>cursor;k--)ed[ED_TEXT+k]=ed[ED_TEXT+k-1u];
        len++;
    }
    ed[ED_TEXT+cursor++]=(U8)c;ed[ED_TEXT+len]=0;*(U32 *)(ed+ED_CURSOR)=cursor;
}
void __attribute__((thiscall)) kpc_chat_escape(void *editor){editor_clear((U8 *)editor);chat_selected=0;}
/* Chat box placement: the list's per-frame update (call 0xd5e44 -> 0xd50c0)
 * runs with its rectangle already moved for this frame (+0x3c top, +0x44
 * bottom, 640x480 coordinates); the entry bar (+0x2c, same coordinates) is
 * kept below that bottom. Both are logged once (two-PC test RC5: the bar
 * covered the chat at 2560x1440, which the static values do not explain). */
static I32 list_top,list_bottom;static U32 list_seen,layout_logged;
void __attribute__((thiscall)) kpc_list_update(void *list){
    U8 *l=(U8 *)list;
    list_top=*(const I32 *)(l+0x3cu);list_bottom=*(const I32 *)(l+0x44u);list_seen=1;
    ((EditorFn)(g_command_shell+g_shell_layout->list_update))(list);
}
static void layout_dec(char *b,U32 *at,I32 v){
    char t[12];U32 k=0,n=(U32)(v<0?-v:v);if(v<0)b[(*at)++]='-';
    do{t[k++]=(char)('0'+n%10u);n/=10u;}while(n);while(k)b[(*at)++]=t[--k];
}
typedef void (__attribute__((thiscall)) *DrawFn1)(void *,void *);
void __attribute__((thiscall)) kpc_chat_draw(void *editor,void *surface){
    U8 *ed=(U8 *)editor;U8 saved;I32 *y=(I32 *)(ed+0x2cu);
    if(g_chat_hide_loading&&chat_hidden())return;
    if(list_seen&&list_bottom>0&&list_bottom<480&&*y<list_bottom+2){
        if(g_focus_log&&layout_logged<2u){
            char line[128];U32 at=0;const char *p="Chat layout: entry bar y ";layout_logged++;
            while(*p)line[at++]=*p++;layout_dec(line,&at,*y);
            p=" moved below the chat lines (list ";while(*p)line[at++]=*p++;
            layout_dec(line,&at,list_top);line[at++]='-';layout_dec(line,&at,list_bottom);
            p=", 640x480 units).\r\n";while(*p)line[at++]=*p++;line[at]=0;g_focus_log(line);
        }
        *y=list_bottom+2;
    }
    if(chat_selected&&!ed[ED_FRESH])chat_selected=0;   /* every key clears "fresh" */
    if(chat_selected&&ed[ED_TEXT]){
        saved=ed[ED_HILITE];ed[ED_HILITE]=1;
        ((DrawFn1)(g_command_shell+g_shell_layout->chat_draw))(editor,surface);
        ed[ED_HILITE]=saved;return;
    }
    ((DrawFn1)(g_command_shell+g_shell_layout->chat_draw))(editor,surface);
}
U32 kpc_blocked_actions(void){return blocked_actions;}

/* The engine also keeps the bound commands' on/off state itself and sends it
 * to the server: object.lto polls IsCommandOn(client, 93/91) (0xef1b4,
 * 0xef204) for Invuln/NoTarget, so the OnCommandOn filter above cannot stop
 * them. client.exe 16131ccf input update 0x23c26: after the input manager
 * fills the new command buffer, 0x23c84 reads the input-enabled flag
 * [cm+0x1014] (SetInputState) and clears the whole buffer when it is 0.
 * While the chat editor is typing that read reports 0, so the frame has no
 * command on: no movement, firing or developer cheat reaches the shell or the
 * server, and held commands are released as on a normal key-up. Typed text
 * still arrives through WM_CHAR/key events. */
static U32 blocked_frames;
static int chat_typing(void){
    const ShellLayout *L=g_shell_layout;U8 *shell;
    if(!g_chat_blocks_actions||!g_command_shell||!L||!L->client_shell_global||!L->chat_editor)return 0;
    shell=*(U8 **)(g_command_shell+L->client_shell_global);
    return shell&&shell[L->chat_editor+L->chat_editing];
}
/* Chat during cinematics ([Multiplayer] ChatInCinematics, default 1). A
 * multiplayer cinematic locks input (MID_CINEMATIC sub 1 -> Lock(2, 2) on the
 * lock object [cshell+0x1d2340], index 2 = SetInputState 0), so 0x23c84 wipes
 * the whole command buffer every frame and the Message action (42) that
 * opens the chat bar never reaches OnCommandOn. While that lock (mask 2 at
 * +8) is set and the bar is closed, every command except 42 is cleared here
 * and the buffer is kept (EBP = the frame's 255-byte command buffer). */
U32 g_chat_in_cinematics;
#define ACTION_MESSAGE 42u
U32 CDECL kpc_input_enabled(U32 state,U8 *commands){
    if(state&&chat_typing()){blocked_frames++;return 0;}
    if(!state&&g_chat_in_cinematics&&commands&&g_command_shell&&g_shell_layout->input_locks&&
       !chat_typing()&&kpc_remote_multiplayer()&&(*(const U32 *)(g_command_shell+g_shell_layout->input_locks+8u)&2u)){
        U8 message=commands[ACTION_MESSAGE];U32 i;
        for(i=0;i<255u;i++)commands[i]=0;
        commands[ACTION_MESSAGE]=message;
        return 1;
    }
    return state;
}
U32 kpc_blocked_input_frames(void){return blocked_frames;}
/* Replaces "mov ecx,[eax+0x1014]" (6 bytes) at 0x23c84; EAX = cm. */
void kpc_input_state_entry(void);
__asm__(".global kpc_input_state_entry\n"
        "kpc_input_state_entry:\n"
        " pushl %eax\n"
        " pushl %edx\n"
        " pushl %ebp\n"
        " pushl 0x1014(%eax)\n"
        " call kpc_input_enabled\n"
        " addl $8,%esp\n"
        " movl %eax,%ecx\n"
        " popl %edx\n"
        " popl %eax\n"
        " ret\n");
static U32 key_ms,key_pending;
#define CONSOLE_KEY_WINDOW_MS 250u

U32 CDECL kpc_console_key_down(void){
    key_ms=GetTickCount();key_pending=1;
    return *(volatile U32 *)0x01d4a60cu;
}
U32 CDECL kpc_console_char_drop(void){
    U32 drop=key_pending&&(U32)(GetTickCount()-key_ms)<CONSOLE_KEY_WINDOW_MS;
    key_pending=0;
    return drop;
}
/* Replaces "call 0x287a0" at 0x6dc9 (console key down). */
void kpc_console_key_entry(void);
__asm__(".global kpc_console_key_entry\n"
        "kpc_console_key_entry:\n"
        " pushl %ecx\n"
        " pushl %edx\n"
        " call kpc_console_key_down\n"
        " popl %edx\n"
        " popl %ecx\n"
        " ret\n");
/* Replaces "cmp eax,0x60 / je 0x7054" at 0x700a (WM_CHAR, console open):
 * EAX = character. Skips to 0x7054 for a backtick or a dropped character,
 * otherwise continues at 0x700f. */
void kpc_console_char_entry(void);
__asm__(".global kpc_console_char_entry\n"
        "kpc_console_char_entry:\n"
        " cmpl $0x60,%eax\n"
        " je 2f\n"
        " pushl %eax\n"
        " pushl %ecx\n"
        " pushl %edx\n"
        " call kpc_console_char_drop\n"
        " testl %eax,%eax\n"
        " popl %edx\n"
        " popl %ecx\n"
        " popl %eax\n"
        " jnz 2f\n"
        " ret\n"
        "2:\n"
        " addl $4,%esp\n"
        " pushl $0x01ca7054\n"
        " ret\n");
