#include "winmini.h"
#include "sha256.h"
#include "runtime_data.h"
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
static int paths_ready,hooked,active,initial_cheats,spawn_warning_checked;
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
/* Some stock campaign-map spawn warnings contain a literal trailing newline.
 * The engine appends its own line ending, so trim only that exact suffix in
 * the already-validated in-memory object image. Never change object.lto on disk.
 * A warning without an embedded newline is deliberately left untouched. */
static int trim_spawn_warning(U8 *b){
 static const U8 message[]="Error!! Could not find any multiplayer start points. Looking for a singleplayer start point.";
 U32 pe=rd(b+60),sections,count,s,start,size,i,n=sizeof(message)-1;
 if(pe>0x1000-0x100)return -1;
 count=*(U16 *)(b+pe+6);
 sections=pe+24+*(U16 *)(b+pe+20);
 if(!count||count>32||sections>0x1000-count*40)return -1;
 for(s=0;s<count;s++){
  const U8 *header=b+sections+s*40;
  start=rd(header+12);size=rd(header+8);
  if(start>=0x196000||size>0x196000-start||size<n+5)continue;
  for(i=0;i+n+5<=size;i++){
   U8 *at=b+start+i,*tail;U32 extra=0,old,restore;
   if(!same(at,message,n))continue;
   tail=at+n;
   while(extra<4&&(tail[extra]=='\\r'||tail[extra]=='\\n'))extra++;
   if(!extra||tail[extra]!=0)return 2;
   if(!VirtualProtect(tail,1,0x40,&old))return -1;
   tail[0]=0;
   if(!VirtualProtect(tail,1,old,&restore))
    fatal("[KPCFix Server Tools] ERROR: cannot restore spawn warning memory protection.");
   return 1;
  }
 }
 return 0;
}

/* Preserve the code already tested in object.lto, but install only loaded-memory
 * code/data edits. PE headers and relocation tables are never patched here. */
static int apply_object(HMODULE module){
 U8 *b=(U8 *)module;U8 hash[32],expected[8];U32 i,j,old,restore,already=1;
 U32 mbi[7],delta=(U32)b-0x10000000u;
 if(!module_hash(module,hash)||!same(hash,object_hash,32)||!valid_image(b,0x196000))return 0;
 for(i=0;i<PATCH_COUNT;i++)if(!same(b+patches[i].rva,patches[i].after,patches[i].size))already=0;
 if(already){
  if(b[HOOK_RVA+INITIAL_OFFSET]>1||!same(b+0x116550,noclip_init,sizeof(noclip_init)))return 0;
  for(i=0;i<sizeof(hook_code);i++)if(i!=INITIAL_OFFSET&&b[HOOK_RVA+i]!=hook_code[i])return 0;
  return 1;
 }
 for(i=0;i<PATCH_COUNT;i++){
  copy(expected,patches[i].before,patches[i].size);
  if(patches[i].reloc>=0)*(U32 *)(expected+patches[i].reloc)+=delta;
  if(!same(b+patches[i].rva,expected,patches[i].size))return 0;
 }
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
  if(!spawn_warning_checked){
   int trim=trim_spawn_warning((U8 *)module);
   spawn_warning_checked=1;
   if(trim==1)log_file("[KPCFix Server Tools] Removed embedded newline from stock spawn warning (memory only).");
   else if(trim==2)log_file("[KPCFix Server Tools] Stock spawn warning has no embedded newline; spacing unchanged.");
   else if(trim==0)log_file("[KPCFix Server Tools] Stock spawn warning not found in object module; spacing unchanged.");
   else log_file("[KPCFix Server Tools] Could not inspect stock spawn warning; spacing unchanged.");
  }
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
