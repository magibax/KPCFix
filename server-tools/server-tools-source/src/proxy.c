#include "winmini.h"
#include "sha256.h"
#include "runtime_data.h"
#include "coop_spawn.h"
#include "coop_intersect_abi.h"
extern HANDLE WINAPI GetStdHandle(DWORD);
extern void WINAPI ExitProcess(UINT) __attribute__((noreturn));
extern DWORD WINAPI VirtualQuery(const void *,void *,DWORD);
extern void CDECL runtime_print(void *,const char *,U32);
typedef I32 (CDECL *CreateFn)(U32,const char *,void **);
typedef void (CDECL *DeleteFn)(void *);
static HMODULE self,backend;
static CreateFn create_real;
static DeleteFn delete_real;
static WCHAR backend_path[1024],ini_path[1024],log_path[1024];
static int paths_ready,hooked,active,initial_cheats;
static U32 object_installs;
static U32 len(const char *s){U32 n=0;while(s[n])n++;return n;}
static int eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int same(const U8 *a,const U8 *b,U32 n){U32 i;for(i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void copy(U8 *d,const U8 *s,U32 n){U32 i;for(i=0;i<n;i++)d[i]=s[i];}
static U32 rd(const U8 *p){return *(const U32 *)p;}
static void paths(void){
 WCHAR path[1024];U32 n,i,j;
 if(paths_ready)return;
 n=GetModuleFileNameW(self,path,1024);if(!n||n>=1024)ExitProcess(1);
 i=n;while(i&&path[i-1]!='\\'&&path[i-1]!='/')i--;
 if(i>970)ExitProcess(1);
 for(j=0;j<i;j++)backend_path[j]=ini_path[j]=log_path[j]=path[j];
 {const WCHAR *s=(const WCHAR *)L"server.original.dll";for(j=0;(backend_path[i+j]=s[j])!=0;j++);}
 {const WCHAR *s=(const WCHAR *)L"KPCServerTools.ini";for(j=0;(ini_path[i+j]=s[j])!=0;j++);}
 {const WCHAR *s=(const WCHAR *)L"KPCServerTools.log";for(j=0;(log_path[i+j]=s[j])!=0;j++);}
 paths_ready=1;
}
static void log_file(const char *s){
 HANDLE f;U32 wrote;paths();
 f=CreateFileW(log_path,4,3,NULL,4,0x80,NULL);
 if(f!=INVALID_HANDLE_VALUE){WriteFile(f,s,len(s),&wrote,NULL);WriteFile(f,"\r\n",2,&wrote,NULL);CloseHandle(f);}
}
static void __attribute__((noreturn)) fatal(const char *s){
 HANDLE h=GetStdHandle((DWORD)-11);U32 wrote;
 log_file(s);
 if(h&&h!=INVALID_HANDLE_VALUE){WriteFile(h,s,len(s),&wrote,NULL);WriteFile(h,"\r\n",2,&wrote,NULL);}
 ExitProcess(1);
}
static int file_hash(const WCHAR *p,U8 out[32]){
 HANDLE f;LARGE_INTEGER size;KpcSha state;U8 buf[4096];U32 got,total=0;int ok=0;
 f=CreateFileW(p,0x80000000u,1,NULL,3,0x80,NULL);
 if(f==INVALID_HANDLE_VALUE)return 0;
 if(!GetFileSizeEx(f,&size)||size.high||!size.low||size.low>4*1024*1024)goto done;
 kpc_sha_init(&state);
 for(;;){if(!ReadFile(f,buf,sizeof(buf),&got,NULL))goto done;if(!got)break;kpc_sha_update(&state,buf,got);total+=got;}
 if(total!=size.low)goto done;
 kpc_sha_final(&state,out);ok=1;
done:CloseHandle(f);return ok;
}
static int module_hash(HMODULE m,U8 out[32]){
 WCHAR p[1024];U32 n=GetModuleFileNameW(m,p,1024);
 return n&&n<1024&&file_hash(p,out);
}
static int valid_image(U8 *b,U32 image_size){
 U32 p;if(*(U16 *)b!=0x5a4d)return 0;p=rd(b+60);
 return p<0x1000&&rd(b+p)==0x4550&&*(U16 *)(b+p+4)==0x14c&&rd(b+p+24+56)==image_size;
}
/* Preserve the code already tested in object.lto, but install only loaded-memory
 * code/data edits. PE headers and relocation tables are never patched here. */

/* Co-op research: opt-in, read-only observation of the original player-spawn
 * position setter. This callsite is inside the *original* 1.13 object image;
 * guard the original bytes before installing the redirect. */
#define SPAWN_POS_CALL_RVA 0x0f13c4u
#define SPAWN_PREPARE_CALL_RVA 0x0f13a4u
#define SPAWN_PREPARE_TARGET_RVA 0x0f10c0u
#define SPAWN_POS_GLOBAL_RVA 0x16de80u
static const U8 original_spawn_pos_call[6]={0xff,0x91,0xe0,0x01,0x00,0x00};
static const U8 original_prepare_call[5]={0xe8,0x17,0xfd,0xff,0xff};
typedef void (CDECL *SpawnSetPos)(void *,const U32 *);
static U8 *spawn_image;
static int spawn_trace_enabled,spawn_separation_enabled,collision_trace_enabled;
static volatile U32 spawn_trace_count,collision_trace_count;
#define COLLISION_FILTER_RVA 0x102730u
#define COLLISION_DIAG_LIMIT 9u
typedef U8 (CDECL *KpcIntersectFn)(const KpcIntersectQuery *,KpcIntersectInfo *);
static float collision_float(U32 bits){union{U32 bits;float value;}u;u.bits=bits;return u.value;}
static U32 collision_bits(float value){union{U32 bits;float value;}u;u.value=value;return u.bits;}

static CoopSpawnReservations spawn_trial_reservations;
static void trace_append(char *out,U32 *n,const char *s){
 while(*s&&*n<115)out[(*n)++]=*s++;
}
static void trace_hex(char *out,U32 *n,U32 value){
 static const char hex[]="0123456789abcdef";int i;
 trace_append(out,n,"0x");
 for(i=7;i>=0&&*n<115;i--)out[(*n)++]=hex[(value>>(i*4))&15u];
}
/* Evidence from original object.lto RVA f2fae/f2fb4:
 * API slot +0x90, two stack pointer args, 8-byte caller cleanup, bool in AL.
 * Original callback at +0x102730 filters the supplied user object.
 * Diagnostic only: a downward RAY is NOT a player collision-volume test.
 * Each test leaves all player transforms and engine game state unchanged.
 */
static void diagnostic_intersect_segment(const U32 *object_pos,void *object){
 KpcIntersectQuery query={0};
 KpcIntersectInfo hit={0};
 U32 *engine;
 U8 result;
 char line[116];U32 n=0;
 float x=collision_float(object_pos[0]),y=collision_float(object_pos[1]),z=collision_float(object_pos[2]);
 engine=spawn_image?*(U32 **)(spawn_image+SPAWN_POS_GLOBAL_RVA):0;
 if(!engine||!engine[KPC_INTERSECT_SERVER_SLOT/4]){
  log_file("[KPCFix Co-op] ABI segment unavailable (no engine pointer).");
  return;
 }
 query.from.x=x;query.from.y=y+64.f;query.from.z=z;
 query.to.x=x;query.to.y=y-256.f;query.to.z=z;
 query.flags=KPC_INTERSECT_OBJECTS|KPC_INTERSECT_HPOLY;
 query.object_filter=(U32)(spawn_image+COLLISION_FILTER_RVA);
 query.filter_userdata=(U32)object;
 result=((KpcIntersectFn)engine[KPC_INTERSECT_SERVER_SLOT/4])(&query,&hit);
 trace_append(line,&n,"[KPCFix Co-op] ABI segment hit=");
 if(n<115)line[n++]=result?'1':'0';
 trace_append(line,&n," obj=");trace_hex(line,&n,(U32)object);
 if(result){
  trace_append(line,&n," y=");trace_hex(line,&n,collision_bits(hit.point.y));
  trace_append(line,&n," ny=");trace_hex(line,&n,collision_bits(hit.plane.normal.y));
 }
 line[n]=0;log_file(line);
}
/* The native game call pushes (HOBJECT, LTVector*) and cleans eight bytes
 * in the caller. Forward it unchanged and only then write an observation. */
static void CDECL trace_spawn_position(void *object,const U32 *pos){
 U32 *engine=spawn_image?*(U32 **)(spawn_image+SPAWN_POS_GLOBAL_RVA):0;
 if(!engine||!engine[0x1e0/4])fatal("[KPCFix Co-op] Missing original position-set API.");
 ((SpawnSetPos)engine[0x1e0/4])(object,pos);
 if(pos&&__sync_fetch_and_add(&spawn_trace_count,1)<64){
  char line[116];U32 n=0;
  trace_append(line,&n,"[KPCFix Co-op] spawn position obj=");
  trace_hex(line,&n,(U32)object);
  trace_append(line,&n," x=");trace_hex(line,&n,pos[0]);
  trace_append(line,&n," y=");trace_hex(line,&n,pos[1]);
  trace_append(line,&n," z=");trace_hex(line,&n,pos[2]);
  line[n]=0;log_file(line);
 }
 /* Limit opt-in live ABI tests to three known-good R1M2B coordinates.
  * The result is logged AFTER the original engine position assignment.
  * This neither changes spawn positions nor disables telefragging. */
 if(collision_trace_enabled&&pos&&
    pos[0]==0x44c80000u&&pos[1]==0xc3800000u&&
    (pos[2]==0xc5380000u||pos[2]==0xc5320000u||pos[2]==0xc53e0000u)&&
    __sync_fetch_and_add(&collision_trace_count,1)<COLLISION_DIAG_LIMIT)
  diagnostic_intersect_segment(pos,object);
}
/* RVA f13a4 precedes the engine's nearby-object / telefrag handling.
 * The callee is a thiscall method (ECX=player, ret 12). Adjust the private
 * local copy of the spawn vector BEFORE forwarding the original invocation.
 * This is an experimental trial, not a collision-validated placement API. */
typedef void (__attribute__((thiscall)) *SpawnPrepareFn)(void *,U32 *,const U32 *,const U32 *);
static void __attribute__((thiscall)) coop_before_telefrag(void *player,U32 *pos,const U32 *a,const U32 *b){
 if(spawn_separation_enabled&&coop_is_r1m2b_anchor(pos)){
  int slot=coop_claim_slot(&spawn_trial_reservations,(U32)player);
  if(slot>=0){
   coop_assign_trial_position(pos,slot);
   if(spawn_trace_count<64){
    char line[116];U32 n=0;
    trace_append(line,&n,"[KPCFix Co-op] trial slot=");
    if(n<115)line[n++]=(char)('0'+slot);
    trace_append(line,&n," player=");trace_hex(line,&n,(U32)player);
    trace_append(line,&n," z=");trace_hex(line,&n,pos[2]);
    line[n]=0;log_file(line);
   }
  }else{
   log_file("[KPCFix Co-op] WARNING: trial full (3 slots). Set maxclients=3; original fallback may telefrag.");
  }
 }
 ((SpawnPrepareFn)(spawn_image+SPAWN_PREPARE_TARGET_RVA))(player,pos,a,b);
}
static void make_prepare_call(U8 *out,U8 *site){
 U32 relative=(U32)coop_before_telefrag-((U32)site+5);
 out[0]=0xe8;copy(out+1,(const U8 *)&relative,4);
}

static void make_spawn_call(U8 *out,U8 *site){
 U32 relative=(U32)trace_spawn_position-((U32)site+5);
 out[0]=0xe8;copy(out+1,(const U8 *)&relative,4);out[5]=0x90;
}

static int apply_object(HMODULE module){
 U8 *b=(U8 *)module;U8 hash[32],expected[8],spawn_call[6],prepare_call[5];U32 i,j,old,restore,already=1;
 U32 mbi[7],delta=(U32)b-0x10000000u;
 if(!module_hash(module,hash)||!same(hash,object_hash,32)||!valid_image(b,0x196000))return 0;
 make_spawn_call(spawn_call,b+SPAWN_POS_CALL_RVA);
 make_prepare_call(prepare_call,b+SPAWN_PREPARE_CALL_RVA);
 for(i=0;i<PATCH_COUNT;i++)if(!same(b+patches[i].rva,patches[i].after,patches[i].size))already=0;
 if(already){
  if(!same(b+SPAWN_POS_CALL_RVA,spawn_trace_enabled?spawn_call:original_spawn_pos_call,6))return 0;
  if(!same(b+SPAWN_PREPARE_CALL_RVA,spawn_separation_enabled?prepare_call:original_prepare_call,5))return 0;
  if(b[HOOK_RVA+INITIAL_OFFSET]>1||!same(b+0x116550,noclip_init,sizeof(noclip_init)))return 0;
  for(i=0;i<sizeof(hook_code);i++)if(i!=INITIAL_OFFSET&&b[HOOK_RVA+i]!=hook_code[i])return 0;
  return 1;
 }
 for(i=0;i<PATCH_COUNT;i++){
  copy(expected,patches[i].before,patches[i].size);
  if(patches[i].reloc>=0)*(U32 *)(expected+patches[i].reloc)+=delta;
  if(!same(b+patches[i].rva,expected,patches[i].size))return 0;
 }
 if(!same(b+SPAWN_POS_CALL_RVA,original_spawn_pos_call,6))return 0;
 if(!same(b+SPAWN_PREPARE_CALL_RVA,original_prepare_call,5))return 0;
 for(i=0;i<sizeof(hook_code);i++)if(b[HOOK_RVA+i])return 0;
 for(i=0;i<sizeof(noclip_init);i++)if(b[0x116550+i])return 0;
 /* The known .data section's final page includes unused, writable padding.
  * Reserve it only after confirming it is committed, writable and all zero. */
 if(VirtualQuery(b+0x174f00,mbi,sizeof(mbi))!=sizeof(mbi)||mbi[4]!=0x1000||
    !(mbi[5]&0x0c)||(mbi[5]&0x100)||mbi[0]>((U32)b+0x174f00)||
    mbi[0]+mbi[3]<((U32)b+0x174f90))return 0;
 for(i=0;i<0x90;i++)if(b[0x174f00+i])return 0;
 if(rd(b+0x16d950)!=3&&rd(b+0x16d950)!=0)return 0;
 if(!VirtualProtect(b+0x1000,0x116000,0x40,&old))return 0;
 copy(b+HOOK_RVA,hook_code,sizeof(hook_code));
 b[HOOK_RVA+INITIAL_OFFSET]=(U8)initial_cheats;
 copy(b+0x116550,noclip_init,sizeof(noclip_init));
 for(i=0;i<PATCH_COUNT;i++)for(j=0;j<patches[i].size;j++)b[patches[i].rva+j]=patches[i].after[j];
 if(spawn_trace_enabled||spawn_separation_enabled){
  spawn_image=b;spawn_trace_count=0;collision_trace_count=0;
  for(i=0;i<COOP_TRIAL_SLOTS;i++)spawn_trial_reservations.player[i]=0;
  if(spawn_trace_enabled)copy(b+SPAWN_POS_CALL_RVA,spawn_call,6);
  if(spawn_separation_enabled)copy(b+SPAWN_PREPARE_CALL_RVA,prepare_call,5);
 }
 /* LoadLibrary has already applied relocations and may have run table init. */
 *(U32 *)(b+0x16d950)&=~3u;
 if(!VirtualProtect(b+0x1000,0x116000,old,&restore)||
    !FlushInstructionCache(GetCurrentProcess(),b+0x1000,0x116000))
  fatal("[KPCFix Server Tools] ERROR: could not finalize runtime hooks.");
 object_installs++;
 {const char *message="[KPCFix Server Tools] Runtime hooks installed; original object.lto unchanged.";runtime_print(b,message,len(message));}
 log_file("[KPCFix Server Tools] Runtime object hooks installed; file unchanged.");
 return 1;
}
static FARPROC WINAPI proxy_get_proc(HMODULE module,const char *name){
 FARPROC p=GetProcAddress(module,name);
 if(active&&(U32)name>65535&&eq(name,"ObjectDLLSetup")){
  if(!p||!apply_object(module))fatal("[KPCFix Server Tools] ERROR: unsupported or altered object.lto; run Setup-KPCServerTools.cmd.");
 }
 return p;
}
static int intercept_backend(void){
 U8 *b=(U8 *)backend;U32 p=rd(b+60),rva=rd(b+p+24+96+8),old,restore;
 U32 *desc=(U32 *)(b+rva),*names,*slots,i;
 while(desc[3]){
  if(desc[0]){
   names=(U32 *)(b+desc[0]);slots=(U32 *)(b+desc[4]);
   for(i=0;names[i];i++){
    if(!(names[i]&0x80000000u)&&eq((char *)(b+names[i]+2),"GetProcAddress")){
     if(slots[i]==(U32)proxy_get_proc)return 1;
     if(slots[i]!=(U32)GetProcAddress(GetModuleHandleW((const WCHAR *)L"kernel32.dll"),"GetProcAddress"))return 0;
     if(!VirtualProtect(&slots[i],4,4,&old))return 0;
     slots[i]=(U32)proxy_get_proc;
     if(!VirtualProtect(&slots[i],4,old,&restore))return 0;
     return 1;
    }
   }
  }
  desc+=5;
 }
 return 0;
}
static void initialize(void){
 U8 exe_sha[32],back_sha[32];int profile=-1,i;U32 pe_size;
 paths();
 if(!module_hash(NULL,exe_sha))fatal("[KPCFix Server Tools] ERROR: cannot identify the host executable.");
 for(i=0;i<2;i++)if(same(exe_sha,exe_hashes[i],32))profile=i;
 if(!file_hash(backend_path,back_sha))fatal("[KPCFix Server Tools] ERROR: missing server.original.dll; run setup first.");
 if(profile>=0){if(!same(back_sha,backend_hashes[profile],32))fatal("[KPCFix Server Tools] ERROR: dedicated executable and original server DLL do not match.");}
 else if(!same(back_sha,backend_hashes[0],32)&&!same(back_sha,backend_hashes[1],32))
  fatal("[KPCFix Server Tools] ERROR: unsupported original server DLL.");
 if(!backend){
  backend=LoadLibraryW(backend_path);
  if(!backend)fatal("[KPCFix Server Tools] ERROR: original server DLL could not be loaded.");
  pe_size=same(back_sha,backend_hashes[0],32)?0x51000:0x55000;
  if(!valid_image((U8 *)backend,pe_size))fatal("[KPCFix Server Tools] ERROR: original engine image layout mismatch.");
  create_real=(CreateFn)GetProcAddress(backend,"CreateServer");
  delete_real=(DeleteFn)GetProcAddress(backend,"DeleteServer");
  if(!create_real||!delete_real)fatal("[KPCFix Server Tools] ERROR: missing original server exports.");
 }
 active=profile>=0&&GetPrivateProfileIntW((const WCHAR *)L"ServerTools",(const WCHAR *)L"Enabled",1,ini_path)!=0;
 initial_cheats=GetPrivateProfileIntW((const WCHAR *)L"ServerTools",(const WCHAR *)L"AllowCheats",0,ini_path)!=0;
 spawn_separation_enabled=active&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TrialSeparateSpawns",0,ini_path)!=0;
 collision_trace_enabled=active&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceCollisionABI",0,ini_path)!=0;
 spawn_trace_enabled=active&&(spawn_separation_enabled||collision_trace_enabled||
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceSpawns",0,ini_path)!=0);
 if(active&&!hooked){
  if(!intercept_backend())fatal("[KPCFix Server Tools] ERROR: could not install engine loading hook.");
  hooked=1;
 }
 log_file(active?"[KPCFix Server Tools] Dedicated DLL active.":"[KPCFix Server Tools] Original engine pass-through.");
}
I32 CDECL CreateServer(U32 version,const char *name,void **out){
 initialize();return create_real(version,name,out);
}
void CDECL DeleteServer(void *server){if(delete_real)delete_real(server);}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void *reserved){
 (void)reserved;if(reason==1){self=module;DisableThreadLibraryCalls(module);}return 1;
}
