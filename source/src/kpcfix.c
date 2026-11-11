#include "winmini.h"
#include "sha256.h"
#include "hud.h"
#include "ui.h"

#include "shell_layout.h"
#include "loading.h"
#include "netwatch.h"
#include "focus.h"
#define KPC_VERSION "KPCFix 1.13 for Windows"
#define KPC_CLOCK_DEFAULT 1
const ShellLayout *g_shell_layout;
typedef char assert_pointer_size[(sizeof(void *)==4)?1:-1];
typedef char assert_wchar_size[(sizeof(L'a')==2)?1:-1];
#define PATH_CAP 260u
#define ARRAY_COUNT(a) ((U32)(sizeof(a)/sizeof((a)[0])))
#define TIMING_READY 1u
#define ANIMATION_READY 2u
#define GROUND_READY 4u
#define TIMING_RUNTIME 8u
#define ANIMATION_RUNTIME 16u
#define GROUND_RUNTIME 32u
#define DISABLED 64u
#define ERROR_STATE 128u
#define BACKEND_READY 256u

typedef struct {
    const char *name; U8 hash[32]; U32 file_size,image_size;
    U8 clock[35],limiter[5],animation[8][5];
} ClientProfile;
typedef struct {
    const char *name; U8 hash[32]; U32 file_size,image_size;
    ShellLayout layout;
} ShellProfile;
#include "profiles.h"

extern void carry_0(void),carry_1(void),carry_2(void),carry_3(void);
extern void carry_4(void),carry_5(void),carry_6(void),carry_7(void);
extern void probe_entry(void);
extern void input_interval_guard(void);
U32 g_input_late_events,g_input_zero_intervals,g_input_first_late_delta;
static int late_event_reported;
void *g_original_ftol;
double g_carries[8];
/* Optional minimum local-server step (minstep_hook.S); 0 = Timing V3 clock_v2. */
float g_min_step_seconds;
static U32 min_step_ms;
extern void local_server_min_step(void);
static void (*const carry_helpers[8])(void)={carry_0,carry_1,carry_2,carry_3,carry_4,carry_5,carry_6,carry_7};

static HMODULE self_module;
static volatile U32 backend_state,backend_thread,game_state,game_thread,movement_lock;
volatile U32 kpc_status;
static FARPROC real_input[7];
static WCHAR main_path[PATH_CAP],ini_path[PATH_CAP],log_path[PATH_CAP],shell_path[PATH_CAP];
static HANDLE log_handle=INVALID_HANDLE_VALUE;
static int paths_ready;
static const ClientProfile *client_profile;
static const ShellProfile *startup_shell_profile;
typedef HMODULE (WINAPI *LoadA)(const char *);
static LoadA original_load_library;
typedef DWORD (WINAPI *InputClock)(void);
static InputClock precise_input_clock;
static int input_clock_enabled;
static int stock_mouse;
static U32 sensitivity_range_percent;
static int normalize_configs_enabled;
static float sensitivity_maximum,sensitivity_minimum,sensitivity_inverse;
static U32 clock_samples,coarse_previous,fine_previous,coarse_start,fine_start,coarse_zero,fine_zero;
static int net_trace_enabled;
typedef int (WINAPI *NetErrorGet)(void);
static NetErrorGet net_error_get;
static U32 net_epoch;
static volatile U32 net_events,net_tx,net_rx,net_tx_errors,net_rx_errors,net_wouldblock;
static volatile U32 net_log_lock;
static int (WINAPI *net_startup)(U32,void *);
static int (WINAPI *net_cleanup)(void);
static U32 (WINAPI *net_socket)(int,int,int);
static int (WINAPI *net_close)(U32);
static int (WINAPI *net_bind)(U32,const void *,int);
static int (WINAPI *net_send)(U32,const char *,int,int,const void *,int);
static int (WINAPI *net_receive)(U32,char *,int,int,void *,int *);

HRESULT WINAPI DirectInputCreateA(HINSTANCE,DWORD,void **,void *);
HRESULT WINAPI DirectInputCreateW(HINSTANCE,DWORD,void **,void *);
HRESULT WINAPI DirectInputCreateEx(HINSTANCE,DWORD,const void *,void **,void *);
static HMODULE WINAPI kpc_load_library_a(const char *);

static void copy_bytes(void *destination,const void *source,U32 n){
    U8 *d=(U8 *)destination;const U8 *s=(const U8 *)source;U32 i;
    for(i=0;i<n;i++)d[i]=s[i];
}
static int equal_bytes(const void *a,const void *b,U32 n){
    const U8 *x=(const U8 *)a,*y=(const U8 *)b;U32 i;
    for(i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static U32 text_length(const char *s){U32 n=0;while(s[n])n++;return n;}
static U16 word(const U8 *p){return (U16)(p[0]|((U16)p[1]<<8));}
static U32 dword(const U8 *p){return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24);}
static void store32(U8 *p,U32 value){U32 i;for(i=0;i<4;i++)p[i]=(U8)(value>>(8*i));}

static void log_text(const char *text){
    DWORD done;
    if(log_handle!=INVALID_HANDLE_VALUE)WriteFile(log_handle,text,text_length(text),&done,NULL);
}
static void log_hex(U32 value){
    char out[11];U32 i;static const char digits[]="0123456789abcdef";
    out[0]='0';out[1]='x';for(i=0;i<8;i++)out[i+2]=digits[(value>>(28-4*i))&15];out[10]=0;log_text(out);
}
static void log_hash(const U8 hash[32]){
    char out[65];U32 i;static const char digits[]="0123456789abcdef";
    for(i=0;i<32;i++){out[i*2]=digits[hash[i]>>4];out[i*2+1]=digits[hash[i]&15];}out[64]=0;log_text(out);
}
static void net_decimal(U32 n){
    char b[11];U32 at=10;b[at]=0;
    do{b[--at]=(char)('0'+n%10u);n/=10u;}while(n);
    log_text(b+at);
}
static int net_budget(volatile U32 *counter,U32 limit){return __sync_fetch_and_add(counter,1)<limit;}
static int net_take_log(void){return __sync_bool_compare_and_swap(&net_log_lock,0,1);}
static void net_end_log(void){__sync_lock_release(&net_log_lock);}
static void net_line(const char *op,U32 socket,int result,int error,U32 caller){
    log_text("NET ms=");net_decimal(GetTickCount()-net_epoch);log_text(" ");log_text(op);
    log_text(" socket=");log_hex(socket);log_text(" result=");log_hex((U32)result);
    log_text(" post_wsa_error=");net_decimal((U32)error);
    log_text(" caller=");log_hex(caller);
}
static void net_endpoint(const void *address,int length){
    const U8 *a=(const U8 *)address;
    if(a&&length>=8&&a[0]==2&&a[1]==0){
        log_text(" endpoint=");net_decimal(a[4]);log_text(".");net_decimal(a[5]);log_text(".");
        net_decimal(a[6]);log_text(".");net_decimal(a[7]);log_text(":");net_decimal(((U32)a[2]<<8)|a[3]);
    }
}
static void net_header(const char *buffer,int requested,int actual){
    char hex[17];U32 n,i;static const char digits[]="0123456789abcdef";
    if(!buffer||requested<=0||actual<=0)return;
    n=(U32)(actual<requested?actual:requested);if(n>8)n=8;
    for(i=0;i<n;i++){U8 v=(U8)buffer[i];hex[i*2]=digits[v>>4];hex[i*2+1]=digits[v&15];}
    hex[n*2]=0;log_text(" header8=");log_text(hex);
}
/* Read errors immediately after forwarding. Logging must not replace the
 * network call's result, buffer contents or per-thread error state. */
static int net_capture(DWORD *win_error){*win_error=GetLastError();return net_error_get();}
/* The logger uses only Win32 file/time APIs, not other Winsock operations.
 * Restore their thread error slot. A distinct Winsock error slot is untouched;
 * when the slots alias this restores both. No Winsock setter/startup is added. */
static void net_restore(DWORD win_error){SetLastError(win_error);}
__attribute__((noipa)) int WINAPI kpc_net_startup(U32 version,void *data){
    int r=net_startup(version,data),e;DWORD w;e=net_capture(&w);
    if(net_budget(&net_events,128)&&net_take_log()){net_line("WSAStartup",0,r,e,(U32)__builtin_return_address(0));log_text(" version=");log_hex(version);log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
__attribute__((noipa)) int WINAPI kpc_net_cleanup(void){
    int r=net_cleanup(),e;DWORD w;e=net_capture(&w);
    if(net_budget(&net_events,128)&&net_take_log()){net_line("WSACleanup",0,r,e,(U32)__builtin_return_address(0));log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
__attribute__((noipa)) U32 WINAPI kpc_net_socket(int family,int type,int protocol){
    U32 r=net_socket(family,type,protocol);int e;DWORD w;e=net_capture(&w);
    if(net_budget(&net_events,128)&&net_take_log()){net_line("socket",r,(int)r,e,(U32)__builtin_return_address(0));log_text(" family=");net_decimal((U32)family);log_text(" type=");net_decimal((U32)type);log_text(" protocol=");net_decimal((U32)protocol);log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
__attribute__((noipa)) int WINAPI kpc_net_close(U32 socket){
    int r=net_close(socket),e;DWORD w;e=net_capture(&w);
    if(net_budget(&net_events,128)&&net_take_log()){net_line("closesocket",socket,r,e,(U32)__builtin_return_address(0));log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
__attribute__((noipa)) int WINAPI kpc_net_bind(U32 socket,const void *address,int length){
    int r=net_bind(socket,address,length),e;DWORD w;e=net_capture(&w);
    if(net_budget(&net_events,128)&&net_take_log()){net_line("bind",socket,r,e,(U32)__builtin_return_address(0));if(r==0)net_endpoint(address,length);log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
__attribute__((noipa)) int WINAPI kpc_net_send(U32 socket,const char *buffer,int length,int flags,const void *address,int address_length){
    int r=net_send(socket,buffer,length,flags,address,address_length),e,show;DWORD w;e=net_capture(&w);
    show=r<0?net_budget(&net_tx_errors,32):net_budget(&net_tx,48);
    if(show&&net_take_log()){net_line("sendto",socket,r,e,(U32)__builtin_return_address(0));log_text(" requested=");net_decimal((U32)length);if(r>=0){net_endpoint(address,address_length);net_header(buffer,length,r);}log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
__attribute__((noipa)) int WINAPI kpc_net_receive(U32 socket,char *buffer,int length,int flags,void *address,int *address_length){
    int r=net_receive(socket,buffer,length,flags,address,address_length),e,show;DWORD w;e=net_capture(&w);
    if(r>=0)show=net_budget(&net_rx,48);
    else if(e==10035){U32 n=__sync_add_and_fetch(&net_wouldblock,1);show=n<=0x100000u&&(n&(n-1u))==0;}
    else show=net_budget(&net_rx_errors,32);
    if(show&&net_take_log()){net_line("recvfrom",socket,r,e,(U32)__builtin_return_address(0));if(r>=0){if(address&&address_length)net_endpoint(address,*address_length);net_header(buffer,length,r);}else if(e==10035){log_text(" wouldblock_count=");net_decimal(net_wouldblock);}log_text("\r\n");net_end_log();}
    net_restore(w);return r;
}
void CDECL kpc_hud_observe(U32 width,U32 height,U32 screen){
    static U32 observed;
    if(__sync_val_compare_and_swap(&observed,0,1)!=0)return;
    log_text("HUD first draw: width=");log_hex(width);log_text(" height=");log_hex(height);
    log_text(" scale_percent=");log_hex(g_hud_percent);log_text(" screen=");log_hex(screen);log_text("\r\n");
}
/* Same uptime-millisecond domain; change only the two reviewed input call sites.
 * Capture one bounded comparison of API cadence without input-event logging. */
static DWORD WINAPI kpc_input_clock(void){
    U32 fine=precise_input_clock();
    if(g_input_late_events&&!late_event_reported){
        late_event_reported=1;
        log_text("First late input event: negative interval bits=");log_hex(g_input_first_late_delta);
        log_text("; unsigned underflow prevented.\r\n");
    }
    if(clock_samples<=512){
        U32 coarse=GetTickCount();
        if(!clock_samples){coarse_start=coarse;fine_start=fine;}
        else{if(coarse==coarse_previous)coarse_zero++;if(fine==fine_previous)fine_zero++;}
        coarse_previous=coarse;fine_previous=fine;
        if(clock_samples==512){
            log_text("Input clock sample: intervals=512 coarse_zero=");log_hex(coarse_zero);
            log_text(" fine_zero=");log_hex(fine_zero);log_text(" coarse_elapsed_ms=");log_hex(coarse-coarse_start);
            log_text(" fine_elapsed_ms=");log_hex(fine-fine_start);log_text("\r\n");
            log_text("Input interval guards: late_events=");log_hex(g_input_late_events);
            log_text(" zero_interval_seeds=");log_hex(g_input_zero_intervals);log_text("\r\n");
        }
        clock_samples++;
    }
    return fine;
}
static void log_wide(const WCHAR *path){
    char out[1024];int n=WideCharToMultiByte(65001,0,path,-1,out,sizeof(out),NULL,NULL);
    if(n>0)log_text(out);else log_text("[path conversion failed]");
}
void CDECL kpc_config_notice(const WCHAR *path,const char *message){
    log_text(message);log_wide(path);log_text("\r\n");
}
static void report_error(const char *message){
    kpc_status|=ERROR_STATE;log_text("ERROR: ");log_text(message);log_text("\r\n");
    MessageBoxA(NULL,message,"KPCFix 1.13 for Windows",0x10);
}
static int sibling_path(WCHAR out[PATH_CAP],const WCHAR *name){
    U32 n=0,last=0,i;
    while(n<PATH_CAP&&main_path[n]){if(main_path[n]=='\\'||main_path[n]=='/')last=n+1;n++;}
    if(n==PATH_CAP||!last)return 0;
    for(i=0;i<last;i++)out[i]=main_path[i];
    for(i=0;name[i];i++){if(last+i+1>=PATH_CAP)return 0;out[last+i]=name[i];}
    out[last+i]=0;return 1;
}
static U16 lower_ascii(U16 v){return(v>='A'&&v<='Z')?(U16)(v+32):v;}
static int basename_is(const WCHAR *path,const WCHAR *name){
    U32 n=0,start=0,i=0;
    while(n<PATH_CAP&&path[n]){if(path[n]=='\\'||path[n]=='/')start=n+1;n++;}
    if(n==PATH_CAP)return 0;
    while(name[i]&&start+i<n){if(lower_ascii(path[start+i])!=lower_ascii(name[i]))return 0;i++;}
    return !name[i]&&start+i==n;
}

/* state: 0 unstarted, 1 running, 2 ready, 3 failed. No work in DllMain. */
static int enter_once(volatile U32 *state,volatile U32 *owner){
    DWORD tid=GetCurrentThreadId(),start;
    if(__sync_val_compare_and_swap(state,0,1)==0){*owner=tid;__sync_synchronize();return 1;}
    if(*state==2)return 2;
    if(*state!=1||*owner==tid)return 0;
    start=GetTickCount();
    while(*state==1&&(DWORD)(GetTickCount()-start)<10000)Sleep(1);
    return *state==2?2:0;
}
static void finish_once(volatile U32 *state,U32 value){__sync_synchronize();*state=value;}

static int ensure_backend(void){
    static const char *const names[7]={"DirectInputCreateA","DirectInputCreateW","DirectInputCreateEx","DllGetClassObject","DllCanUnloadNow","DllRegisterServer","DllUnregisterServer"};
    WCHAR system_path[PATH_CAP];U32 i,n;HMODULE real;int entered=enter_once(&backend_state,&backend_thread);
    if(entered!=1)return entered==2;
    n=GetModuleFileNameW(NULL,main_path,PATH_CAP);
    main_path[PATH_CAP-1]=0;
    if(n&&n<PATH_CAP&&sibling_path(ini_path,L"KPCFix.ini")&&sibling_path(log_path,L"KPCFix.log")&&sibling_path(shell_path,L"cshell.dll")){
        paths_ready=1;
        if(GetPrivateProfileIntW(L"KPCFix",L"Log",1,ini_path))
            log_handle=CreateFileW(log_path,4,3,NULL,4,0x80,NULL); /* append, OPEN_ALWAYS */
    }
    log_text("\r\n" KPC_VERSION " -- Timing V3 + Ground V5 + Sensitivity Range V1\r\nProcess: ");log_wide(main_path);log_text("\r\n");
    n=GetSystemDirectoryW(system_path,PATH_CAP);
    if(!n||n>=PATH_CAP-12){report_error("Cannot resolve the system DirectInput library. Remove the local dinput.dll to restore the previous launch.");finish_once(&backend_state,3);return 0;}
    if(system_path[n-1]!='\\')system_path[n++]='\\';
    {const WCHAR *tail=L"dinput.dll";for(i=0;tail[i];i++)system_path[n+i]=tail[i];system_path[n+i]=0;}
    real=LoadLibraryW(system_path);
    if(!real||real==self_module){report_error("Could not load dinput.dll from the Windows system directory.");finish_once(&backend_state,3);return 0;}
    for(i=0;i<7;i++)real_input[i]=GetProcAddress(real,names[i]);
    if(!real_input[0]||!real_input[1]||!real_input[2]||real_input[0]==(FARPROC)DirectInputCreateA){
        report_error("System DirectInput exports are unavailable or point back to the proxy. No game patches were applied.");finish_once(&backend_state,3);return 0;
    }
    log_text("System DirectInput: ");log_wide(system_path);log_text("\r\n");
    kpc_status|=BACKEND_READY;finish_once(&backend_state,2);return 1;
}

__attribute__((noipa)) int CDECL kpc_hash_file(const WCHAR *path,U8 digest[32],U32 *file_size){
    HANDLE file;LARGE_INTEGER size;KpcSha state;U8 buffer[1024];DWORD got;U32 total=0;int ok=0;
    file=CreateFileW(path,0x80000000u,1,NULL,3,0x80,NULL);
    if(file==INVALID_HANDLE_VALUE)return 0;
    if(!GetFileSizeEx(file,&size)||size.high||!size.low||size.low>4u*1024u*1024u)goto done;
    kpc_sha_init(&state);
    for(;;){
        if(!ReadFile(file,buffer,sizeof(buffer),&got,NULL))goto done;
        if(got>sizeof(buffer)||total>size.low||got>size.low-total)goto done;
        if(!got)break;
        kpc_sha_update(&state,buffer,got);total+=got;
    }
    if(total!=size.low)goto done;
    kpc_sha_final(&state,digest);*file_size=total;ok=1;
done:CloseHandle(file);return ok;
}
static const ClientProfile *find_client(const U8 hash[32],U32 size){
    U32 i;for(i=0;i<ARRAY_COUNT(client_profiles);i++)if(size==client_profiles[i].file_size&&equal_bytes(hash,client_profiles[i].hash,32))return &client_profiles[i];return NULL;
}
static const ShellProfile *find_shell(const U8 hash[32],U32 size){
    U32 i;for(i=0;i<ARRAY_COUNT(shell_profiles);i++)if(size==shell_profiles[i].file_size&&equal_bytes(hash,shell_profiles[i].hash,32))return &shell_profiles[i];return NULL;
}
static int valid_image(const U8 *base,U32 image_size,U32 preferred){
    U32 pe;
    if(!base||word(base)!=0x5a4d)return 0;
    pe=dword(base+0x3c);
    if(pe<64||pe>4096-248||dword(base+pe)!=0x4550||word(base+pe+4)!=0x14c||word(base+pe+24)!=0x10b)return 0;
    return dword(base+pe+24+56)==image_size&&dword(base+pe+24+28)==preferred;
}

typedef struct {U8 *address;U32 length;U8 before[40],after[40];DWORD protection;int touched;} Edit;
static int add_edit(Edit edits[48],U32 *count,U8 *address,const U8 *before,const U8 *after,U32 length){
    Edit *e;
    if(length>40||!equal_bytes(address,before,length))return 0;
    if(equal_bytes(before,after,length))return 1;
    if(*count>=48)return 0;
    e=&edits[(*count)++];e->address=address;e->length=length;e->protection=0;e->touched=0;copy_bytes(e->before,before,length);copy_bytes(e->after,after,length);return 1;
}
static int apply_edit(Edit *e){
    DWORD ignored;int restored,flushed;
    if(!equal_bytes(e->address,e->before,e->length)||!VirtualProtect(e->address,e->length,0x40,&e->protection))return 0;
    e->touched=1;
    if(!equal_bytes(e->address,e->before,e->length)){VirtualProtect(e->address,e->length,e->protection,&ignored);return 0;}
    copy_bytes(e->address,e->after,e->length);
    restored=VirtualProtect(e->address,e->length,e->protection,&ignored);
    flushed=FlushInstructionCache(GetCurrentProcess(),e->address,e->length);
    return restored&&flushed;
}
static int undo_edit(Edit *e){
    DWORD ignored;int restored,flushed;
    if(!e->touched)return 1;
    if(!equal_bytes(e->address,e->after,e->length)&&!equal_bytes(e->address,e->before,e->length))return 0;
    if(!VirtualProtect(e->address,e->length,0x40,&ignored))return 0;
    copy_bytes(e->address,e->before,e->length);
    restored=VirtualProtect(e->address,e->length,e->protection,&ignored);
    flushed=FlushInstructionCache(GetCurrentProcess(),e->address,e->length);
    return restored&&flushed;
}
static int commit_edits(Edit edits[48],U32 count){
    U32 i,j;
    for(i=0;i<count;i++)if(!equal_bytes(edits[i].address,edits[i].before,edits[i].length))return 0;
    for(i=0;i<count;i++){
        Edit *e=&edits[i];
        if(!apply_edit(e)){
            /* Include the current edit: protection/cache failure can follow its copy. */
            for(j=i+1;j>0;j--)if(!undo_edit(&edits[j-1]))log_text("ERROR: restoring a previous memory edit also failed. Restart before further testing.\r\n");
            return 0;
        }
    }
    return 1;
}
static void near_call(U8 bytes[5],const U8 *site,const void *target){bytes[0]=0xe8;store32(bytes+1,(U32)target-((U32)site+5));}

/* Only the local bind-address parser is redirected. An empty IP setting
 * means INADDR_ANY, just like the original null-pointer branch. Explicit
 * addresses (including invalid nonempty strings) keep native inet_addr behavior. */
static U32 (WINAPI *original_bind_inet_addr)(const char *);
__attribute__((noipa)) U32 WINAPI kpc_bind_ip(const char *ip){
    if(!ip||!*ip)return 0;
    return original_bind_inet_addr(ip);
}
static int queue_bind_fix(Edit edits[48],U32 *count,U8 *base){
    U8 before[6]={0xff,0x15,0,0,0,0},after[6];
    if(!GetPrivateProfileIntW(L"Multiplayer",L"EmptyBindIPFix",1,ini_path))return 1;
    original_bind_inet_addr=(void *)dword(base+0x97288);
    if(!original_bind_inet_addr)return 0;
    store32(before+2,(U32)base+0x97288);
    near_call(after,base+0x772e2,kpc_bind_ip);after[5]=0x90;
    if(!add_edit(edits,count,base+0x772e2,before,after,6))return 0;
    log_text("EmptyBindIPFix v1: empty local IP uses INADDR_ANY; explicit IP preserved.\r\n");
    return 1;
}

static int queue_net_trace(Edit edits[48],U32 *count,U8 *base){
    static const U32 sites[7]={0x97284,0x972a0,0x972a8,0x9728c,0x972a4,0x97290,0x9729c};
    void *targets[7]={(void *)kpc_net_startup,(void *)kpc_net_cleanup,(void *)kpc_net_socket,(void *)kpc_net_close,(void *)kpc_net_bind,(void *)kpc_net_send,(void *)kpc_net_receive};
    U8 before[4],after[4];U32 i,values[7];
    if(!net_trace_enabled)return 1;
    net_error_get=(NetErrorGet)dword(base+0x972ac);
    if(!net_error_get){log_text("NET trace unavailable: Winsock error getter unresolved; no network hooks installed.\r\n");net_trace_enabled=0;return 1;}
    for(i=0;i<7;i++){values[i]=dword(base+sites[i]);if(!values[i])return 0;}
    net_startup=(void *)values[0];net_cleanup=(void *)values[1];net_socket=(void *)values[2];net_close=(void *)values[3];
    net_bind=(void *)values[4];net_send=(void *)values[5];net_receive=(void *)values[6];
    for(i=0;i<7;i++){
        store32(before,values[i]);store32(after,(U32)targets[i]);
        if(!add_edit(edits,count,base+sites[i],before,after,4))return 0;
    }
    net_epoch=GetTickCount();return 1;
}

/* Redirect only the menu's sensitivity operands; shared constants have other users. */
static int queue_sensitivity(Edit edits[48],U32 *count,U8 *base){
    const U32 *sites=g_shell_layout->sensitivity_sites;
    const U32 *values=g_shell_layout->sensitivity_constants;
    const float *targets[3]={&sensitivity_maximum,&sensitivity_minimum,&sensitivity_inverse};
    U8 before[8],after[8];U32 i;
    if(sensitivity_range_percent==100)return 1;
    for(i=0;i<3;i++){
        before[0]=after[0]=0xd8;before[1]=after[1]=(i==1?0x1d:0x0d);
        store32(before+2,(U32)base+values[i]);store32(after+2,(U32)targets[i]);
        if(!equal_bytes(base+sites[i],after,6)&&!add_edit(edits,count,base+sites[i],before,after,6))return 0;
    }
    before[0]=after[0]=0xc7;before[1]=after[1]=0x44;
    before[2]=after[2]=0x24;before[3]=after[3]=0x04;
    store32(before+4,0x3dcccccdu);store32(after+4,dword((const U8 *)&sensitivity_minimum));
    return equal_bytes(base+g_shell_layout->sensitivity_min_store,after,8)||add_edit(edits,count,base+g_shell_layout->sensitivity_min_store,before,after,8);
}

/* Limit scaling to the HUD's own panel draws, glyphs and crosshair calls.
 * No renderer surfaces, global resolution values or menu draw calls change. */
static int queue_hud(Edit edits[48],U32 *count,U8 *base){
    typedef struct {U32 length;U8 before[7];void (*entry)(void);} HudSite;
    static const HudSite sites[]={
        {5,{0xff,0x50,0x04,0x8b,0x16},hud_panel_entry},
        /* Skip-panel branches land immediately after this site.
         * Take the vtable load, argument push and call together before it. */
        {6,{0x8b,0x11,0x50,0xff,0x52,0x08},hud_post_entry},
        {5,{0xc2,0x04,0x00,0x90,0x90},hud_rect_entry},
        {6,{0xff,0x91,0x88,0x01,0x00,0x00},hud_solid_ecx},
        {6,{0xff,0x91,0x88,0x01,0x00,0x00},hud_solid_ecx},
        {6,{0xff,0x90,0x90,0x01,0x00,0x00},hud_scale_eax},
        {6,{0xff,0x90,0x90,0x01,0x00,0x00},hud_scale_eax},
        {6,{0xff,0x92,0x90,0x01,0x00,0x00},hud_scale_edx},
        {6,{0xff,0x92,0x90,0x01,0x00,0x00},hud_scale_edx},
        {6,{0xff,0x92,0x90,0x01,0x00,0x00},hud_scale_edx},
        {6,{0xff,0x91,0x88,0x01,0x00,0x00},hud_cross_ecx},
        {6,{0xff,0x90,0x88,0x01,0x00,0x00},hud_cross_eax},
        {5,{0},hud_carousel_entry}
    };
    U32 i,j;U8 after[7],call[5];
    if(g_hud_percent==100)return 1;
    for(i=0;i<ARRAY_COUNT(sites);i++){
        const HudSite *site=&sites[i];
        U8 *address=base+g_shell_layout->hud_sites[i];
        const U8 *expected=site->before;
        if(site->entry==hud_carousel_entry){
            near_call(call,address,base+g_shell_layout->panel_number);
            expected=call;
        }
        near_call(after,address,site->entry);
        if(site->entry==hud_rect_entry)after[0]=0xe9; /* whole ret-4 helper entry */
        for(j=5;j<site->length;j++)after[j]=0x90;
        if(!equal_bytes(address,after,site->length)&&
           !add_edit(edits,count,address,expected,after,site->length))return 0;
    }
    /* The monster-health pass draws six opaque rectangles, then uses
     * the same bitmap font as the ordinary HUD. Scope that font too. */
    const U32 offsets[]={0x178,0x1f5,0x27b,0x2ff,0x3fb,0x496};
    const U8 registers[]={0x91,0x91,0x91,0x90,0x92,0x92};
    U8 *draw=base+g_shell_layout->enemy_draw;
    U8 *caller=base+g_shell_layout->enemy_call;
    near_call(call,caller,draw);
    near_call(after,caller,kpc_enemy_draw);
    if(!add_edit(edits,count,caller,call,after,5))return 0;
    for(i=0;i<ARRAY_COUNT(offsets);i++){
        U8 original[]={0xff,registers[i],0x8c,1,0,0};
        U8 *site=draw+offsets[i];
        near_call(after,site,kpc_enemy_blit);
        after[5]=0x90;
        if(!add_edit(edits,count,site,original,after,6))return 0;
    }
    /* EDI already holds (bar height - font height) / 2. Skip only the
     * screen-height bias; the next instruction adds this row's top. */
    static const U8 bias[2]={0x2b,0xf8}; /* sub edi,eax */
    static const U8 centered[2]={0x90,0x90};
    if(!add_edit(edits,count,base+g_shell_layout->enemy_label_y_bias,bias,centered,2))return 0;
    return 1;
}

/* Additional reviewed UI paths. Each option queues into the SAME cshell
 * transaction as the established ground/sensitivity/HUD changes. */
static int queue_ui(Edit edits[48],U32 *count,U8 *base){
    U8 before[7],after[7];U32 i;
    if(g_cursor_percent!=100){
        static const U8 dims[6]={0xff,0x92,0x74,0x01,0x00,0x00};
        static const U8 draw[6]={0xff,0x92,0x88,0x01,0x00,0x00};
        near_call(after,base+g_shell_layout->cursor_dims,ui_cursor_dims_entry);after[5]=0x90;
        if(!equal_bytes(base+g_shell_layout->cursor_dims,after,6)&&!add_edit(edits,count,base+g_shell_layout->cursor_dims,dims,after,6))return 0;
        near_call(after,base+g_shell_layout->cursor_draw,ui_cursor_draw_entry);after[5]=0x90;
        if(!equal_bytes(base+g_shell_layout->cursor_draw,after,6)&&!add_edit(edits,count,base+g_shell_layout->cursor_draw,draw,after,6))return 0;
    }
    if(g_tooltip_percent!=100){
        static const U8 font[6]={0xff,0x90,0x24,0x01,0x00,0x00};
        const U32 *lift_sites=g_shell_layout->tooltip_lift;
        U32 lift=(12*g_tooltip_percent+50)/100;
        near_call(after,base+g_shell_layout->tooltip_font,ui_tooltip_font_entry);after[5]=0x90;
        if(!equal_bytes(base+g_shell_layout->tooltip_font,after,6)&&!add_edit(edits,count,base+g_shell_layout->tooltip_font,font,after,6))return 0;
        for(i=0;i<3;i++){
            before[0]=after[0]=0x83;before[1]=after[1]=i==0?0xea:0xe8;
            before[2]=12;after[2]=(U8)lift;
            if(!equal_bytes(base+lift_sites[i],after,3)&&!add_edit(edits,count,base+lift_sites[i],before,after,3))return 0;
        }
    }
    if(g_lock_input_rate){
        U8 setter[6]={0x81,0xec,(U8)g_shell_layout->rate_stack,0,0,0};
        void (*bridge)(void)=g_shell_layout->rate_stack==0x94?ui_rate_setter_italian:ui_rate_setter_entry;
        near_call(after,base+g_shell_layout->rate_setter,bridge);after[0]=0xe9;after[5]=0x90;
        if(!equal_bytes(base+g_shell_layout->rate_setter,after,6)&&!add_edit(edits,count,base+g_shell_layout->rate_setter,setter,after,6))return 0;
    }
    if(g_hide_input_rate){
        U8 ready[5];
        near_call(ready,base+g_shell_layout->menu_ready_call,base+g_shell_layout->menu_ready);
        const U32 *positions=g_shell_layout->widget_y;
        static const U32 original_y[4]={260,280,320,360};
        near_call(after,base+g_shell_layout->menu_ready_call,kpc_mouse_menu_ready);
        if(!equal_bytes(base+g_shell_layout->menu_ready_call,after,5)&&!add_edit(edits,count,base+g_shell_layout->menu_ready_call,ready,after,5))return 0;
        store32(before,(U32)base+g_shell_layout->menu_key);store32(after,(U32)kpc_mouse_menu_key);
        if(!equal_bytes(base+g_shell_layout->menu_key_slot,after,4)&&!add_edit(edits,count,base+g_shell_layout->menu_key_slot,before,after,4))return 0;
        /* Reflow logical positions BEFORE the original initialization computes
         * the widgets' other bounds. Original fixed dispatch indices survive. */
        for(i=0;i<4;i++){
            before[0]=after[0]=0xc7;before[1]=after[1]=g_shell_layout->widget_y_modrm;before[2]=after[2]=0x2c;
            store32(before+3,original_y[i]);store32(after+3,original_y[i]-40);
            if(!equal_bytes(base+positions[i],after,7)&&!add_edit(edits,count,base+positions[i],before,after,7))return 0;
        }
    }
    return 1;
}

/* Called after LoadLibrary returns, before its caller can use cshell exports. */
/* Campaign loading art (loading.c): the PreLoadWorld vtable slot, the 0x69
 * close call and the per-frame screen draw call, all original values checked. */
static int queue_loading(Edit edits[48],U32 *count,U8 *base){
    const ShellLayout *L=g_shell_layout;U8 before[5],after[5];
    if(!g_loading_enabled)return 1;
    if(!L->preload_slot){g_loading_enabled=0;log_text("Campaign loading art: not verified for this cshell edition; stock loading retained.\r\n");return 1;}
    store32(before,(U32)(base+L->preload_world));store32(after,(U32)kpc_preload_world);
    if(!add_edit(edits,count,base+L->preload_slot,before,after,4))return 0;
    near_call(before,base+L->close_call,base+L->screen_close);near_call(after,base+L->close_call,kpc_close_on_entry);
    if(!add_edit(edits,count,base+L->close_call,before,after,5))return 0;
    near_call(before,base+L->draw_call,base+L->screen_draw);near_call(after,base+L->draw_call,kpc_draw_screen);
    return add_edit(edits,count,base+L->draw_call,before,after,5);
}
static int patch_shell(HMODULE module){
    WCHAR path[PATH_CAP];U8 digest[32],movement_call[7],guard_call[5];
    static const U8 original_move[7]={0x8b,0x46,0x38,0x50,0xff,0x52,0x40};U32 file_size,count=0,n;U8 *base=(U8 *)module;
    const ShellProfile *profile;Edit edits[48];int result=0;
    n=GetModuleFileNameW(module,path,PATH_CAP);
    if(!n||n>=PATH_CAP||!basename_is(path,L"cshell.dll"))return 1;
    if(__sync_val_compare_and_swap(&movement_lock,0,1)!=0){report_error("Concurrent cshell loading prevented safe movement-patch installation. Restart with the tested V3/V5 setup.");return 0;}
    if(!kpc_hash_file(path,digest,&file_size)||(profile=find_shell(digest,file_size))!=startup_shell_profile || !profile){report_error("The loaded cshell.dll is not a supported build. Its movement code was left unchanged. See KPCFix.log.");goto done;}
    log_text("Loaded cshell: ");log_wide(path);log_text("\r\ncshell profile: ");log_text(profile->name);log_text(" SHA256 ");log_hash(digest);log_text(" base ");log_hex((U32)base);log_text("\r\n");
    if(!valid_image(base,profile->image_size,0x20000000u)){report_error("Loaded cshell PE layout differs from its supported profile. Movement code was left unchanged.");goto done;}
    U8 *move=base+profile->layout.ground_move;
    U8 *guard=base+profile->layout.ground_guard;
    near_call(guard_call,guard,base+profile->layout.ground_guard_target);
    if(!equal_bytes(guard,guard_call,5)){
        report_error("Unexpected movement guard in cshell.dll.");goto done;
    }
    near_call(movement_call,move,probe_entry);movement_call[5]=movement_call[6]=0x90;
    if(!equal_bytes(move,movement_call,7) &&
       !add_edit(edits,&count,move,original_move,movement_call,7)){
        report_error("Unexpected movement dispatch in cshell.dll.");goto done;
    }
    if(!queue_sensitivity(edits,&count,base)||!queue_hud(edits,&count,base)||!queue_ui(edits,&count,base)||
       !queue_loading(edits,&count,base)||!commit_edits(edits,count)){
        report_error("UI/HUD/sensitivity/movement instructions differ or their memory update failed. No changes in this cshell transaction were retained.");goto done;
    }
    g_hud_shell=base;
    g_ui_shell=base;g_rate_setter_tail=base+g_shell_layout->rate_setter+6;g_loading_shell=base;
    if(g_net_timeout_enabled){
        if(g_shell_layout->client_shell_global)g_net_shell_slot=base+g_shell_layout->client_shell_global;
        else log_text("Server timeout: not verified for this cshell edition; stock 120 s retained.\r\n");
    }
    if(g_loading_enabled){log_text("Campaign loading art: original R?M? pictures in multiplayer campaign loads, minimum ms=");log_hex(g_loading_min_ms);log_text(".\r\n");}
    log_text("Ground contact: V5 applied in memory.\r\n");
    kpc_status|=GROUND_READY|GROUND_RUNTIME;
    log_text("Sensitivity range: percent of original=");log_hex(sensitivity_range_percent);
    log_text(sensitivity_range_percent==100?"; original menu instructions retained.\r\n":"; matching slider write/read/minimum adjusted.\r\n");
    log_text("HUD scale: percent=");log_hex(g_hud_percent);
    log_text(g_hud_percent==100?"; original HUD instructions retained.\r\n":"; 13 panel and 7 enemy health HUD sites installed in memory.\r\n");
    log_text("Menu cursor scale percent=");log_hex(g_cursor_percent);log_text(". InputRate lock=");log_hex(g_lock_input_rate);
    log_text("; hidden/reflowed menu section=");log_hex(g_hide_input_rate);log_text(".\r\n");result=1;
    log_text("Tooltip font scale percent=");log_hex(g_tooltip_percent);log_text("; cursor keeps original bitmap sources and uses screen scaling.\r\n");
done:__sync_synchronize();movement_lock=0;return result;
}

static HMODULE WINAPI kpc_load_library_a(const char *name){
    HMODULE module=original_load_library(name);DWORD saved=GetLastError();
    if(module)patch_shell(module);
    SetLastError(saved);return module;
}

extern void movie_aspect_entry(void);
extern void *g_movie_tail;
extern U32 g_movie_filter;
static int install_game_fixes(void){
    U8 digest[32],replacement[16],before_iat[4],after_iat[4];U32 size,i,count=0,n;
    U8 *base=(U8 *)GetModuleHandleW(NULL);Edit edits[48];HMODULE pinned,loaded;
    if(!paths_ready){report_error("Game path is unavailable or too long. No game fixes were applied.");return 0;}
    if(!basename_is(main_path,L"client.exe")){log_text("Non-game host: input forwarding only.\r\n");return 1;}
    if(!GetPrivateProfileIntW(L"KPCFix",L"Enabled",1,ini_path)){
        kpc_status|=DISABLED;log_text("Enabled=0: input forwarding only; no game-memory patches.\r\n");return 1;
    }
    if(!kpc_hash_file(main_path,digest,&size)||(client_profile=find_client(digest,size))==NULL){
        report_error("KPCFix requires the original client.exe from official game version 1.13.");return 0;
    }
    log_text("Client profile: ");log_text(client_profile->name);log_text(" SHA256 ");log_hash(digest);log_text("\r\n");
    if(!kpc_hash_file(shell_path,digest,&size)||(startup_shell_profile=find_shell(digest,size))==NULL){report_error("KPCFix requires the original region-matched 1.13 cshell.dll in the game folder. See KPCFix.log.");return 0;}
    g_shell_layout=&startup_shell_profile->layout;
    log_text("Folder cshell profile: ");log_text(startup_shell_profile->name);log_text("\r\n");
    if((U32)base!=0x01ca0000u||!valid_image(base,client_profile->image_size,0x01ca0000u)||
       !equal_bytes(base+0x81028,original_ftol_bytes,6)){
        report_error("Loaded client base, PE layout or original integer-conversion code differs. No timing changes were applied.");return 0;
    }
    if(!equal_bytes(base+0x664b2,client_profile->clock,35)||!equal_bytes(base+0x1396e,client_profile->limiter,5)){
        report_error("Client timing instructions were modified at runtime. No portable timing changes were applied.");return 0;
    }
    for(i=0;i<8;i++)if(!equal_bytes(base+animation_sites[i],client_profile->animation[i],5)){
        report_error("An animation timing call was modified at runtime. No portable timing changes were applied.");return 0;
    }
    if(GetPrivateProfileIntW(L"AspectRatio",L"Cinematics4x3",1,ini_path)){
        static const U8 expected[9]={0x55,0xff,0x50,0x14,0xe9,0xd9,0,0,0};
        g_movie_filter=GetPrivateProfileIntW(L"AspectRatio",L"CinematicFilter",1,ini_path)!=0;
        g_movie_tail=base+0x7b16d;
        near_call(replacement,base+0x7b08b,movie_aspect_entry);
        replacement[0]=0xe9;
        for(i=5;i<9;i++)replacement[i]=0x90;
        if(!add_edit(edits,&count,base+0x7b08b,expected,replacement,9)){
            report_error("Fullscreen movie Blt instructions differ.");return 0;
        }
        log_text("Cinematics4x3=1: queued fullscreen movie correction.\r\n");
    }
    g_original_ftol=base+0x81028;
    stock_mouse=GetPrivateProfileIntW(L"KPCFix",L"StockMouse",0,ini_path)!=0;
    input_clock_enabled=!stock_mouse && GetPrivateProfileIntW(L"KPCFix",L"InputClock",KPC_CLOCK_DEFAULT,ini_path)!=0;
    net_trace_enabled=log_handle!=INVALID_HANDLE_VALUE&&GetPrivateProfileIntW(L"Multiplayer",L"NetTrace",0,ini_path)!=0;
    sensitivity_range_percent=stock_mouse ? 100 : GetPrivateProfileIntW(L"KPCFix",L"SensitivityRangePercent",10,ini_path);
    if(sensitivity_range_percent<1||sensitivity_range_percent>100){
        report_error("SensitivityRangePercent must be an integer from 1 to 100. No client changes were applied.");return 0;
    }
    g_hud_percent=GetPrivateProfileIntW(L"HUD",L"ScalePercent",100,ini_path);
    if(!g_hud_percent)g_hud_percent=100;
    if(g_hud_percent<50||g_hud_percent>400){
        report_error("[HUD] ScalePercent must be 0 (disabled), or an integer from 50 to 400. No client changes were applied.");return 0;
    }
    g_cursor_percent=GetPrivateProfileIntW(L"Cursor",L"ScalePercent",0,ini_path);
    if(!g_cursor_percent)g_cursor_percent=g_hud_percent;
    if(g_cursor_percent<50||g_cursor_percent>400){
        report_error("[Cursor] ScalePercent must be 0 (follow HUD), or an integer from 50 to 400. No client changes were applied.");return 0;
    }
    g_tooltip_percent=GetPrivateProfileIntW(L"Tooltip",L"ScalePercent",0,ini_path);
    if(!g_tooltip_percent)g_tooltip_percent=g_hud_percent;
    if(g_tooltip_percent<50||g_tooltip_percent>400){
        report_error("[Tooltip] ScalePercent must be 0 (follow HUD), or an integer from 50 to 400. No client changes were applied.");return 0;
    }
    {
        U32 seconds=GetPrivateProfileIntW(L"Multiplayer",L"ServerTimeoutSeconds",15,ini_path);
        if(seconds&&(seconds<5||seconds>120)){
            report_error("[Multiplayer] ServerTimeoutSeconds must be 0 (stock 120 s) or an integer from 5 to 120. No client changes were applied.");return 0;
        }
        g_net_timeout_enabled=seconds!=0;g_net_timeout_seconds=(float)seconds;
    }
    min_step_ms=GetPrivateProfileIntW(L"KPCFix",L"LocalServerMinStepMs",0,ini_path);
    if(min_step_ms>50){
        report_error("[KPCFix] LocalServerMinStepMs must be 0 (off) or an integer from 1 to 50. No client changes were applied.");return 0;
    }
    g_min_step_seconds=(float)min_step_ms*0.001f;
    g_focus_enabled=GetPrivateProfileIntW(L"Multiplayer",L"KeepAudioOnFocusLoss",1,ini_path)!=0;
    g_loading_enabled=GetPrivateProfileIntW(L"Multiplayer",L"CampaignLoadingArt",1,ini_path)!=0;
    g_loading_min_ms=GetPrivateProfileIntW(L"Multiplayer",L"CampaignLoadingMinimumMs",5000,ini_path);
    if(g_loading_min_ms>30000){
        report_error("[Multiplayer] CampaignLoadingMinimumMs must be an integer from 0 to 30000. No client changes were applied.");return 0;
    }
    g_lock_input_rate=!stock_mouse && GetPrivateProfileIntW(L"KPCFix",L"LockInputRate",1,ini_path)!=0;
    g_hide_input_rate=g_lock_input_rate&&GetPrivateProfileIntW(L"KPCFix",L"HideInputRate",1,ini_path)!=0;
    normalize_configs_enabled=g_lock_input_rate&&GetPrivateProfileIntW(L"KPCFix",L"NormalizeInputRateConfigs",1,ini_path)!=0;
    sensitivity_maximum=(float)sensitivity_range_percent/5.0f;
    sensitivity_minimum=(float)sensitivity_range_percent/1000.0f;
    sensitivity_inverse=500.0f/(float)sensitivity_range_percent;
    if(input_clock_enabled){
        static const U8 expected[6]={0xff,0x15,0x88,0x71,0xd3,0x01};
        static const U32 sites[2]={0x33c91,0x342c3};
        precise_input_clock=(InputClock)dword(base+0x97278);
        if(!precise_input_clock){report_error("The input timeGetTime import is unavailable. No client edits were applied.");return 0;}
        for(i=0;i<2;i++){
            near_call(replacement,base+sites[i],kpc_input_clock);replacement[5]=0x90;
            if(!add_edit(edits,&count,base+sites[i],expected,replacement,6)){
                report_error("A reviewed input clock call differs. No portable client edits were applied.");return 0;
            }
        }
        {
            static const U8 before[10]={0x8b,0x54,0x24,0x24,0x8b,0x8e,0x00,0x01,0x00,0x00};
            near_call(replacement,base+0x344a5,input_interval_guard);
            replacement[5]=0xe9;store32(replacement+6,0x344d1-(0x344a5+10));
            if(!add_edit(edits,&count,base+0x344a5,before,replacement,10)){
                report_error("The original input interval block differs. No client edits were applied.");return 0;
            }
        }
    }
    {
        U8 clock_edit[35];
        copy_bytes(clock_edit,clock_v2,35);
        if(min_step_ms){
            clock_edit[0]=0xe9;store32(clock_edit+1,(U32)local_server_min_step-((U32)base+0x664b2+5));
            for(i=5;i<35;i++)clock_edit[i]=0x90;
        }
        if(!add_edit(edits,&count,base+0x664b2,client_profile->clock,clock_edit,35)||
           !add_edit(edits,&count,base+0x1396e,client_profile->limiter,limiter_v2,5))return 0;
        for(i=0;i<8;i++){
            near_call(replacement,base+animation_sites[i],carry_helpers[i]);
            if(!add_edit(edits,&count,base+animation_sites[i],client_profile->animation[i],replacement,5))return 0;
        }
    }
    if(g_lock_input_rate){
        static const U32 sites[2]={0x1aa0e,0x1ac90};
        U8 before[5];
        for(i=0;i<2;i++){
            near_call(before,base+sites[i],base+0x1a870);
            near_call(replacement,base+sites[i],kpc_rate_value);
            if(!add_edit(edits,&count,base+sites[i],before,replacement,5)){
                report_error("The original console value setter calls differ. No client changes were applied.");return 0;
            }
        }
    }
    copy_bytes(before_iat,base+0x971f0,4);original_load_library=(LoadA)dword(before_iat);
    if(!original_load_library){report_error("The client's LoadLibrary import is unavailable. No portable fixes were applied.");return 0;}
    store32(after_iat,(U32)kpc_load_library_a);
    if(!add_edit(edits,&count,base+0x971f0,before_iat,after_iat,4))return 0;
    if(!queue_bind_fix(edits,&count,base)){report_error("Bind address patch validation failed. No client edits applied.");return 0;}
    if(g_focus_enabled){
        static const U32 sites[2]={0x6d5c,0x6cdb},targets[2]={0x74b90,0x74c10};
        void *const hooks[2]={(void *)kpc_focus_release,(void *)kpc_focus_restore};
        U8 before[5];
        g_focus_client=base;
        for(i=0;i<2;i++){
            near_call(before,base+sites[i],base+targets[i]);near_call(replacement,base+sites[i],hooks[i]);
            if(!add_edit(edits,&count,base+sites[i],before,replacement,5)){
                report_error("The original focus-change sound calls differ. No client edits were applied.");return 0;
            }
        }
    }
    if(g_net_timeout_enabled){
        static const U8 before[14]={0x8b,0x4e,0x30,0xd9,0x46,0x74,0xd8,0x59,0x4c,0xdf,0xe0,0xf6,0xc4,0x41};
        static const U8 tail[2]={0x75,0x33};
        if(!equal_bytes(base+0x4b50a,tail,2)){report_error("The original connection timeout branch differs. No client edits were applied.");return 0;}
        near_call(replacement,base+0x4b4fc,net_timeout_compare);
        for(i=5;i<14;i++)replacement[i]=0x90;
        if(!add_edit(edits,&count,base+0x4b4fc,before,replacement,14)){
            report_error("The original connection timeout comparison differs. No client edits were applied.");return 0;
        }
    }
    if(!queue_net_trace(edits,&count,base)){report_error("Network trace import transaction could not be prepared. No client edits were applied.");return 0;}
    if(!GetModuleHandleExW(5,(const WCHAR *)self_module,&pinned)||pinned!=self_module){report_error("The portable module could not be pinned for safe callback lifetime. No fixes were applied.");return 0;}
    if(!commit_edits(edits,count)){report_error("Client patch transaction failed validation or protection/cache updates. Use the tested V3/V5 setup.");return 0;}
    log_text(net_trace_enabled?"NET trace v1 active: original Winsock calls forwarded; original browser clock retained.\r\n":"NET trace inactive; original networking imports retained.\r\n");
    if(g_lock_input_rate){
        typedef U32 (CDECL *Set)(void *,const char *,const char *,U32,U32,U32,U32);
        typedef void *(CDECL *Lookup)(void *,const char *);
        /* Original lookup safely returns NULL before its hash table exists.
         * Update an existing variable, never register one before console init.
         * Descriptor+0x0c is an optional HANDLE-DESTINATION ADDRESS, not a live
         * record pointer; InputRate's descriptor has no such destination. */
        if(((Lookup)(base+0x1b010))(base+0xab3d8,"InputRate"))
            ((Set)(base+0x1abd0))(base+0xab3d8,"InputRate","0",0,0,0,0);
        *(U32 *)(base+0xab668)=0;
        log_text("InputRate lock: console strings, numeric binding and profile cache use zero.\r\n");
        if(normalize_configs_enabled)kpc_normalize_configs(main_path);
    }
    kpc_status|=TIMING_READY|ANIMATION_READY;
    kpc_status|=TIMING_RUNTIME|ANIMATION_RUNTIME;
    log_text("Timing V3: clock guard and 8 independent carries applied in memory.\r\n");
    if(min_step_ms){log_text("LocalServerMinStepMs=");log_hex(min_step_ms);log_text(": EXPERIMENTAL minimum local-server step; shorter frames are carried, not simulated.\r\n");}
    if(g_focus_enabled)log_text("Focus loss: remote multiplayer keeps server-owned sounds running (single player and menus stock).\r\n");
    if(g_net_timeout_enabled){log_text("Server timeout: in-world multiplayer connection seconds=");log_hex((U32)g_net_timeout_seconds);log_text(" (stock 120 s elsewhere).\r\n");}
    log_text(input_clock_enabled?"Input clock v2: timeGetTime plus delayed/zero interval guards; no global clock hook.\r\n":"InputClock=0: original input clocks and intervals retained; Timing V3/Ground V5 remain enabled.\r\n");
    if(GetModuleHandleExW(0,L"cshell.dll",&loaded)){
        i=(U32)patch_shell(loaded);
        /* Balance the temporary reference outside the loader-lock callback. */
        {extern BOOL WINAPI FreeLibrary(HMODULE);FreeLibrary(loaded);}
        if(!i)return 0;
    }else log_text("Movement: waiting for client LoadLibraryA to load cshell.dll.\r\n");
    n=count;log_text("Startup client edits: ");log_hex(n);log_text(". Executable files on disk remain unchanged.\r\n");return 1;
}
static void ensure_game(void){
    int entered=enter_once(&game_state,&game_thread),ok;
    if(entered!=1)return;
    ok=install_game_fixes();finish_once(&game_state,ok?2:3);
}

HRESULT WINAPI DirectInputCreateA(HINSTANCE instance,DWORD version,void **out,void *outer){
    typedef HRESULT (WINAPI *Fn)(HINSTANCE,DWORD,void **,void *);DWORD saved=GetLastError();
    if(!ensure_backend()){if(out)*out=NULL;return E_FAIL;}
    ensure_game();SetLastError(saved);return ((Fn)real_input[0])(instance,version,out,outer);
}
HRESULT WINAPI DirectInputCreateW(HINSTANCE instance,DWORD version,void **out,void *outer){
    typedef HRESULT (WINAPI *Fn)(HINSTANCE,DWORD,void **,void *);DWORD saved=GetLastError();
    if(!ensure_backend()){if(out)*out=NULL;return E_FAIL;}
    ensure_game();SetLastError(saved);return ((Fn)real_input[1])(instance,version,out,outer);
}
HRESULT WINAPI DirectInputCreateEx(HINSTANCE instance,DWORD version,const void *iid,void **out,void *outer){
    typedef HRESULT (WINAPI *Fn)(HINSTANCE,DWORD,const void *,void **,void *);DWORD saved=GetLastError();
    if(!ensure_backend()){if(out)*out=NULL;return E_FAIL;}
    ensure_game();SetLastError(saved);return ((Fn)real_input[2])(instance,version,iid,out,outer);
}
HRESULT WINAPI DllGetClassObject(const void *clsid,const void *iid,void **out){
    typedef HRESULT (WINAPI *Fn)(const void *,const void *,void **);DWORD saved=GetLastError();
    if(!ensure_backend()||!real_input[3]){if(out)*out=NULL;return E_FAIL;}
    ensure_game();SetLastError(saved);return ((Fn)real_input[3])(clsid,iid,out);
}
HRESULT WINAPI DllCanUnloadNow(void){return S_FALSE;}
HRESULT WINAPI DllRegisterServer(void){
    typedef HRESULT (WINAPI *Fn)(void);DWORD saved=GetLastError();
    if(!ensure_backend()||!real_input[5])return E_NOTIMPL;SetLastError(saved);return ((Fn)real_input[5])();
}
HRESULT WINAPI DllUnregisterServer(void){
    typedef HRESULT (WINAPI *Fn)(void);DWORD saved=GetLastError();
    if(!ensure_backend()||!real_input[6])return E_NOTIMPL;SetLastError(saved);return ((Fn)real_input[6])();
}
const char *WINAPI KPCFixVersion(void){return KPC_VERSION ": Timing V3 + Ground V5 + Input Clock V2 option + Sensitivity Range V1 + HUD V3 + Menu/InputRate V4 + Cursor/Tooltip V5";}
U32 WINAPI KPCFixStatus(void){return kpc_status;}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void *reserved){
    (void)reserved;
    if(reason==1){self_module=instance;DisableThreadLibraryCalls(instance);}
    return 1;
}
