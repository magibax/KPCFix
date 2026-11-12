#include "winmini.h"
/* Not in the shared winmini.h (kept byte-identical with the client). */
extern U32 WINAPI GetPrivateProfileStringW(const WCHAR *,const WCHAR *,const WCHAR *,WCHAR *,U32,const WCHAR *);
#include "sha256.h"
#include "runtime_data.h"
#include "coop_spawn.h"
#include "coop_intersect_abi.h"
#include "coop_volume_probe.h"
#include "coop_bsp_box.h"
#include "coop_model_registry.h"
#include "coop_aabb_overlap.h"
#include "coop_model_state.h"
#include "coop_session.h"
#include "coop_scoreboard.h"
#include "coop_script_clock.h"
#include "coop_inventory.h"
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
   while(extra<4&&(tail[extra]=='\r'||tail[extra]=='\n'))extra++;
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
#define COLLISION_DIAG_LIMIT 3u /* at most 3 known player locations */
typedef U8 (CDECL *KpcIntersectFn)(const KpcIntersectQuery *,KpcIntersectInfo *);
static float collision_float(U32 bits){union{U32 bits;float value;}u;u.bits=bits;return u.value;}
static U32 collision_bits(float value){union{U32 bits;float value;}u;u.value=value;return u.bits;}

static int clearance_enabled,bsp_volume_enabled,model_registry_enabled,model_state_enabled,coop_spread_bsp;
static const U8 bsp_original_hash[32]={0x83,0xb7,0xbc,0xe8,0x09,0x94,0x8b,0x6f,0x70,0xed,0x3f,0x35,0xa7,0x7a,0x77,0x1b,0x60,0x87,0xf1,0x10,0x4d,0x90,0xd0,0x6a,0x62,0xac,0x3d,0xc0,0xd4,0xbd,0x37,0xd5};
static volatile U32 clearance_started;
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
/* Four carefully bounded, read-only queries at each of the three known
 * R1M2B positions. Mode 0 reproduces our live-tested all-zero +0x30 control;
 * modes 1-3 vary the previously omitted original query word and flags/range.
 * The original game at RVA f2f7c writes 0x6003 to query+0x30, so retaining
 * that control comparison identifies whether its omission caused hit=0.
 * Even a hit is NOT sufficient to approve a player-sized spawn position. */
static void diagnostic_intersect_segment(const U32 *object_pos,void *object){
 U32 *engine;
 U32 mode;
 float x=collision_float(object_pos[0]),y=collision_float(object_pos[1]),z=collision_float(object_pos[2]);
 engine=spawn_image?*(U32 **)(spawn_image+SPAWN_POS_GLOBAL_RVA):0;
 if(!engine||!engine[KPC_INTERSECT_SERVER_SLOT/4]){
  log_file("[KPCFix Co-op] ABI segment unavailable (no engine pointer).");
  return;
 }
 for(mode=0;mode<4;mode++){
  KpcIntersectQuery query={0};
  KpcIntersectInfo hit={0};
  U8 result;
  char line[116];U32 n=0;
  query.from.x=x;query.from.y=y+(mode==3?192.f:64.f);query.from.z=z;
  query.to.x=x;query.to.y=y-(mode==3?2048.f:256.f);query.to.z=z;
  query.flags=(mode==2)?KPC_INTERSECT_HPOLY:(KPC_INTERSECT_OBJECTS|KPC_INTERSECT_HPOLY);
  query.object_filter=(U32)(spawn_image+COLLISION_FILTER_RVA);
  query.filter_userdata=(U32)object;
  query.legacy_word_30=(unsigned short)(mode==0?0:KPC_INTERSECT_ORIGINAL_WORD_30);
  result=((KpcIntersectFn)engine[KPC_INTERSECT_SERVER_SLOT/4])(&query,&hit);
  trace_append(line,&n,"[KPCFix Co-op] ray m=");
  if(n<115)line[n++]=(char)('0'+mode);
  trace_append(line,&n," flags=");
  if(n<115)line[n++]=(char)('0'+query.flags);
  trace_append(line,&n," ext=");trace_hex(line,&n,query.legacy_word_30);
  trace_append(line,&n," z=");trace_hex(line,&n,object_pos[2]);
  trace_append(line,&n," hit=");
  if(n<115)line[n++]=result?'1':'0';
  line[n]=0;log_file(line);
  if(result){
   n=0;
   trace_append(line,&n,"[KPCFix Co-op] ray+ m=");
   if(n<115)line[n++]=(char)('0'+mode);
   trace_append(line,&n," y=");trace_hex(line,&n,collision_bits(hit.point.y));
   trace_append(line,&n," ny=");trace_hex(line,&n,collision_bits(hit.plane.normal.y));
   trace_append(line,&n," obj=");trace_hex(line,&n,hit.hit_object);
   trace_append(line,&n," surf=");trace_hex(line,&n,hit.surface_flags);
   line[n]=0;log_file(line);
  }
 }
}
/* Original overlap code at 0xf1157 supplies ECX=this and pushes upper,
 * lower, HOBJECT. The callee pops its three stack arguments. */
typedef void (__attribute__((thiscall)) *ReadBounds)(void *,void *,KpcIntersectVec *,KpcIntersectVec *);

static void log_vector(const char *label,KpcIntersectVec v){
 char line[116];U32 n=0;
 trace_append(line,&n,"[KPCFix Co-op] ");trace_append(line,&n,label);
 trace_append(line,&n," x=");trace_hex(line,&n,collision_bits(v.x));
 trace_append(line,&n," y=");trace_hex(line,&n,collision_bits(v.y));
 trace_append(line,&n," z=");trace_hex(line,&n,collision_bits(v.z));
 line[n]=0;log_file(line);
}

static int world_segment(KpcIntersectFn intersect,KpcIntersectVec from,KpcIntersectVec to,KpcIntersectInfo *info){
 KpcIntersectQuery q={0};
 q.from=from;q.to=to;q.flags=KPC_INTERSECT_HPOLY;
 q.legacy_word_30=KPC_INTERSECT_ORIGINAL_WORD_30;
 return intersect(&q,info)!=0;
}


/* Experimental, opt-in, read-only BASE WORLD BSP volume differential.
 * The original 1.13 server DLL is hash-gated at initialize(). Pointer
 * provenance was recovered from its GetPointStatus wrapper at +0x2440a.
 * No code is patched here and no transforms or objects are modified.
 * Not a complete world-hull probe: movable models/actors are outside scope.
 */
static int bsp_mem_readable(U32 addr,U32 size){
 U32 m[7],base,extent,protect;
 if(!addr||!size||addr>0xffffffffu-size)return 0;
 if(VirtualQuery((const void *)addr,m,sizeof(m))!=sizeof(m))return 0;
 base=m[0];extent=m[3];protect=m[5];
 if(m[4]!=0x1000u||(protect&0x100u)||size>extent||
    addr<base||addr-base>extent-size)return 0;
 protect&=0xffu;
 return protect==0x02u||protect==0x04u||protect==0x08u||
        protect==0x20u||protect==0x40u||protect==0x80u;
}
static int bsp_read_native(void *ctx,unsigned int address,CoopBspNode *out){
 const U32 *node;
 U32 plane_addr;
 const float *plane;
 (void)ctx;
 if(!out||!bsp_mem_readable(address,4))return 0;
 node=(const U32 *)address;
 out->flags=node[0];
 out->child_neg=out->child_pos=0;
 out->normal=(CoopBspVec){0.f,0.f,0.f};out->distance=0.f;
 if(out->flags&3u)return 1;
 if(!bsp_mem_readable(address,0x34))return 0;
 plane_addr=node[0x08/4];
 if(!bsp_mem_readable(plane_addr,0x10))return 0;
 plane=(const float *)plane_addr;
 out->normal=(CoopBspVec){plane[0],plane[1],plane[2]};
 out->distance=plane[3];
 out->child_neg=node[0x2c/4];
 out->child_pos=node[0x30/4];
 return out->child_neg!=0u&&out->child_pos!=0u;
}
static CoopBspResult bsp_probe_base_world(KpcIntersectVec centre,
 KpcIntersectVec lo,KpcIntersectVec hi,U32 *nodes,U32 *root_out){
 U32 manager,shared,world,root;
 CoopBspBox candidate;
 const U32 global_ptr=(U32)backend+0x4a250u;
 if(root_out)*root_out=0;
 if(nodes)*nodes=0;
 if(!backend||!(bsp_volume_enabled||coop_spread_bsp)||
    !bsp_mem_readable(global_ptr,4))return COOP_BSP_UNKNOWN;
 manager=*(const U32 *)global_ptr;
 if(!bsp_mem_readable(manager+0x19cu,4))return COOP_BSP_UNKNOWN;
 shared=*(const U32 *)(manager+0x19cu);
 if(!bsp_mem_readable(shared+0x08u,4))return COOP_BSP_UNKNOWN;
 world=*(const U32 *)(shared+0x08u);
 if(!bsp_mem_readable(world+0x44u,4))return COOP_BSP_UNKNOWN;
 root=*(const U32 *)(world+0x44u);
 if(!bsp_mem_readable(root,4))return COOP_BSP_UNKNOWN;
 if(root_out)*root_out=root;
 candidate.min=(CoopBspVec){centre.x+lo.x,centre.y+lo.y,centre.z+lo.z};
 candidate.max=(CoopBspVec){centre.x+hi.x,centre.y+hi.y,centre.z+hi.z};
 return coop_bsp_box_classify(&candidate,root,bsp_read_native,0,nodes);
}
static void bsp_log_box(KpcIntersectVec centre,KpcIntersectVec lo,KpcIntersectVec hi){
 U32 nodes=0,root=0,n=0;
 char msg[116];
 CoopBspResult status=bsp_probe_base_world(centre,lo,hi,&nodes,&root);
 trace_append(msg,&n,"[KPCFix Co-op] base BSP box=");
 if(status==COOP_BSP_INTERIOR_ONLY)trace_append(msg,&n,"INTERIOR_ONLY");
 else if(status==COOP_BSP_TOUCHES_CLASS_ZERO)trace_append(msg,&n,"TOUCHES_CLASS_ZERO");
 else trace_append(msg,&n,"UNKNOWN");
 trace_append(msg,&n," nodes=");trace_hex(msg,&n,nodes);
 trace_append(msg,&n," root=");trace_hex(msg,&n,root);
 msg[n]=0;log_file(msg);
}


/* Named model RESOURCE registry; never identifies solid/live occupancy.
 * The exact original DLL hash is required before this path is enabled.
 * Native named-model lookup 0x10008f69 walks a sentinel list at +0xb0.
 * No native callback is invoked; all dereferences are VirtualQuery guarded.
 */
static int registry_read32(void *ctx,unsigned int address,unsigned int *out){
 (void)ctx;
 if(!out||!bsp_mem_readable(address,4))return 0;
 *out=*(const U32 *)address;return 1;
}
typedef struct {
 CoopAabb player;
 U32 disjoint,overlap,unknown;
} ModelOverlapSurvey;

static int read_spatial_box(U32 object,CoopModelRead32 read,void *ctx,CoopAabb *box){
 float coords[6];
 for(U32 i=0;i<6;i++){
  U32 bits;
  if(!coop_model_read_offset(read,ctx,object,0xacu+4*i,&bits))return 0;
  coords[i]=collision_float(bits);
 }
 box->min=(CoopVec3){coords[0],coords[1],coords[2]};
 box->max=(CoopVec3){coords[3],coords[4],coords[5]};
 return coop_aabb_valid(box);
}

/* Inspect the same object boxes passed by the native spatial visitor. */
static void observe_model_box(void *user,U32 entry,U32 model,U32 object,
 int matched,CoopModelRead32 read,void *ctx){
 ModelOverlapSurvey *survey=user;
 CoopAabb box;
 (void)entry;(void)model;
 if(!matched||!read_spatial_box(object,read,ctx,&box)){survey->unknown++;return;}
 switch(coop_aabb_overlap(&survey->player,&box,0.f)){
  case COOP_AABB_OVERLAP:survey->overlap++;break;
  case COOP_AABB_DISJOINT:survey->disjoint++;break;
  default:survey->unknown++;break;
 }
}

/* Original spawn preparation traverses selector zero at object.lto
 * 0xf1120 and 0xf1285. Use a private cursor: the original iterators
 * write manager+0xc and therefore cannot be nested in a diagnostic. */
static void survey_spawn_list(const CoopAabb *candidate,U32 player){
 CoopAabb box;
 U32 manager,node,seen=0,own=0,overlap=0,unknown=0,n=0;
 char line[116];
 CoopAabbResult self=read_spatial_box(player,registry_read32,0,&box)?
  coop_aabb_overlap(candidate,&box,0.f):COOP_AABB_UNKNOWN;
 log_file(self==COOP_AABB_OVERLAP?"[KPCFix Co-op] player control=OVERLAP":
          self==COOP_AABB_DISJOINT?"[KPCFix Co-op] player control=DISJOINT":
          "[KPCFix Co-op] player control=UNKNOWN");
 if(!coop_model_read_offset(registry_read32,0,(U32)spawn_image,0x157724,&manager)||
    !coop_model_read_offset(registry_read32,0,manager,4,&node))goto incomplete;
 while(node){
  U32 handle,next;
  if(++seen>1024||!registry_read32(0,node,&handle)||
     !coop_model_read_offset(registry_read32,0,node,4,&next))goto incomplete;
  if(handle==player)own++;
  else if(!read_spatial_box(handle,registry_read32,0,&box))unknown++;
  else if(coop_aabb_overlap(candidate,&box,0.f)==COOP_AABB_OVERLAP)overlap++;
  node=next;
 }
 trace_append(line,&n,"[KPCFix Co-op] spawn list count=");trace_hex(line,&n,seen);
 trace_append(line,&n," self=");trace_hex(line,&n,own);
 trace_append(line,&n," otherHits=");trace_hex(line,&n,overlap);
 trace_append(line,&n," unknown=");trace_hex(line,&n,unknown);
 line[n]=0;log_file(line);return;
incomplete:
 log_file("[KPCFix Co-op] spawn list=incomplete");
}
static void survey_candidate_boxes(U32 player,CoopVec3 center,CoopVec3 lo,CoopVec3 hi){
 ModelOverlapSurvey survey={0};
 CoopModelRegistrySnapshot snapshot={0};
 U32 manager,state,n=0;
 char line[116];
 if(!coop_aabb_from_relative(&center,&lo,&hi,&survey.player)||
    !registry_read32(0,(U32)backend+0x4a250u,&manager)||
    !coop_model_read_offset(registry_read32,0,manager,0x19cu,&state)||
    coop_model_registry_scan_visit(state,registry_read32,0,&snapshot,observe_model_box,&survey)!=COOP_MODEL_REGISTRY_TRAVERSED){
  log_file("[KPCFix Co-op] linked boxes=incomplete");return;
 }
 survey_spawn_list(&survey.player,player);
 trace_append(line,&n,"[KPCFix Co-op] linked boxes overlap=");trace_hex(line,&n,survey.overlap);
 trace_append(line,&n," disjoint=");trace_hex(line,&n,survey.disjoint);
 trace_append(line,&n," unknown=");trace_hex(line,&n,survey.unknown);
 line[n]=0;log_file(line);
}
static void registry_trace_loaded(void){
 const U32 global_ptr=(U32)backend+0x4a250u;
 U32 manager=0,state=0,n=0;
 CoopModelRegistrySnapshot snap={0};
 CoopModelRawState raw={0};
 CoopModelRegistryStatus verdict=COOP_MODEL_REGISTRY_UNKNOWN;
 char msg[116];
 if(model_registry_enabled&&registry_read32(0,global_ptr,&manager)&&
    registry_read32(0,manager+0x19cu,&state)){
  if(model_state_enabled)
   verdict=coop_model_state_scan(state,registry_read32,0,&snap,&raw);
  else verdict=coop_model_registry_scan(state,registry_read32,0,&snap);
 }
 trace_append(msg,&n,"[KPCFix Co-op] model registry=");
 trace_append(msg,&n,verdict==COOP_MODEL_REGISTRY_TRAVERSED?"TRAVERSED":"UNKNOWN");
 trace_append(msg,&n," entries=");trace_hex(msg,&n,snap.entries);
 trace_append(msg,&n," roots=");trace_hex(msg,&n,snap.root_candidates);
 trace_append(msg,&n," attached=");trace_hex(msg,&n,snap.associated_candidates);
 msg[n]=0;log_file(msg);
 n=0;
 trace_append(msg,&n,"[KPCFix Co-op] model refs matched=");
 trace_hex(msg,&n,snap.reciprocal_matches);
 trace_append(msg,&n," mismatch=");trace_hex(msg,&n,snap.reciprocal_mismatches);
 trace_append(msg,&n," unreadable=");trace_hex(msg,&n,snap.reciprocal_unreadable);
 msg[n]=0;log_file(msg);
 if(model_state_enabled){
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] model state=");
  trace_append(msg,&n,verdict==COOP_MODEL_REGISTRY_TRAVERSED?"TRAVERSED":"UNKNOWN");
  trace_append(msg,&n," matched=");trace_hex(msg,&n,raw.matched);
  trace_append(msg,&n," skipped=");trace_hex(msg,&n,raw.skipped_unverified);
  msg[n]=0;log_file(msg);
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] model types type2=");
  trace_hex(msg,&n,raw.type_2);
  trace_append(msg,&n," type1=");trace_hex(msg,&n,raw.type_1);
  trace_append(msg,&n," other=");trace_hex(msg,&n,raw.type_other);
  trace_append(msg,&n," unread=");trace_hex(msg,&n,raw.type_unreadable);
  msg[n]=0;log_file(msg);
  n=0;
  {
   unsigned i;
   for(i=0;i<16u;i++){
    if(!raw.type_hist[i])continue;
    n=0;
    trace_append(msg,&n,"[KPCFix Co-op] model type bucket=");
    trace_hex(msg,&n,i);
    trace_append(msg,&n," count=");trace_hex(msg,&n,raw.type_hist[i]);
    msg[n]=0;log_file(msg);
   }
   if(raw.type_outside_hist){
    n=0;
    trace_append(msg,&n,"[KPCFix Co-op] model type bucket=OUTSIDE_0_TO_15 count=");
    trace_hex(msg,&n,raw.type_outside_hist);
    msg[n]=0;log_file(msg);
   }
  }
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] type2 masks any12000=");
  trace_hex(msg,&n,raw.type2_flags_any12000);
  trace_append(msg,&n," none=");trace_hex(msg,&n,raw.type2_flags_none12000);
  trace_append(msg,&n," bit4000=");trace_hex(msg,&n,raw.type2_flags_4000);
  msg[n]=0;log_file(msg);
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] other masks any12000=");
  trace_hex(msg,&n,raw.other_flags_any12000);
  trace_append(msg,&n," none=");trace_hex(msg,&n,raw.other_flags_none12000);
  trace_append(msg,&n," bit4000=");trace_hex(msg,&n,raw.other_flags_4000);
  msg[n]=0;log_file(msg);
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] model flags 2000=");
  trace_hex(msg,&n,raw.flags_2000);
  trace_append(msg,&n," 10000=");trace_hex(msg,&n,raw.flags_10000);
  trace_append(msg,&n," any12000=");trace_hex(msg,&n,raw.flags_any_12000);
  trace_append(msg,&n," unread=");trace_hex(msg,&n,raw.flags_unreadable);
  msg[n]=0;log_file(msg);
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] model vectors equal=");
  trace_hex(msg,&n,raw.init_vector_equal);
  trace_append(msg,&n," changed=");trace_hex(msg,&n,raw.init_vector_different);
  trace_append(msg,&n," unread=");trace_hex(msg,&n,raw.init_vector_unreadable);
  msg[n]=0;log_file(msg);
  {
   unsigned i,j;
   for(i=0;i<raw.sample_count;i++){
    const CoopModelVectorSample *sample=&raw.samples[i];
    n=0;
    trace_append(msg,&n,"[KPCFix Co-op] model delta object=");
    trace_hex(msg,&n,sample->object);
    trace_append(msg,&n," record=");trace_hex(msg,&n,sample->resource);
    trace_append(msg,&n," type=");trace_hex(msg,&n,sample->type);
    trace_append(msg,&n," flags=");trace_hex(msg,&n,sample->flags);
    msg[n]=0;log_file(msg);
    for(j=0;j<2;j++){
     const U32 *v=j?sample->object_vector:sample->model_vector;
     n=0;
     trace_append(msg,&n,j?"[KPCFix Co-op] model vector current":"[KPCFix Co-op] model vector source");
     trace_append(msg,&n," x=");trace_hex(msg,&n,v[0]);
     trace_append(msg,&n," y=");trace_hex(msg,&n,v[1]);
     trace_append(msg,&n," z=");trace_hex(msg,&n,v[2]);
     msg[n]=0;log_file(msg);
    }
    n=0;
    trace_append(msg,&n,"[KPCFix Co-op] model adjacent valid=");
    trace_hex(msg,&n,sample->adjacent_valid);
    trace_append(msg,&n," a=");trace_hex(msg,&n,sample->adjacent_object_words[0]);
    trace_append(msg,&n," b=");trace_hex(msg,&n,sample->adjacent_object_words[1]);
    trace_append(msg,&n," c=");trace_hex(msg,&n,sample->adjacent_object_words[2]);
    trace_append(msg,&n," d=");trace_hex(msg,&n,sample->adjacent_object_words[3]);
    msg[n]=0;log_file(msg);
    for(j=0;j<2;j++){
     const U32 *v=&sample->resource_bounds_words[j*3];
     n=0;
     trace_append(msg,&n,j?"[KPCFix Co-op] model raw bounds high":"[KPCFix Co-op] model raw bounds low");
     trace_append(msg,&n," valid=");trace_hex(msg,&n,sample->bounds_valid);
     trace_append(msg,&n," x=");trace_hex(msg,&n,v[0]);
     trace_append(msg,&n," y=");trace_hex(msg,&n,v[1]);
     trace_append(msg,&n," z=");trace_hex(msg,&n,v[2]);
     msg[n]=0;log_file(msg);
    }
   }
  }
 }
 if(snap.first_root){
  n=0;
  trace_append(msg,&n,"[KPCFix Co-op] model registry first root=");
  trace_hex(msg,&n,snap.first_root);
  trace_append(msg,&n," object=");trace_hex(msg,&n,snap.first_associated);
  msg[n]=0;log_file(msg);
 }
}

/* Sample horizontal cross sections at three heights. An intersection is
 * evidence of obstruction. Misses leave unsampled space unknown. */
/* Ray samples provide positive obstruction evidence only. Full-hull and
 * active-actor clearance are UNKNOWN until their native ABI is verified. */
static void sample_candidate(U32 player,KpcIntersectFn intersect,KpcIntersectVec center,KpcIntersectVec lo,KpcIntersectVec hi){
 KpcIntersectVec floor_end=center;
 KpcIntersectInfo hit={0};
 CoopVolumeObservation o={0};
 CoopVec3 candidate={center.x,center.y,center.z};
 float levels[]={lo.y+1.f,(lo.y+hi.y)*0.5f,hi.y-1.f};
 unsigned level;
 int blocked=0;
 o.bounds_min=(CoopVec3){lo.x,lo.y,lo.z};
 o.bounds_max=(CoopVec3){hi.x,hi.y,hi.z};
 o.world_hull=COOP_VOLUME_UNKNOWN;
 o.other_actors=COOP_VOLUME_UNKNOWN;
 log_vector("candidate",center);
 if(model_registry_enabled&&model_state_enabled)survey_candidate_boxes(player,candidate,o.bounds_min,o.bounds_max);
 floor_end.y-=256.f;
 if(world_segment(intersect,center,floor_end,&hit)){
  o.ground_hit=1;
  o.ground_point=(CoopVec3){hit.point.x,hit.point.y,hit.point.z};
  o.ground_normal=(CoopVec3){hit.plane.normal.x,hit.plane.normal.y,hit.plane.normal.z};
  log_vector("floor",hit.point);
  log_vector("floor normal",hit.plane.normal);
 }else log_file("[KPCFix Co-op] floor missed");
 for(level=0;level<3;level++){
  KpcIntersectVec a=center,b=center;
  a.y=b.y=center.y+levels[level];
  a.x+=lo.x;b.x+=hi.x;
  blocked|=world_segment(intersect,a,b,&hit);
  blocked|=world_segment(intersect,b,a,&hit);
  a.x=b.x=center.x;
  a.z+=lo.z;b.z+=hi.z;
  blocked|=world_segment(intersect,a,b,&hit);
  blocked|=world_segment(intersect,b,a,&hit);
 }
 log_file(blocked?"[KPCFix Co-op] body sample=obstructed":"[KPCFix Co-op] body sample=no sampled hit");
 o.sampled_body_hit=blocked;
 if(bsp_volume_enabled)bsp_log_box(center,lo,hi);
 {CoopProbeResult verdict=coop_volume_assess(&candidate,&o);
  log_file(verdict==COOP_PROBE_BLOCKED?"[KPCFix Co-op] volume gate=BLOCKED":
    verdict==COOP_PROBE_SAFE?"[KPCFix Co-op] volume gate=SAFE":
    "[KPCFix Co-op] volume gate=UNSUPPORTED (full hull/actors unverified)");
 }
}
static void trace_clearance(U32 *engine,void *object){
 U32 *vtable=(U32 *)engine[0];
 KpcIntersectFn intersect=(KpcIntersectFn)engine[KPC_INTERSECT_SERVER_SLOT/4];
 KpcIntersectVec lo={0},hi={0},anchor={1600.f,-256.f,-2944.f};
 static const int directions[][2]={{1,0},{-1,0},{0,1},{0,-1}};
 unsigned i;
 if(!object||!vtable||!vtable[0x104/4]||!intersect){
  log_file("[KPCFix Co-op] bounds or segment API unavailable");return;
 }
 ((ReadBounds)vtable[0x104/4])(engine,object,&lo,&hi);
 log_vector("bounds min",lo);log_vector("bounds max",hi);
 if(model_registry_enabled)registry_trace_loaded();
 else log_file("[KPCFix Co-op] model registry=DISABLED (check effective KPCServerTools.ini; restart)");
 if(!(lo.x<0&&lo.y<-1&&lo.z<0&&hi.x>0&&hi.y>1&&hi.z>0&&
      lo.x>=-128&&lo.y>=-128&&lo.z>=-128&&hi.x<=128&&hi.y<=128&&hi.z<=128)){
  log_file("[KPCFix Co-op] bounds outside diagnostic range");return;
 }
 for(i=0;i<3;i++){
  KpcIntersectVec candidate=anchor;
  if(i==1)candidate.z+=96.f;
  if(i==2)candidate.z-=96.f;
  sample_candidate((U32)object,intersect,candidate,lo,hi);
 }
 for(i=0;i<4;i++){
  int dx=directions[i][0],dz=directions[i][1];
  KpcIntersectVec end=anchor;
  KpcIntersectInfo hit={0};
  float reach=dx>0?hi.x:dx<0?-lo.x:dz>0?hi.z:-lo.z;
  end.x+=512.f*dx;end.z+=512.f*dz;
  if(world_segment(intersect,anchor,end,&hit)&&
     hit.plane.normal.x*dx+hit.plane.normal.z*dz < -0.9f){
   KpcIntersectVec candidate=hit.point;
   log_vector("wall reference",hit.point);
   candidate.x-=0.5f*reach*dx;candidate.z-=0.5f*reach*dz;
   sample_candidate((U32)object,intersect,candidate,lo,hi);
   return;
  }
 }
 log_file("[KPCFix Co-op] wall control unavailable within 512 units");
}
/* The native game call pushes (HOBJECT, LTVector*) and cleans eight bytes
 * in the caller. Forward it unchanged and only then write an observation. */
static void CDECL trace_spawn_position(void *object,const U32 *pos){
 U32 *engine=spawn_image?*(U32 **)(spawn_image+SPAWN_POS_GLOBAL_RVA):0;
 if(!engine||!engine[0x1e0/4])fatal("[KPCFix Co-op] Missing original position-set API.");
 ((SpawnSetPos)engine[0x1e0/4])(object,pos);
 if(clearance_enabled&&coop_is_r1m2b_anchor(pos)&&
    __sync_bool_compare_and_swap(&clearance_started,0,1))trace_clearance(engine,object);
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
/* ------------------------------------------------------------------------
 * Experimental co-op session (CoopSession=1, protocol kpc-coop/1).
 * Every engine location below was verified in the original 1.13 object.lto
 * (SHA-256 3a03f879...) by static disassembly; see coop/COOP-MVP.md.
 *   0x16de60  CServerShell* global; +0x1d0 game type, +0x210 + i*0x7f maplist
 *             names, +0x1ae0 maplist count (fc640)
 *   0x16da88  maplist cursor (object 0x16da78 +0x10, advanced by fc640)
 *   0x100760  native multiplayer end-of-level (time/frag-limit path, fc626)
 *   0x1005c0  native NextLevel; called from 0x100744 (pending change) and
 *             0xfde97 (dedicated "nextlevel" console command)
 *   0x157724  object manager; [+4] selector-0 player list {HOBJECT,next}
 *   player+8 HOBJECT, +0x120 HCLIENT, +0x1e8 name[32]
 *   0xf10c0   telefrag loop over the player list; only side effect is damage
 * --------------------------------------------------------------------- */
#define COOP_SHELL_RVA 0x16de60u
#define COOP_MAPCURSOR_RVA 0x16da88u
#define COOP_PLAYERLIST_RVA 0x157724u
#define COOP_END_LEVEL_RVA 0x100760u
#define COOP_NEXT_LEVEL_RVA 0x1005c0u
#define COOP_LOG_BUDGET 3000u
extern void CDECL runtime_broadcast(void *,const char *);
static int coop_session_enabled;
static U32 coop_min_players=2,coop_max_players=3;
static U8 *coop_image;
static CoopSession coop_state;
static volatile U32 coop_log_count,campaign_log_count;
typedef void (__attribute__((thiscall)) *ShellFn)(void *);

static void coop_text(char *out,U32 *n,const char *s,U32 cap){while(s&&*s&&*n+1<cap)out[(*n)++]=*s++;out[*n]=0;}
static void coop_dec(char *out,U32 *n,U32 v,U32 cap){
 char t[11];U32 k=0;
 do{t[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10);
 while(k&&*n+1<cap)out[(*n)++]=t[--k];
 out[*n]=0;
}
/* Opt-in ([Coop] TraceTiming=1): prefix each session line with t=<ms since
 * the session was armed> so player actions (clicks, audio) can be correlated
 * with intermission, NextLevel and spawn events. Off by default: format unchanged. */
static int coop_trace_timing;
static U32 coop_t0;
/* [Coop] CoopIntermission=1 restores the native click-to-continue scoreboard;
 * default 0 changes world without MID_MULTI_INTERMISSION (see coop_execute). */
static int coop_intermission;
/* [Coop] TraceMusic=1: log the world's music event creation (call site 10494b). */
static int coop_trace_music;
/* [Coop] CoopMusic (default 1 in a session): per-player world music delivery. */
static int coop_music=1;
/* Snapshot of the world's music event (object.lto event class, vtable 11d240):
 * +0x108 song name, +0x188 "play" flag, +0x189 intensity, +0x18a groove,
 * +0x18b transition (0xff = unset), +0x18c fadeout. baa90 sends the song when
 * the name is non-empty: with the play flag as PLAY 0x4e/READY 0x4f chosen by
 * intensity (string 0x4b/0x4c), without it always READY 0x4f/0x4c (bac0f,
 * bad2b). Taken when WorldProperties creates it; epoch tags its world. */
static struct {int valid;U32 epoch;char song[0x80];U8 has_song,play,intensity,groove,transition,fadeout;} coop_music_snap;
/* [Coop] CoopIntro (default 0): play the world's original starting_cinematic
 * once for the session when all present members are in (see coop_intro_frame).
 * Snapshot of the script name from the WorldProperties lookup, per epoch. */
static int coop_intro;
static struct {int valid;U32 epoch;char name[0x100];} coop_intro_snap;
/* Proof of concept: only R1M3A's intro (cathedral1start.scr) is enabled. */
/* [Coop] CoopIntroAll (test only, default 0): allow any route world's intro. */
static int coop_intro_all;
static int coop_intro_world_allowed(U32 route_pos){
 return route_pos<coop_state.route_len&&(coop_intro_all||(coop_state.route[route_pos].flags&COOP_W_INTRO_OK));
}
/* [Coop] CoopScriptClock (default = CoopIntro): run the original script clock
 * on wall-clock time. Evidence (stock server.dll 83b7bce8, dedicated.exe,
 * object.lto 3a03f879):
 *  - dedicated.exe 401c7b-401c94 runs a server frame (403ad0 -> IServerMgr
 *    vtbl+0x50 = server.dll 2358c) only after timeGetTime advanced >= 8 ms;
 *  - 2358c takes time = timeGetTime*0.001 (2ed76); 2a05f sets the frame's
 *    elapsed time and 2a19c-2a1d0 clamps it to [0.01, 0.2] s, so an 8 ms
 *    frame is credited 10 ms (game time runs 1.25x wall time on such hosts);
 *  - the object.lto shell Update ffb70 caps that dt at 0.2 and passes it to
 *    the script updater c5630 (call at ffba5), which advances every playing
 *    script (c6260: time += dt; dispatch events whose time <= script time)
 *    and its running events (camera_path update b3230, fog fade c2b60).
 * The client plays streamed narration in real time, so script-timed
 * subtitles, camera cuts and the window smash ran ahead of the voice. The
 * governor (coop_script_clock.h) gives scripts min(engine dt, unpaid wall
 * time). Only the c5630 argument changes; ea6d0 and the engine's own game
 * time are untouched. TraceTiming=1 installs the same site as a read-only
 * observer (dt forwarded bit-identically) when the governor is off. */
static int coop_script_clock;
static CoopScriptClock coop_clock;
/* [Coop] CoopSoundClock (default = CoopScriptClock): run the server's sound
 * timers on wall-clock time. Evidence (stock server.dll 83b7bce8):
 *  - object.lto streamsound (event bcf2d, local mode 52c80) calls PlaySound
 *    (g_pServerDE vtbl+0x174) with flags 0x430;
 *  - server.dll 2c6e6-2c70a sets a track's time left (+0x20) to
 *    duration + min(0.5, duration/2) (44.19 s narration: 44.69 s);
 *  - the per-frame updater 29f0b (only caller: 2a1fc, ECX = server manager,
 *    one float = the clamped frame dt [esi+0x1f0], ret 4) subtracts that dt
 *    per track (2c805);
 *  - the per-client set builder 1f920 skips a track whose time left <= 0
 *    (1f94f-1f961), so clients stop it; IsSoundDone (277e2) reports done.
 * With frames credited 1.25x wall time, the 44.69 s narration left the set
 * about 35.9 s of real time after it started: the cut after "...useful ally
 * for you" (WAV 35-36.5 s). The client streams in real time. The governor
 * (the same arithmetic as the script clock, separate state) gives 29f0b
 * min(engine dt, unpaid wall time); nothing else in the frame changes. The
 * site is hash-gated to the Europe-family server.dll; others stay original. */
#define SOUND_TRACKS_CALL_RVA 0x2a1fcu
#define SOUND_TRACKS_TARGET_RVA 0x29f0bu
static const U8 original_sound_call[5]={0xe8,0x0a,0xfd,0xff,0xff};
static int coop_sound_clock,coop_sound_clock_installed;
static CoopScriptClock coop_sound;
static struct {U32 frames,engine_us,given_us;} coop_sound_sum;
/* Bounded clock observation (TraceTiming=1): one calibration line for the
 * first 10 s of frames, and during an intro one line per dispatched script
 * event plus a summary. Totals are microseconds; frames are counted. */
static struct {U32 frames,engine_us,given_us;} coop_clock_sum;
static struct {int done,started;U32 t0,frames0,engine0,given0;} coop_clock_cal;
#define COOP_INTRO_EVENT_LOG_MAX 128u
static struct {int active;U32 script,next,logged,t0,frames0,engine0,given0,sframes0,sengine0,sgiven0;} coop_intro_trace;
/* [Coop] CoopStartScoreboard (default 1 in a session with CoopIntermission=0):
 * the stock scoreboard as the !start loading presentation (coop_scoreboard.h).
 * Messages: 0x48 MID_MULTI_INTERMISSION (show) and 0x69 MID_CAN_RENDER
 * (close), both without payload, to one client at a time. */
static int coop_start_scoreboard;
static CoopScoreboard coop_sb;
/* [Coop] CoopSpawners (default 1 in a session): CMonsterSpawner::ReadProp
 * (object.lto 93a80) ORs 0x10 ("not in multiplayer") into the spawner's
 * MiscFlags before the shared skill/game-type filter f9000 (93c5d: or al,0x10;
 * call at 93c65). f9000 returns 0 in multiplayer whenever bit 0x10 is set, so
 * every CMonsterSpawner is marked not-to-create (93c71-93c7b: or [ocs+4],2)
 * at world load. R1M3A has 53 of them in 17 groups, including the three named
 * spawn6 that 47.9 trigger_generic spawn6 activates (CMonsterSpawner7 10x
 * CHeadless Normal+Hard, CMonsterSpawner41 5x CHeadless Easy,
 * CMonsterSpawner35 3x CGasbag Normal+Hard). trigger_generic (bf520 ->
 * 300a0) then finds no group and does nothing. Placed monsters and CSpawner
 * use the authored flags (d4b44), which is why R1M2B has enemies. NOP-ing the
 * forced OR passes the authored flags: designer 0x10 and the difficulty
 * bits are still honoured. The spawners exist once on the server and their
 * monsters replicate like any placed monster. */
static int coop_spawners=1,coop_trace_spawners;
static U32 coop_spawner_ok,coop_spawner_no,coop_spawner_logged;
static int coop_sp_start=1,coop_trace_scripts,coop_script_events=1,coop_friendly_fire,coop_shared_items=1,coop_spread;
static int coop_host_sequences=1;   /* [Coop] CoopHostSequences (default 1) */
/* 2026-11-12 playtest follow-ups (each a [Coop] key, session only):
 *   CoopSpawnerNearest=1   spawner activation radius measured from the nearest player
 *   CoopCheatKeys=1        stock cheat keybinds (message 0x3b) per player under kpc_cheats
 *   CoopKeepInventory=1    weapons/armour/shards/ammo/class kept through world changes and lobby resets
 *   CoopKeepOnDeath=1      ... and through a death in the same world
 *   CoopItemRespawnSec=15  taken weapons, ammo, shards and other pickups come back (0 = off)
 *   CoopPowerupResend=1    the armour "player powerup" effect is re-sent to the observers */
static int coop_spawner_nearest=1,coop_cheat_keys=1,coop_keep_inventory=1,coop_keep_on_death=1,coop_powerup_resend=1;
static U32 coop_item_respawn_sec=15;
static U32 coop_powerup_obj,coop_powerup_due;
static void coop_inv_spawned(int slot,U32 client,int respawn,U32 now);
static void coop_inv_world_unload(void);
static int coop_more_command(U32 player,const char *cmd,const char *who);
/* [Coop] CoopSkill (absent = unchanged; 0 Lullaby, 1 Daydream, 2 Sleepless,
 * 3 Cold Sweat, 4 Nightmare as printed by the stock skill command fb710).
 * f9000 maps skill 0-1 to MiscFlags bit 1, 2 to bit 2, 3-4 to bit 4. The
 * dedicated server never sets it (fb738, f1f66), so it stays 0. Written to
 * the original variable 15b678 before each world load. */
static I32 coop_skill=-1;
/* [Coop] CoopTestStartWorld: route world that !start loads (test option). */
static const char *coop_start_label;
static int coop_start_state;   /* 0 unset, 1 accepted, 2 rejected */
/* [Coop] CoopCampaign (default 0): campaign graph from WORLDORDER with host
 * selection (coop_campaign.h). [Coop] CoopLobbyWorld: world for the lobby
 * after completion or !abort (empty = first world of the selection). */
static int coop_campaign;
static U32 coop_intro_settle_ms=COOP_INTRO_SETTLE_MS;
static char coop_lobby_world[COOP_WORLD_MAX];
/* Bounded, sequenced log: "[KPCFix Co-op] g=1 e=2 seq=7 PLAYING event detail" */
static void coop_log(const char *event,const char *detail){
 char line[240];U32 n=0;
 if(__sync_fetch_and_add(&coop_log_count,1)>=COOP_LOG_BUDGET)return;
 coop_text(line,&n,"[KPCFix Co-op] ",sizeof line);
 if(coop_trace_timing){coop_text(line,&n,"t=",sizeof line);coop_dec(line,&n,GetTickCount()-coop_t0,sizeof line);coop_text(line,&n," ",sizeof line);}
 coop_text(line,&n,"g=",sizeof line);coop_dec(line,&n,coop_state.generation,sizeof line);
 coop_text(line,&n," e=",sizeof line);coop_dec(line,&n,coop_state.epoch,sizeof line);
 coop_text(line,&n," seq=",sizeof line);coop_dec(line,&n,coop_state.seq,sizeof line);
 coop_text(line,&n," ",sizeof line);coop_text(line,&n,coop_phase_name(coop_state.phase),sizeof line);
 coop_text(line,&n," ",sizeof line);coop_text(line,&n,event,sizeof line);
 if(detail){coop_text(line,&n," ",sizeof line);coop_text(line,&n,detail,sizeof line);}
 log_file(line);
}
static int coop_rd(U32 address,U32 *value){return registry_read32(0,address,value);}
/* Copy a C string through guarded reads (page checks), always terminated. */
static U32 coop_copy_str(char *out,U32 src,U32 cap){
 U32 i;
 if(!cap)return 0;
 for(i=0;i+1<cap;i++){
  if((!i||((src+i)&0xfffu)==0)&&!bsp_mem_readable(src+i,1))break;
  out[i]=*(const char *)(src+i);
  if(!out[i])return i;
 }
 out[i]=0;return i;
}
static U32 coop_live_objects(U32 *live,U32 cap){
 U32 manager,node,count=0,guard=0;
 if(!coop_rd((U32)coop_image+COOP_PLAYERLIST_RVA,&manager)||!coop_rd(manager+4,&node))return 0;
 while(node&&guard++<256){
  U32 handle,next;
  if(!coop_rd(node,&handle)||!coop_rd(node+4,&next))break;
  if(count<cap)live[count++]=handle;
  node=next;
 }
 return count;
}
static void coop_presence(U32 now){
 U32 live[64],n=coop_live_objects(live,64);CoopResult r;
 if(coop_session_presence(&coop_state,live,n,now,&r)){coop_log("presence",r.msg);if(r.action)runtime_broadcast(coop_image,r.msg);}
}
/* Select route[target] as the next native maplist entry. Fails closed if the
 * server's maplist does not contain the route world. */
/* The native maplist holds at most 50 names (fdd3f: 0x32 entries of 0x7f
 * bytes at shell+0x210; count shell+0x1ae0), too few for the 48-world
 * campaign plus the map "start" prepends. In campaign mode a target world
 * missing from the maplist is written (memory only; the file is untouched)
 * into the slot NextLevel takes next; the MVP route still requires it. */
static int coop_prepare_maplist(U32 target){
 U32 shell,count,i,cursor_value,slot,mbi[7];char name[0x80];
 const char *world=coop_target_world(&coop_state,target);U32 *cursor=(U32 *)(coop_image+COOP_MAPCURSOR_RVA);
 if(!world[0]||!coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)||
    !coop_rd(shell+0x1ae0u,&count)||!count||count>50)return 0;
 if(VirtualQuery(cursor,mbi,sizeof(mbi))!=sizeof(mbi)||mbi[4]!=0x1000||!(mbi[5]&0xccu)||(mbi[5]&0x100))return 0;
 for(i=0;i<count;i++){
  coop_copy_str(name,shell+0x210u+i*0x7fu,0x7f);
  if(coop_world_equal(name,world)){*cursor=i?i-1:count-1;return 1;}   /* fc640 pre-increments and wraps */
 }
 if(!coop_campaign||!coop_rd((U32)coop_image+COOP_MAPCURSOR_RVA,&cursor_value))return 0;
 slot=cursor_value+1u<count?cursor_value+1u:0u;
 if(!bsp_mem_readable(shell+0x210u+slot*0x7fu,0x7f))return 0;
 {
  char *dst=(char *)(shell+0x210u+slot*0x7fu);U32 n=0,k;const char *pre="worlds\\";
  for(k=0;pre[k]&&n<0x7e;k++)dst[n++]=pre[k];
  for(k=0;world[k]&&n<0x7e;k++)dst[n++]=world[k]=='/'?'\\':world[k];
  dst[n]=0;
  {char line[160];U32 m=0;coop_text(line,&m,"slot=",sizeof line);coop_dec(line,&m,slot,sizeof line);
   coop_text(line,&m," world=",sizeof line);coop_text(line,&m,dst,sizeof line);coop_log("maplist",line);}
 }
 return 1;
}
static void coop_current_world(char out[0x80]){
 U32 shell,count,cursor;out[0]=0;
 if(coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)&&coop_rd(shell+0x1ae0u,&count)&&
    coop_rd((U32)coop_image+COOP_MAPCURSOR_RVA,&cursor)&&count&&count<=64&&cursor<count)
  coop_copy_str(out,shell+0x210u+cursor*0x7fu,0x7f);
}
/* The co-op lobby is a deathmatch world (the launcher picks one at random,
 * CoopLobbyWorld). There every co-op spawn rule steps aside: players use the
 * map's own multiplayer starts (stock strategy f1070), no single-player start,
 * no waiting placement and no spawn spread. */
static int coop_world_is_deathmatch(void){
 char w[0x80],n[COOP_WORLD_MAX];U32 i;
 coop_current_world(w);
 if(!w[0]||!coop_world_normalize(w,n))return 0;
 for(i=0;n[i];i++)if(n[i]=='d'&&n[i+1]=='e'&&n[i+2]=='a'&&n[i+3]=='t'&&n[i+4]=='h'&&n[i+5]=='m'&&n[i+6]=='a'&&n[i+7]=='t'&&n[i+8]=='c'&&n[i+9]=='h')return 1;
 return 0;
}
/* Default co-op path: set the shell's native pending-NextLevel flag +0x1ae9,
 * which the server's own Update (ffb70 -> 100720) consumes on the next frame
 * through the hooked NextLevel site 100744. This is the native post-
 * intermission path without the scoreboard broadcast or the click wait,
 * the same client flow as a console "nextlevel". Refused if a native
 * intermission is already active (+0x1ae8), so no state is left half set. */
static int coop_shell_change_allowed(U32 shell){
 if(!bsp_mem_readable(shell+0x1ae8u,2)||*(const U8 *)(shell+0x1ae8u)){
  CoopResult failed;coop_session_change_failed(&coop_state,&failed);
  coop_log("change-failed","native intermission already active or shell flags unreadable");
  runtime_broadcast(coop_image,failed.msg);return 0;
 }
 return 1;
}
static void coop_set_pending_next_level(U32 shell){
 if(!coop_shell_change_allowed(shell))return;
 *(U8 *)(shell+0x1ae9u)=1;
 coop_log("end-level","native pending NextLevel set (no intermission scoreboard)");
}
static void coop_sb_deliver(const CoopSbMessage *out,U32 n);
static void coop_execute(CoopResult *r){
 if(r->action==COOP_ACT_NONE)return;
 coop_log(r->action==COOP_ACT_CHANGE_WORLD?"change":"say",r->msg);
 runtime_broadcast(coop_image,r->msg);
 if(r->action==COOP_ACT_CHANGE_WORLD){
  U32 shell;
  if(!coop_prepare_maplist(r->target_pos)||!coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)||!shell){
   CoopResult failed;coop_session_change_failed(&coop_state,&failed);
   coop_log("change-failed","maplist lacks route world or shell unreadable");
   runtime_broadcast(coop_image,failed.msg);return;
  }
  if(coop_intermission){
   /* Native multiplayer end-of-level 100760: broadcasts MID_MULTI_INTERMISSION
    * (0x48, scoreboard) and sets +0x1ae8. The change then waits until >= 3.0 s
    * have passed AND a player presses fire/use: 1007c0 is called only from the
    * player method ef960 when its input flags +0x134 & 0x2800 are set. */
   ((ShellFn)(coop_image+COOP_END_LEVEL_RVA))((void *)shell);
   coop_log("end-level","native intermission requested (continues on a player's fire/use)");
  }else{
   if(coop_start_scoreboard&&coop_state.start_pending){
    /* !start: show the stock scoreboard to every present member, then make
     * the same pending-NextLevel change after COOP_SB_HOLD_MS (coop_frame). */
    CoopSbMessage out[COOP_SESSION_MAX_MEMBERS];
    if(!coop_shell_change_allowed(shell))return;
    coop_sb_deliver(out,coop_sb_start(&coop_sb,&coop_state,GetTickCount(),out));
    coop_log("end-level","start scoreboard shown; native pending NextLevel deferred 1000 ms");
    return;
   }
   coop_set_pending_next_level(shell);
  }
 }
}
static void coop_tick(U32 now){CoopResult r;if(coop_session_tick(&coop_state,now,&r))coop_execute(&r);}
/* Engine message API on g_pServerDE (object.lto global 16de80), as used by the
 * original music code bad50/baa90: thiscall vtable slots StartMessage +0xac
 * (hClient, id) -> HMESSAGEWRITE, WriteToMessageByte +0x8 (msg, byte),
 * WriteToMessageString +0x14 (msg, str), EndMessage +0xb4 (msg). */
typedef U32 (__attribute__((thiscall)) *MsgStartFn)(void *,U32,U32);
typedef void (__attribute__((thiscall)) *MsgByteFn)(void *,U32,U32);
typedef void (__attribute__((thiscall)) *MsgStrFn)(void *,U32,const char *);
typedef void (__attribute__((thiscall)) *MsgEndFn)(void *,U32);
static int coop_msg_api(void **de,U32 **vt){
 U32 p,v;
 if(!coop_rd((U32)coop_image+0x16de80u,&p)||!p||!coop_rd(p,&v)||!v||!bsp_mem_readable(v,0xb8))return 0;
 *de=(void *)p;*vt=(U32 *)v;return 1;
}
/* Send one client exactly the sequence the original event method baa90 would
 * broadcast, preceded by the load from bad50, using the string forms the
 * client resolves through the same routines: LOAD_STRING 0x4a, [GROOVE 0x52],
 * [TRANSITION 0x51], PLAY_STRING 0x4b (play flag, no intensity) or READY_STRING 0x4c,
 * [INTENSITY 0x50], [FADEOUT 0x53]. */
static int coop_music_send(U32 client){
 void *de;U32 *vt,msg;
 if(!client||!coop_music_snap.valid||!coop_music_snap.has_song||!coop_msg_api(&de,&vt))return 0;
#define COOP_MSG_BEGIN(id) msg=((MsgStartFn)vt[0xac/4])(de,client,(id));if(!msg)return 0
#define COOP_MSG_END() ((MsgEndFn)vt[0xb4/4])(de,msg)
 COOP_MSG_BEGIN(0x4au);((MsgStrFn)vt[0x14/4])(de,msg,coop_music_snap.song);COOP_MSG_END();
 if(coop_music_snap.groove!=0xffu){COOP_MSG_BEGIN(0x52u);((MsgByteFn)vt[0x8/4])(de,msg,coop_music_snap.groove);COOP_MSG_END();}
 if(coop_music_snap.transition!=0xffu){COOP_MSG_BEGIN(0x51u);((MsgByteFn)vt[0x8/4])(de,msg,coop_music_snap.transition);COOP_MSG_END();}
 COOP_MSG_BEGIN(coop_music_snap.play&&coop_music_snap.intensity==0xffu?0x4bu:0x4cu);((MsgStrFn)vt[0x14/4])(de,msg,coop_music_snap.song);COOP_MSG_END();
 if(coop_music_snap.intensity!=0xffu){COOP_MSG_BEGIN(0x50u);((MsgByteFn)vt[0x8/4])(de,msg,coop_music_snap.intensity);COOP_MSG_END();}
 if(coop_music_snap.fadeout){COOP_MSG_BEGIN(0x53u);COOP_MSG_END();}
#undef COOP_MSG_BEGIN
#undef COOP_MSG_END
 return 1;
}
/* Stock scoreboard show (0x48) / close (0x69) to one client, no payload. */
static int coop_sb_send(U32 client,U32 id){
 void *de;U32 *vt,msg;
 if(!client||!coop_msg_api(&de,&vt))return 0;
 msg=((MsgStartFn)vt[0xac/4])(de,client,id);if(!msg)return 0;
 ((MsgEndFn)vt[0xb4/4])(de,msg);
 return 1;
}
static void coop_sb_deliver(const CoopSbMessage *out,U32 n){
 U32 i,k;
 for(i=0;i<n;i++){
  char line[160];int ok=coop_sb_send(out[i].client,out[i].id);k=0;
  coop_text(line,&k,out[i].id==COOP_SB_SHOW?"show client=":"close client=",sizeof line);
  coop_dec(line,&k,out[i].client,sizeof line);
  coop_text(line,&k,out[i].id==COOP_SB_SHOW?" msg=0x48":" msg=0x69",sizeof line);
  coop_text(line,&k,ok?" sent":" failed",sizeof line);
  coop_log("scoreboard",line);
 }
}
static void coop_sb_pump(U32 now){
 CoopSbMessage out[COOP_SESSION_MAX_MEMBERS];
 if(coop_start_scoreboard)coop_sb_deliver(out,coop_sb_frame(&coop_sb,&coop_state,now,out));
}
/* Deliver due per-player music starts (called once per server frame). */
static void coop_music_frame(U32 now){
 int i,n=0;
 while(n++<4&&(i=coop_session_music_take(&coop_state,now,coop_music_snap.valid&&coop_music_snap.epoch==coop_state.epoch))>=0){
  char line[200];U32 k=0;CoopMember *m=&coop_state.m[i];int ok=coop_music_send(m->client);
  coop_text(line,&k,"client=",sizeof line);coop_dec(line,&k,m->client,sizeof line);
  coop_text(line,&k," name=",sizeof line);coop_text(line,&k,m->name,sizeof line);
  coop_text(line,&k," song=",sizeof line);coop_text(line,&k,coop_music_snap.song,sizeof line);
  coop_text(line,&k," intensity=",sizeof line);coop_dec(line,&k,coop_music_snap.intensity,sizeof line);
  coop_text(line,&k," transition=",sizeof line);coop_dec(line,&k,coop_music_snap.transition,sizeof line);
  coop_text(line,&k,ok?" sent":" failed",sizeof line);
  coop_log("music-send",line);
 }
}
static int coop_identify(U32 player,U32 *client,U32 *object,char name[COOP_NAME_MAX]){
 name[0]=0;
 if(!player||!coop_rd(player+0x120u,client)||!coop_rd(player+8u,object))return 0;
 coop_copy_str(name,player+0x1e8u,COOP_NAME_MAX);
 return *client!=0;
}
/* ---- CoopSpawnSpread (session, default 1): collision-checked spawn spread ----
 * A player whose start point is already taken by another player this world
 * is moved to the first candidate of the allocator's grid around it
 * (coop_allocator.h: 96-unit steps, 64-unit separation) that passes all of:
 *   1. line of sight: a world ray (IntersectSegment, g_pServerDE+0x90, the
 *      R1M2B-verified ABI) from the start to the candidate hits nothing, so a
 *      player is never put into another room;
 *   2. floor: a downward ray within 256 units hits a surface with normal.y >=
 *      0.7 at the start's floor height +-16 (the candidate keeps the start's
 *      height above its floor);
 *   3. body: the player box (-20,-46,-20)..(20,46,20) (original bounds, see
 *      GENERAL-ALLOCATOR.md) lies only in interior leaves of the world BSP
 *      (TraceBspVolume classifier, R1M2B live-validated; original server.dll
 *      83b7bce8 only).
 * Unknown results fail closed; with no passing candidate the player shares
 * the start (no telefrag, as before). Movable world models and live actors are
 * not checked. The first player at a start is never moved. Reservations
 * reset with every world. R1M2B keeps its three play-tested positions. */
static CoopAllocator coop_spread_alloc;
static U32 coop_spread_epoch=0xffffffffu;
typedef struct {CoopVec3 anchor;float anchor_floor;KpcIntersectFn intersect;} CoopSpreadCtx;
static int coop_floor_below(KpcIntersectFn intersect,const CoopVec3 *p,float *floor_y){
 KpcIntersectInfo hit={0};
 KpcIntersectVec from={p->x,p->y,p->z},to={p->x,p->y-256.0f,p->z};
 if(!world_segment(intersect,from,to,&hit))return 0;
 if(hit.plane.normal.y<0.7f)return 0;
 *floor_y=hit.point.y;return 1;
}
static CoopProbeResult coop_spread_probe(void *ctx,const CoopVec3 *c,CoopVec3 *out){
 CoopSpreadCtx *x=(CoopSpreadCtx *)ctx;KpcIntersectInfo hit={0};float floor_y,d;
 KpcIntersectVec from={x->anchor.x,x->anchor.y,x->anchor.z},to={c->x,c->y,c->z};
 KpcIntersectVec centre,lo={-20.0f,-46.0f,-20.0f},hi={20.0f,46.0f,20.0f};
 CoopBspResult body;
 if(world_segment(x->intersect,from,to,&hit))return COOP_PROBE_BLOCKED;
 if(!coop_floor_below(x->intersect,c,&floor_y))return COOP_PROBE_BLOCKED;
 d=floor_y-x->anchor_floor;if(d<-16.0f||d>16.0f)return COOP_PROBE_BLOCKED;
 centre.x=c->x;centre.y=floor_y+(x->anchor.y-x->anchor_floor);centre.z=c->z;
 body=bsp_probe_base_world(centre,lo,hi,0,0);
 if(body==COOP_BSP_UNKNOWN)return COOP_PROBE_UNSUPPORTED;
 if(body!=COOP_BSP_INTERIOR_ONLY)return COOP_PROBE_BLOCKED;
 out->x=centre.x;out->y=centre.y;out->z=centre.z;
 return COOP_PROBE_SAFE;
}
static void coop_spawn_spread(U32 player,U32 *pos){
 CoopVec3 anchor,placed;CoopSpreadCtx ctx;U32 *engine,i,index=0;int taken=0,slot=-1;
 char line[160];U32 n=0;CoopAllocationStatus st;
 if(!coop_spread||!pos||!player)return;
 if(coop_spread_epoch!=coop_state.epoch){coop_spread_epoch=coop_state.epoch;coop_allocator_reset(&coop_spread_alloc,coop_state.epoch);}
 anchor.x=collision_float(pos[0]);anchor.y=collision_float(pos[1]);anchor.z=collision_float(pos[2]);
 /* Free start: reserve it unchanged. */
 for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++){
  const CoopAllocation *a=&coop_spread_alloc.slots[i];
  if(a->owner==player){slot=(int)i;continue;}
  if(a->owner){float dx=a->position.x-anchor.x,dz=a->position.z-anchor.z;if(dx*dx+dz*dz<COOP_ALLOCATOR_MIN_GAP*COOP_ALLOCATOR_MIN_GAP)taken=1;}
 }
 if(!taken){
  if(slot<0)for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++)if(!coop_spread_alloc.slots[i].owner){slot=(int)i;break;}
  if(slot>=0){coop_spread_alloc.slots[slot].owner=player;coop_spread_alloc.slots[slot].position=anchor;}
  return;
 }
 engine=spawn_image?*(U32 **)(spawn_image+SPAWN_POS_GLOBAL_RVA):0;
 if(!engine||!engine[KPC_INTERSECT_SERVER_SLOT/4]){coop_log("spawn-spread","engine query unavailable; start shared");return;}
 ctx.anchor=anchor;ctx.intersect=(KpcIntersectFn)engine[KPC_INTERSECT_SERVER_SLOT/4];
 if(!coop_floor_below(ctx.intersect,&anchor,&ctx.anchor_floor)){coop_log("spawn-spread","no floor under the start; start shared");return;}
 st=coop_allocator_assign(&coop_spread_alloc,player,&anchor,coop_spread_probe,&ctx,&placed,&index);
 if(st==COOP_ALLOC_SUCCESS&&index>0u){
  pos[0]=collision_bits(placed.x);pos[1]=collision_bits(placed.y);pos[2]=collision_bits(placed.z);
  coop_text(line,&n,"moved to candidate ",sizeof line);coop_dec(line,&n,index,sizeof line);
  coop_text(line,&n," (clear line, floor and body)",sizeof line);
 }else coop_text(line,&n,"no candidate passed; start shared",sizeof line);
 coop_log("spawn-spread",line);
}
/* Both original spawn paths: register/refresh the member, place on R1M2B,
 * and SKIP the native telefrag loop (co-op players are teammates). */
static void coop_spawn_hook(U32 player,U32 *pos,int first_path){
 U32 client=0,object=0,now=GetTickCount();char name[COOP_NAME_MAX],line[160];U32 n=0;
 int slot=-1;CoopResult r;
 coop_tick(now);coop_presence(now);
 if(coop_identify(player,&client,&object,name)){
  int before=coop_find_client(&coop_state,client);
  int respawn=before>=0&&coop_state.m[before].spawned&&coop_state.m[before].seen_epoch==coop_state.epoch;
  slot=coop_session_seen(&coop_state,client,player,object,name,now,&r);
  coop_execute(&r);
  coop_inv_spawned(slot,client,respawn,now);
  if(coop_music)coop_session_music_schedule(&coop_state,slot,now);
  if(coop_start_scoreboard)coop_sb_spawn(&coop_sb,&coop_state,slot,now);
 }
 if(coop_world_is_deathmatch()){}   /* lobby: the map's own multiplayer start */
 else if(first_path&&slot>=0&&slot<(int)COOP_TRIAL_SLOTS&&coop_is_r1m2b_anchor(pos))
  coop_assign_trial_position(pos,slot);
 else if(slot>=0)coop_spawn_spread(player,pos);
 coop_text(line,&n,first_path?"path=f13a4 slot=":"path=f167a slot=",sizeof line);
 if(slot<0)coop_text(line,&n,"none",sizeof line);else coop_dec(line,&n,(U32)slot,sizeof line);
 coop_text(line,&n," client=",sizeof line);coop_dec(line,&n,client,sizeof line);
 coop_text(line,&n," name=",sizeof line);coop_text(line,&n,name,sizeof line);
 if(pos){coop_text(line,&n," zbits=",sizeof line);coop_dec(line,&n,pos[2],sizeof line);}
 coop_text(line,&n," telefrag=skipped",sizeof line);
 coop_log("spawn",line);
}
/* Per-player cheats (chat; need kpc_cheats 1 / AllowCheats=1, the same
 * permission as the stock invulnerability and noclip cheats: logging.S STATE
 * 0x174f00 = 1 with the console command registered, +4 = 1):
 *   !gimmie      COOP_GIMMIE_AMMO of each ammo type and the next weapon the
 *                player lacks (BeastClaws .. Stargaze, ids 3-14);
 *   !nextarmor   the next armour piece the player lacks (Gauntlets .. Mask,
 *                0x27-0x2c; ammo needs the gauntlets, f3530).
 * Each use gives only to the player who typed it (f3440, has-item f25f0) and
 * is announced in chat. More chat commands (!kill, !all, !class) and the
 * stock cheat keybinds are in coop_more_command / coop_cheat_msg below. */
#define COOP_GIMMIE_AMMO 50u
typedef U32 (__attribute__((thiscall)) *HasItemFn)(void *,U32);
typedef U32 (__attribute__((thiscall)) *CheatGiveFn)(void *,U32,U32,U32);
static int coop_cheats_allowed(void){
 return *(const U32 *)(coop_image+0x174f04u)==1u&&*(const U32 *)(coop_image+0x174f00u)==1u;
}
static int coop_cheat_word(const char *t,const char *w){
 U32 i;
 for(i=0;w[i];i++)if(coop_lower(t[i])!=w[i])return 0;
 return t[i]==0||t[i]==' '||t[i]=='\t';
}
static U32 coop_item_name(U32 id,char *out,U32 cap){
 U32 np=0;out[0]=0;
 if(coop_rd((U32)coop_image+0x141e40u+id*0x20u,&np)&&np)coop_copy_str(out,np,cap);
 return out[0]!=0;
}
static int coop_cheat_command(U32 player,const char *cmd,const char *who){
 int gimmie=coop_cheat_word(cmd,"gimmie")||coop_cheat_word(cmd,"gimme");
 int armour=coop_cheat_word(cmd,"nextarmor")||coop_cheat_word(cmd,"nextarmour");
 CoopResult r;U32 m=0,id,got=0;char item[24];
 if(coop_more_command(player,cmd,who))return 1;   /* !kill !all !class */
 if(!gimmie&&!armour)return 0;
 coop_result(&r,COOP_ACT_ANNOUNCE,"");
 if(!coop_cheats_allowed()){
  coop_text(r.msg,&m,"[Co-op] Cheats are disabled on this server (host console: kpc_cheats 1)",sizeof r.msg);
  coop_execute(&r);return 1;
 }
 coop_text(r.msg,&m,"[Co-op] ",sizeof r.msg);coop_text(r.msg,&m,who,sizeof r.msg);
 if(gimmie){
  for(id=0x11u;id<=0x14u;id++)((CheatGiveFn)(coop_image+0xf3440u))((void *)player,id,COOP_GIMMIE_AMMO,1);
  for(id=3u;id<=14u;id++)if(!(((HasItemFn)(coop_image+0xf25f0u))((void *)player,id)&0xffu)){got=id;break;}
  if(got)((CheatGiveFn)(coop_image+0xf3440u))((void *)player,got,1,1);
  coop_text(r.msg,&m," used gimmie gimmie gimmie: ammo",sizeof r.msg);
  if(got&&coop_item_name(got,item,sizeof item)){coop_text(r.msg,&m," + ",sizeof r.msg);coop_text(r.msg,&m,item,sizeof r.msg);}
  else coop_text(r.msg,&m," (all weapons owned)",sizeof r.msg);
 }else{
  for(id=0x27u;id<=0x2cu;id++)if(!(((HasItemFn)(coop_image+0xf25f0u))((void *)player,id)&0xffu)){got=id;break;}
  if(got)((CheatGiveFn)(coop_image+0xf3440u))((void *)player,got,1,1);
  coop_text(r.msg,&m," used next armor: ",sizeof r.msg);
  if(got&&coop_item_name(got,item,sizeof item))coop_text(r.msg,&m,item,sizeof r.msg);
  else coop_text(r.msg,&m,"all armour owned",sizeof r.msg);
 }
 coop_log("cheat",r.msg);
 coop_execute(&r);
 return 1;
}
static void __attribute__((used,noinline)) coop_chat_observe(U32 player,U32 text){
 U32 client=0,object=0,now;char name[COOP_NAME_MAX],said[COOP_COMMAND_MAX+2];CoopResult r;U32 i=0;
 if(!coop_session_enabled||!coop_image||!text)return;
 coop_copy_str(said,text,sizeof said);
 while(said[i]==' '||said[i]=='\t')i++;
 if(said[i]!='!')return;
 now=GetTickCount();coop_tick(now);coop_presence(now);
 if(!coop_identify(player,&client,&object,name))return;
 if(coop_cheat_command(player,said+i+1,name))return;
 if(coop_find_client(&coop_state,client)<0){
  coop_session_seen(&coop_state,client,player,object,name,now,&r);coop_execute(&r);
 }
 coop_session_command(&coop_state,client,said,now,&r);
 if(r.action)coop_execute(&r);
}
/* Replaces "add esi,0x1e8" at 0xfeec4 inside the chat relay: ESI=player,
 * EAX=chat text. Registers and flags are preserved; the add is replayed. */
static void __attribute__((naked)) coop_chat_entry(void){
 __asm__ volatile("pushfl; pushal; pushl %eax; pushl %esi; call coop_chat_observe;"
                  "addl $8,%esp; popal; popfl; addl $0x1e8,%esi; ret");
}
/* Exit object (CExitTrigger) evidence, original 1.13 object.lto 3a03f879...:
 *   +0x54 NextWorld, +0xd3 StartPoint (both copied with '\' -> '/').
 *   +0x152 is cleared by the constructor (2d18f) and set to 1 by the property
 *   reader when NextWorld is missing/too long (2d250, 2d266), StartPoint is too
 *   long (2d2e4) or the shell's game type is non-zero (2d33f-2d352). Every exit
 *   in a multiplayer game therefore has +0x152 == 1. It is NOT a mirror flag.
 *   Handler 2d450 returns at its multiplayer gate before reading it.
 * So in multiplayer the flag cannot tell a valid exit from a broken one: the
 * session validates NextWorld itself (non-empty, exactly the next route world).
 * The activator HOBJECT comes from the trigger message (2d400 -> vtbl+0x70). */
#define COOP_EXIT_LOG_PER_EPOCH 8u
static U32 coop_exit_log_epoch=0xffffffffu,coop_exit_log_count;
static void coop_rearm_used_deco(U32 now);
static void coop_exit_trigger(U32 trigger,U32 activator_ptr){
 char next[0x80],start[0x80],line[240],who[COOP_NAME_MAX];
 U32 n=0,now=GetTickCount(),flag=0x100u,act=0,i,logged;CoopResult r;const char *kind="none";
 coop_tick(now);coop_presence(now);
 coop_copy_str(next,trigger+0x54u,0x7f);
 coop_copy_str(start,trigger+0xd3u,0x7f);
 if(bsp_mem_readable(trigger+0x152u,1))flag=*(const U8 *)(trigger+0x152u);
 who[0]=0;
 if(activator_ptr&&coop_rd(activator_ptr,&act)&&act){
  U32 live[64],count=coop_live_objects(live,64);
  kind="object";
  for(i=0;i<count;i++)if(live[i]==act){kind="player";break;}
  for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(coop_state.m[i].used&&coop_state.m[i].object==act){
   U32 k=0;kind="member";coop_text(who,&k,coop_state.m[i].name,sizeof who);break;
  }
 }
 if(coop_exit_log_epoch!=coop_state.epoch){coop_exit_log_epoch=coop_state.epoch;coop_exit_log_count=0;}
 logged=coop_exit_log_count<COOP_EXIT_LOG_PER_EPOCH;
 if(logged)coop_exit_log_count++;
 coop_text(line,&n,"next=",sizeof line);coop_text(line,&n,next,sizeof line);
 coop_text(line,&n," flag152=",sizeof line);coop_dec(line,&n,flag,sizeof line);
 coop_text(line,&n," exit=0x",sizeof line);
 {static const char hex[]="0123456789abcdef";int b;for(b=7;b>=0&&n+1<sizeof line;b--)line[n++]=hex[(trigger>>(b*4))&15u];line[n]=0;}
 coop_text(line,&n," activator=",sizeof line);coop_text(line,&n,kind,sizeof line);
 if(who[0]){coop_text(line,&n,":",sizeof line);coop_text(line,&n,who,sizeof line);}
 coop_text(line,&n," start=",sizeof line);coop_text(line,&n,start[0]?start:"-",sizeof line);
 /* Bounded: at most COOP_EXIT_LOG_PER_EPOCH arrivals are logged per map epoch. */
 if(logged)coop_log("exit-seen",line);
 if(flag!=1u){
  /* Not the native multiplayer state verified above: fail closed. */
  if(logged)coop_log("exit-refused","flag152 differs from the native multiplayer value 1");
  return;
 }
 {
  int member=-1;
  for(i=0;act&&i<COOP_SESSION_MAX_MEMBERS;i++)if(coop_state.m[i].used&&coop_state.m[i].object==act){member=(int)i;break;}
  if(!coop_session_exit_by(&coop_state,member,kind[0]=='m'||kind[0]=='p',&r)){
   if(logged)coop_log("exit-refused","not the host");
   coop_rearm_used_deco(now);
   coop_execute(&r);
   return;
  }
 }
 coop_session_exit(&coop_state,next,now,&r);
 if(r.action==COOP_ACT_CHANGE_WORLD)coop_log("exit",line);
 else if(logged&&r.action==COOP_ACT_REPLY)coop_log("exit-refused",r.msg);
 coop_execute(&r);
}
typedef void (__attribute__((thiscall)) *SpawnPrepareFn)(void *,U32 *,const U32 *,const U32 *);
static void coop_apply_skill(void){
 U32 *skill;
 if(coop_session_enabled)coop_skill=coop_state.skill;   /* !difficulty (host, lobby) or CoopSkill */
 if(coop_skill<0||!coop_image)return;
 skill=(U32 *)(coop_image+0x15b678u);
 if(bsp_mem_readable((U32)skill,4))*skill=(U32)coop_skill;
}
/* Opt-in trace ([Coop] TraceSpawners=1) of the spawner filter call 93c65:
 * cdecl f9000(skill, flags) -> AL, caller pops 8. Forwarded unchanged. */
typedef U32 (CDECL *SkillFilterFn)(U32,U32);
static U32 CDECL coop_spawner_filter(U32 skill,U32 flags){
 U32 ok=((SkillFilterFn)(coop_image+0xf9000u))(skill,flags)&0xffu;
 if(ok)coop_spawner_ok++;else coop_spawner_no++;
 if(coop_spawner_logged<64u){
  char line[160];U32 n=0;coop_spawner_logged++;
  coop_text(line,&n,"skill=",sizeof line);coop_dec(line,&n,skill,sizeof line);
  coop_text(line,&n," miscflags=",sizeof line);coop_dec(line,&n,flags&0xffu,sizeof line);
  coop_text(line,&n,ok?" create=1":" create=0",sizeof line);
  coop_log("spawner-filter",line);
 }
 return ok;
}
static void __attribute__((thiscall)) coop_next_level(void *shell){
 char world[0x80];U32 now=GetTickCount();
 if(coop_session_enabled){
  coop_inv_world_unload();   /* before the members' player objects are forgotten */
  coop_session_world_loading(&coop_state,now);coop_sb_world_loading(&coop_sb);coop_log("world-loading","native NextLevel");
  coop_apply_skill();coop_spawner_ok=coop_spawner_no=0;
 }
 ((ShellFn)(coop_image+COOP_NEXT_LEVEL_RVA))(shell);
 if(coop_session_enabled){
  coop_current_world(world);
  coop_session_world_loaded(&coop_state,world);
  if(coop_intro){
   int playable=coop_intro_snap.valid&&coop_intro_snap.epoch==coop_state.epoch&&coop_intro_world_allowed(coop_state.route_pos);
   coop_session_intro_arm(&coop_state,playable);
   if(coop_state.intro_armed)coop_log("intro-armed",coop_intro_snap.name);
  }
  coop_log("world-loaded",world[0]?world:"(unknown)");
  {CoopResult after;coop_session_after_load(&coop_state,now,&after);if(after.action)coop_execute(&after);}
  {
   char line[160];U32 n=0,skill=0;
   coop_rd((U32)coop_image+0x15b678u,&skill);
   coop_text(line,&n,"skill=",sizeof line);coop_dec(line,&n,skill,sizeof line);
   coop_text(line,&n,coop_spawners?" monster-spawners=authored":" monster-spawners=stock-mp-off",sizeof line);
   if(coop_trace_spawners){
    coop_text(line,&n," accepted=",sizeof line);coop_dec(line,&n,coop_spawner_ok,sizeof line);
    coop_text(line,&n," rejected=",sizeof line);coop_dec(line,&n,coop_spawner_no,sizeof line);
   }
   coop_log("world-content",line);
  }
 }
}

/* RVA f13a4 precedes the engine's nearby-object / telefrag handling.
 * The callee is a thiscall method (ECX=player, ret 12). Adjust the private
 * local copy of the spawn vector BEFORE forwarding the original invocation.
 * This is an experimental trial, not a collision-validated placement API. */
static void __attribute__((thiscall)) coop_before_telefrag(void *player,U32 *pos,const U32 *a,const U32 *b){
 if(coop_session_enabled){coop_spawn_hook((U32)player,pos,1);return;}
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
/* Second original spawn path (call at 0xf167a). Installed only for sessions. */
static void __attribute__((thiscall)) coop_respawn_telefrag(void *player,U32 *pos,const U32 *a,const U32 *b){
 if(coop_session_enabled){coop_spawn_hook((U32)player,pos,0);return;}
 ((SpawnPrepareFn)(spawn_image+SPAWN_PREPARE_TARGET_RVA))(player,pos,a,b);
}

/* ---- CoopSinglePlayerStart (session, default on) ----
 * Original start choice, object.lto 3a03f879:
 *   f12a0 (spawn path 1): ebp = f1070() -- the game-type strategy over the
 *     multiplayer (GameType 1) PlayerStarts; if none, it logs "Could not find
 *     any multiplayer start points", sets shell+0x1d0 (game type) to 0 around
 *     f08a0(name) and so picks the single-player start (named, else first).
 *   f15d0 (spawn path 2): f08a0(name) directly with the live game type.
 * R1M2B has no GameType-1 start and always fell back; R1M1A (9) and R1M1B
 * (16) do, so co-op players appeared at dispersed deathmatch starts. In a
 * session the strategy call at f12be returns 0 (the engine's own fallback runs)
 * and the call at f15f2 runs f08a0 with game type 0, exactly as the fallback
 * does. Players share the start point (telefrag stays skipped) until the
 * collision-checked allocator exists. */
typedef U32 (__attribute__((thiscall)) *ThisNoArgFn)(void *);
typedef U32 (__attribute__((thiscall)) *ThisNameFn)(void *,const char *);
static U32 coop_sp_start_logged;
static void coop_sp_start_log(const char *path,const char *name,U32 start){
 char line[160];U32 n=0;
 if(coop_sp_start_logged>=64u)return;
 coop_sp_start_logged++;
 coop_text(line,&n,"single-player start path=",sizeof line);coop_text(line,&n,path,sizeof line);
 coop_text(line,&n," name=",sizeof line);coop_text(line,&n,name&&name[0]?name:"-",sizeof line);
 if(start)coop_text(line,&n," found",sizeof line);
 coop_log("spawn-start",line);
}
static U32 __attribute__((thiscall)) coop_mp_start_strategy(void *player){
 if(coop_session_enabled&&coop_sp_start&&!coop_world_is_deathmatch()){coop_sp_start_log("f12be",0,0);return 0;}
 return ((ThisNoArgFn)(coop_image+0xf1070u))(player);
}
static U32 __attribute__((thiscall)) coop_find_start(void *player,const char *name){
 U32 shell=0,saved=0,start;
 if(!coop_session_enabled||!coop_sp_start||coop_world_is_deathmatch()||!coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)||
    !shell||!coop_rd(shell+0x1d0u,&saved))
  return ((ThisNameFn)(coop_image+0xf08a0u))(player,name);
 *(U32 *)(shell+0x1d0u)=0;
 start=((ThisNameFn)(coop_image+0xf08a0u))(player,name);
 *(U32 *)(shell+0x1d0u)=saved;
 {char copy_name[0x40];copy_name[0]=0;if(name)coop_copy_str(copy_name,(U32)name,sizeof copy_name);coop_sp_start_log("f15f2",copy_name,start);}
 return start;
}

/* ---- CoopScriptEvents (session, default 1): single-player script events ----
 * object.lto 3a03f879: five cinematic events return at a multiplayer gate,
 * each the 6-byte load "mov reg,[shell+0x1d0]" (game type) right before
 * "test reg,reg / jne return". Campaign scripts use them heavily
 * (MAPDATA *.SCR: player 1040 lines, disable_ai 208, cheatscript 42,
 * wake_monster_group 17):
 *   c1a89 CCinematicEventPlayer::Fire  ("player teleport/invulnerable/giveitem/
 *         animate/powerup/lookat ...": f4da0 -> dispatcher f6890)
 *   c28a5 CCinematicEventDisableAI     (6bd20: global AI disable 0x10156ed4)
 *   c269b CCinematicEventWakeMonsterGroup
 *   c2a75 CCinematicEventDisableTargetting (6bd40: 0x10156ed5)
 *   b8ad8 CCinematicEventCheatScript   (still needs the CheatScripts cvar)
 * In a session the load yields 0, so the single-player code runs. The player
 * event's single-player code applies the command to the first player only
 * (c1a93-c1abe: list 0x10157724 head -> ee7d0 -> f4da0(player, text)); its
 * call at c1abe is redirected to apply the command to every player in the
 * world, so e.g. "player invulnerable true" during a cutscene protects all of
 * them and "player giveitem gauntlets" equips everyone. */
static void __attribute__((naked)) coop_gate_zero_ecx(void){
 __asm__ volatile("xorl %ecx,%ecx; ret");
}
static void __attribute__((naked)) coop_gate_zero_edx(void){
 __asm__ volatile("xorl %edx,%edx; ret");
}
typedef U32 (CDECL *ToPlayerFn)(U32 *);
typedef U32 (__attribute__((thiscall)) *PlayerTextFn)(void *,const char *);
typedef U32 (CDECL *IsPlayerFn)(U32);
/* Activator: the player who last set off a trigger. The three trigger
 * classes test their activator with ee900 (IsPlayer, cdecl, argument a
 * POINTER to the HOBJECT: ee925 "mov ecx,[esp+4]; mov edx,[ecx]" -> AL):
 * CTriggerCommand 2e205 (filter 2e200, then its command list 2eac2) and two
 * trigger touch paths 2ffea / 3005a. The result is forwarded unchanged.
 * (Until 2026-11-12 the pointer itself was stored, so it never matched a
 * live player and every personal command went to the first player: the
 * "player 2 teleported in front of the camera" playtest report.) */
static U32 coop_activator,coop_activator_ms;
static void coop_set_activator(U32 handle){if(handle){coop_activator=handle;coop_activator_ms=GetTickCount();}}
static U32 CDECL coop_record_activator(U32 object_ptr){
 U32 r=((IsPlayerFn)(coop_image+0xee900u))(object_ptr),h=0;
 if((r&0xffu)&&coop_rd(object_ptr,&h))coop_set_activator(h);
 return r;
}
static int coop_member_of(U32 handle){
 U32 i;
 for(i=0;handle&&i<COOP_SESSION_MAX_MEMBERS;i++)if(coop_state.m[i].used&&coop_state.m[i].object==handle)return (int)i;
 return -1;
}

/* ---- Host-only level-ending sequences (with CoopScriptEvents) ----
 * object.lto script registry: loaded scripts at [0x1015a0b8] (count
 * [0x1015a0b4]); c63f0(name, script) (cdecl) is 1 when the names match.
 * A script keeps its events at script+0x108 (array) / +0x104 (count), each
 * event [vtable, float time, ...]. The events that name something keep it
 * at event+0x108: trigger_generic (vtable 0x1011d4a8, factory bf560),
 * scriptplay (0x1011d0d4, Fire b8670 -> c5340) and endingscript (0x1011d128,
 * Fire b8cf0 -> c50c0: the script that runs when the sequence is skipped).
 * A CTriggerCommand's commands are themselves a registered script whose name
 * is the command-list object [trigger+0x54] (c4230: played if byte +0x40). */
typedef U32 (CDECL *ScriptMatchFn)(const char *,U32);
static U32 coop_find_script(const char *name){
 U32 count=0,arr=0,i,s;
 if(!name||!bsp_mem_readable((U32)name,1)||!name[0])return 0;
 if(!coop_rd((U32)coop_image+0x15a0b4u,&count)||!coop_rd((U32)coop_image+0x15a0b8u,&arr)||!arr||count>4096u)return 0;
 for(i=0;i<count;i++)if(coop_rd(arr+i*4u,&s)&&s&&(((ScriptMatchFn)(coop_image+0xc63f0u))(name,s)&0xffu)==1u)return s;
 return 0;
}
static int coop_script_reaches_exit(U32 script,const char *label,U32 depth){
 U32 count=0,arr=0,i,ev,vt;char name[0x80];
 if(!script||depth>4u||!coop_rd(script+0x104u,&count)||!coop_rd(script+0x108u,&arr)||!arr||count>4096u)return 0;
 for(i=0;i<count;i++){
  if(!coop_rd(arr+i*4u,&ev)||!ev||!coop_rd(ev,&vt))continue;
  if(vt!=(U32)coop_image+0x11d4a8u&&vt!=(U32)coop_image+0x11d0d4u&&vt!=(U32)coop_image+0x11d128u)continue;
  coop_copy_str(name,ev+0x108u,sizeof name);
  if(vt==(U32)coop_image+0x11d4a8u){if(coop_is_exit_name(label,name))return 1;}
  else if(coop_script_reaches_exit(coop_find_script(name),label,depth+1u))return 1;
 }
 return 0;
}
static int coop_commands_reach_exit(U32 cmdlist){
 U8 armed=0;
 if(!cmdlist||!bsp_mem_readable(cmdlist+0x40u,1))return 0;
 armed=*(const U8 *)(cmdlist+0x40u);
 if(!armed||!coop_state.world_label[0])return 0;
 return coop_script_reaches_exit(coop_find_script((const char *)cmdlist),coop_state.world_label,0);
}
/* CTriggerCommand filter 2e200 (ECX = trigger, unchanged before 2e205):
 * a non-host player in a trigger that starts a level-ending sequence is
 * reported as "not a player", so 2eaa0 neither plays the commands nor marks
 * the trigger used (2eabd -> 2eacb): it stays armed for the host. */
static U32 coop_seq_refused_logged;
static U32 __attribute__((used,noinline)) CDECL coop_trigger_filter(U32 object_ptr,U32 trigger){
 U32 r=coop_record_activator(object_ptr),h=0,cmd=0,now;int member;CoopResult res;
 if(!(r&0xffu)||!coop_host_sequences||!coop_rd(object_ptr,&h)||!coop_rd(trigger+0x54u,&cmd))return r;
 if(coop_state.phase!=COOP_PHASE_GAMEPLAY)return r;
 member=coop_member_of(h);
 if(member>=0&&member==coop_state.host)return r;
 if(!coop_commands_reach_exit(cmd))return r;
 now=GetTickCount();
 if(coop_session_sequence_by(&coop_state,member,now,&res))return r;
 if(res.action){
  char line[120];U32 n=0;
  coop_text(line,&n,"trigger refused for a non-host player (",sizeof line);
  coop_text(line,&n,member>=0?coop_state.m[member].name:"unknown",sizeof line);
  coop_text(line,&n,"): its commands reach the exit of ",sizeof line);coop_text(line,&n,coop_state.world_label,sizeof line);
  coop_log("sequence-refused",line);coop_seq_refused_logged++;
  coop_execute(&res);
 }
 return 0;
}
void coop_trigger_filter_entry(void);
__asm__(".global coop_trigger_filter_entry\n"
        "coop_trigger_filter_entry:\n"
        " pushl %ecx\n"
        " pushl 8(%esp)\n"
        " call coop_trigger_filter\n"
        " addl $8,%esp\n"
        " ret\n");

/* CModelDeco use (409a0, vtable 0x119d68): plays its use command list
 * (409bb call c4230) and marks itself used (+0x1a1 = 1, honoured when
 * play_use_script_once +0x178 is set). An exit mirror ("trigger_generic
 * Gubba") used by a non-host was refused at the exit but stayed used, so
 * the host's later click did nothing. The last used deco is remembered;
 * coop_exit_trigger re-arms it when it refuses that exit. */
static U32 coop_used_deco,coop_used_deco_ms;
U32 coop_c4230_target;
static void __attribute__((used,noinline)) CDECL coop_deco_used(U32 deco){coop_used_deco=deco;coop_used_deco_ms=GetTickCount();}
void coop_deco_use_entry(void);
__asm__(".global coop_deco_use_entry\n"
        "coop_deco_use_entry:\n"
        " pushl %ecx\n"
        " pushl %esi\n"
        " call coop_deco_used\n"
        " addl $4,%esp\n"
        " popl %ecx\n"
        " jmp *coop_c4230_target\n");
#define COOP_DECO_REARM_MS 3000u
static void coop_rearm_used_deco(U32 now){
 U8 *used;
 if(!coop_used_deco||now-coop_used_deco_ms>COOP_DECO_REARM_MS)return;
 used=(U8 *)(coop_used_deco+0x1a1u);
 if(bsp_mem_readable((U32)used,1)&&*used==1u){*used=0;coop_log("exit-rearmed","the refused exit's use object can be used again by the host");}
 coop_used_deco=0;
}
/* Sub-commands that move or animate one body go to the activator only;
 * the rest (giveitem, invulnerable, sethealth, class, ...) to everyone. */
static int coop_player_command_personal(const char *text){
 static const char *const words[]={"teleport","lookatangles","lookatangle","lookat","powerup","animate","alignbody"};
 char w[16];U32 i,k;
 if(!text)return 0;
 for(i=0;text[i]==' '||text[i]=='\t';i++){}
 for(k=0;k<15&&text[i+k]&&text[i+k]!=' '&&text[i+k]!='\t';k++)w[k]=coop_lower(text[i+k]);
 w[k]=0;
 for(i=0;i<sizeof(words)/sizeof(words[0]);i++)if(coop_word(w,words[i]))return 1;
 return 0;
}
#define COOP_ACTIVATOR_MS 60000u
static U32 coop_player_event_epoch=0xffffffffu,coop_player_event_logged;
static U32 __attribute__((thiscall)) coop_player_event_all(void *head,const char *text){
 U32 live[64],n=coop_live_objects(live,64),i,applied=0,result=0,personal=coop_player_command_personal(text),only=0;
 if(personal){
  /* The activator if it is still a player in this world and recent, else the
   * first player (the single-player code's choice, which is "head"). */
  if(coop_activator&&GetTickCount()-coop_activator_ms<COOP_ACTIVATOR_MS)
   for(i=0;i<n;i++)if(live[i]==coop_activator){only=coop_activator;break;}
 }
 for(i=0;i<n;i++){
  U32 h=live[i],player;
  if(personal&&(only?h!=only:applied))continue;
  player=((ToPlayerFn)(coop_image+0xee7d0u))(&h);
  if(!player)continue;
  result=((PlayerTextFn)(coop_image+0xf4da0u))((void *)player,text);applied++;
 }
 if(!applied&&head)result=((PlayerTextFn)(coop_image+0xf4da0u))(head,text);
 /* "player powerup" (f6ff0) broadcasts message 0x39 (activator HOBJECT,
  * effect 0x35) once. An observer whose client does not hold the activator's
  * object yet (it was just teleported into the cutscene spot) drops it, so
  * it is sent to every other member again shortly after (coop_powerup_frame). */
 if(personal&&only&&coop_powerup_resend&&text){
  U32 k=0;while(text[k]==' '||text[k]=='\t')k++;
  if(coop_lower(text[k])=='p'&&coop_lower(text[k+1])=='o'&&coop_lower(text[k+2])=='w'&&coop_lower(text[k+3])=='e'&&
     coop_lower(text[k+4])=='r'&&coop_lower(text[k+5])=='u'&&coop_lower(text[k+6])=='p'&&(text[k+7]==0||text[k+7]==' '||text[k+7]=='\t')){
   coop_powerup_obj=only;coop_powerup_due=GetTickCount()+400u;
  }
 }
 if(coop_player_event_epoch!=coop_state.epoch){coop_player_event_epoch=coop_state.epoch;coop_player_event_logged=0;}
 if(coop_player_event_logged<24u){
  char line[200],cmd[0x60];U32 m=0;coop_player_event_logged++;
  cmd[0]=0;if(text)coop_copy_str(cmd,(U32)text,sizeof cmd);
  coop_text(line,&m,"player ",sizeof line);coop_text(line,&m,cmd,sizeof line);
  coop_text(line,&m,personal?(only?" -> activator, players=":" -> first player, players="):" -> players=",sizeof line);coop_dec(line,&m,applied,sizeof line);
  coop_log("script-event",line);
 }
 return result;
}

/* Cutscene camera view (camera_path, not gated in multiplayer): c2c60 sends
 * the camera to every client (c58c0, MID 0x3d sub 5) but moves only the FIRST
 * player's server view position (c2c9c ef920(player, pos): [player+0x98]
 * +0x10 = 1, +0x14 = pos), which decides what the server sends to that
 * client. Every other client kept its own body's view, so objects seen only
 * from the camera (R1M1B: the golden mirror behind the opened gate) could be
 * missing on their screens. Both calls (set c2c9c, reset c2c56 ef950) are
 * applied to every player in the world. */
typedef void (__attribute__((thiscall)) *ViewSetFn)(void *,const void *);
typedef void (__attribute__((thiscall)) *ViewResetFn)(void *);
static void __attribute__((thiscall)) coop_view_set_all(void *head,const void *pos){
 U32 live[64],n=coop_live_objects(live,64),i,done=0;
 for(i=0;i<n;i++){
  U32 h=live[i],player=((ToPlayerFn)(coop_image+0xee7d0u))(&h);
  if(player){((ViewSetFn)(coop_image+0xef920u))((void *)player,pos);done++;}
 }
 if(!done&&head)((ViewSetFn)(coop_image+0xef920u))(head,pos);
}
static void __attribute__((thiscall)) coop_view_reset_all(void *head){
 U32 live[64],n=coop_live_objects(live,64),i,done=0;
 for(i=0;i<n;i++){
  U32 h=live[i],player=((ToPlayerFn)(coop_image+0xee7d0u))(&h);
  if(player){((ViewResetFn)(coop_image+0xef950u))((void *)player);done++;}
 }
 if(!done&&head)((ViewResetFn)(coop_image+0xef950u))(head);
}

/* ---- CoopFriendlyFire (session, default 0 = off) ----
 * object.lto CPlayer damage handler f3bc0 (thiscall, damage record ebp):
 * at f3c08 "cmp [shell+0x1f0],edi" tests teamplay == 1; the teamplay branch
 * skips the damage (jump f3f1e) when the damager [rec+0x20] is the player
 * itself or a player of the same team (+0x24c). Co-op keeps self-damage and
 * monster damage but drops damage from any other player: a co-op death
 * would cost the victim every weapon, armour piece and its ammo. */
U32 coop_ff_skip_target;
static U32 coop_ff_skipped;
static U32 __attribute__((used,noinline)) CDECL coop_ff_decide(U32 player,U32 record){
 U32 damager=0,self=0;
 if(!coop_rd(record+0x20u,&damager)||!damager||!coop_rd(player+8u,&self)||damager==self)return 0;
 if(!((ToPlayerFn)(coop_image+0xee7d0u))(&damager))return 0;
 coop_ff_skipped++;
 return 1;
}
void coop_ff_entry(void);
__asm__(".global coop_ff_entry\n"
        "coop_ff_entry:\n"
        " pushal\n"
        " pushl %ebp\n"
        " pushl %esi\n"
        " call coop_ff_decide\n"
        " addl $8,%esp\n"
        " testl %eax,%eax\n"
        " popal\n"
        " jz 1f\n"
        " addl $4,%esp\n"
        " jmp *coop_ff_skip_target\n"
        "1:\n"
        " cmpl %edi,0x1f0(%eax)\n"
        " ret\n");

/* ---- CoopSharedItems (session, default 1): progression pickups for everyone ----
 * object.lto pickup path (every item class): cc120(item, toucher) resolves the
 * toucher (ee7d0), skips a dead one (vt+0x60) and calls cc180(item, player)
 * at cc145 (thiscall, ret 4). cc180 reads the item definition item+0x54
 * ([def] = item id, ids from the table at 0x141e40, stride 0x20) and runs the
 * item's handler. After the toucher's own pickup, a progression item is given
 * to every other player with the engine's own give call f3440(player, id, 1, 1)
 * (thiscall, ret 0xc; the spawn code gives start armour this way, f1492).
 * Shared ids: weapons 3-14 (BeastClaws..Stargaze) and UltimateWeapon 0x23,
 * Pamphlet 0x19, Key 0x24, Ticket 0x25, armour 0x27-0x2c. Health, ammo,
 * charms and other power-ups stay with whoever takes them. Each item object
 * is shared at most once per world (a refused pickup leaves the item lying,
 * so repeated touches cannot hand it out again). */
typedef void (__attribute__((thiscall)) *PickupFn)(void *,void *);
static U32 coop_ammo_logged;
typedef U32 (__attribute__((thiscall)) *GiveItemFn)(void *,U32,U32,U32);
#define COOP_SHARED_MAX 128u
static U32 coop_shared_list[COOP_SHARED_MAX],coop_shared_count,coop_shared_epoch=0xffffffffu;
static int coop_item_shared_id(U32 id){
 return (id>=3u&&id<=14u)||id==0x23u||id==0x19u||id==0x24u||id==0x25u||(id>=0x27u&&id<=0x2cu);
}
/* Armour touch c8340 (all six piece classes, vtables 0x11db1c..0x11ddd4):
 * byte 0x1013e1b8 = 1 in multiplayer (c8348 game-type load), else the
 * item's ReallyGiveArmor (+0xe8); the use function c8310 gives the piece
 * only when it is 1. So multiplayer handed the piece over at the touch,
 * playing the unsheathe sound at once, while single player gives nothing
 * at the touch: the piece's script ("player giveitem <piece>" at 6.75-8.75 s,
 * e.g. r1m1a_gauntlets.scr, cs1belt.scr) gives it near the end of its
 * cutscene. With CoopScriptEvents the c8348 load yields 0 (single-player
 * rule) and that giveitem reaches every player; a piece whose
 * ReallyGiveArmor is set (TUTORIAL) is still given at the touch and shared. */
#define COOP_ARMOUR_GIVE_FLAG_RVA 0x13e1b8u
static int coop_item_armour_id(U32 id){return id>=0x27u&&id<=0x2cu;}
/* Ammo (ids 0x11-0x14 Magma/Water/Air/Earth): what the picker actually
 * received (inventory count dd190 before and after the pickup; f3440 caps
 * ammo at 100 + 100 per armour piece) is added to every other player with
 * f3440(player, id, amount, 1). Firing uses each player's own count, so
 * consumption stays personal. Every pickup is shared (respawning ammo too). */
typedef U32 (__attribute__((thiscall)) *InvCountFn)(void *,U32);
static int coop_item_ammo_id(U32 id){return id>=0x11u&&id<=0x14u;}
static U32 coop_ammo_count(U32 player,U32 id){
 U32 inv=0;
 if(!player||!coop_rd(player+0x54u,&inv)||!inv)return 0;
 return ((InvCountFn)(coop_image+0xdd190u))((void *)inv,id);
}
static void coop_share_ammo(U32 player,U32 id,U32 amount){
 U32 live[64],n=coop_live_objects(live,64),i,given=0,self=0;
 coop_rd(player+8u,&self);
 for(i=0;i<n;i++){
  U32 h=live[i],other;
  if(h==self)continue;
  other=((ToPlayerFn)(coop_image+0xee7d0u))(&h);
  if(!other||other==player)continue;
  ((GiveItemFn)(coop_image+0xf3440u))((void *)other,id,amount,1);given++;
 }
 if(coop_ammo_logged<32u){
  char line[120],name[24];U32 m=0,np=0;coop_ammo_logged++;
  name[0]=0;if(coop_rd((U32)coop_image+0x141e40u+id*0x20u,&np)&&np)coop_copy_str(name,np,sizeof name);
  coop_text(line,&m,"shared ammo=",sizeof line);coop_text(line,&m,name[0]?name:"?",sizeof line);
  coop_text(line,&m," amount=",sizeof line);coop_dec(line,&m,amount,sizeof line);
  coop_text(line,&m," to ",sizeof line);coop_dec(line,&m,given,sizeof line);coop_text(line,&m," other player(s)",sizeof line);
  coop_log("item",line);
 }
}
static void __attribute__((thiscall)) coop_item_pickup(void *item,void *player){
 U32 def=0,id=0,live[64],n,i,given=0,self=0,before=0,after;
 if(coop_rd((U32)player+8u,&self))coop_set_activator(self);   /* a pickup can start a cutscene */
 if(coop_rd((U32)item+0x54u,&def)&&def&&coop_rd(def,&id)&&coop_item_ammo_id(id))before=coop_ammo_count((U32)player,id);
 ((PickupFn)(coop_image+0xcc180u))(item,player);
 if(coop_item_ammo_id(id)){
  after=coop_ammo_count((U32)player,id);
  if(after>before)coop_share_ammo((U32)player,id,after-before);
  return;
 }
 if(!coop_rd((U32)item+0x54u,&def)||!def||!coop_rd(def,&id)||!coop_item_shared_id(id))return;
 if(coop_item_armour_id(id)&&coop_script_events&&*(const U8 *)(coop_image+COOP_ARMOUR_GIVE_FLAG_RVA)!=1u){
  coop_log("item","armour piece touched; its cutscene gives it to every player (single-player timing)");
  return;
 }
 if(coop_shared_epoch!=coop_state.epoch){coop_shared_epoch=coop_state.epoch;coop_shared_count=0;}
 for(i=0;i<coop_shared_count;i++)if(coop_shared_list[i]==(U32)item)return;
 if(coop_shared_count<COOP_SHARED_MAX)coop_shared_list[coop_shared_count++]=(U32)item;
 coop_rd((U32)player+8u,&self);
 n=coop_live_objects(live,64);
 for(i=0;i<n;i++){
  U32 h=live[i],other;
  if(h==self)continue;
  other=((ToPlayerFn)(coop_image+0xee7d0u))(&h);
  if(!other||other==(U32)player)continue;
  ((GiveItemFn)(coop_image+0xf3440u))((void *)other,id,1,1);given++;
 }
 {
  char line[120],name[24];U32 m=0,np=0;
  name[0]=0;if(coop_rd((U32)coop_image+0x141e40u+id*0x20u,&np)&&np)coop_copy_str(name,np,sizeof name);
  coop_text(line,&m,"shared item=",sizeof line);coop_text(line,&m,name[0]?name:"?",sizeof line);
  coop_text(line,&m," id=",sizeof line);coop_dec(line,&m,id,sizeof line);
  coop_text(line,&m," given to ",sizeof line);coop_dec(line,&m,given,sizeof line);coop_text(line,&m," other player(s)",sizeof line);
  coop_log("item",line);
 }
}

/* Secrets: CTriggerBrush_Secret touch (30c20) fires once per secret and
 * counts it in one world counter (0x1015b690, found; 0x1015b68c, total), so
 * the count is already shared. Only the "secret found" message went to the
 * finder: 30c9e f8d90(client, 1, 3.0f, text) (cdecl). With CoopSharedItems
 * every session member gets it. */
typedef void (CDECL *ClientMessageFn)(U32,U32,U32,U32);
static void CDECL coop_secret_message(U32 client,U32 kind,U32 seconds,U32 text){
 U32 i,sent=0;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(coop_state.m[i].used&&coop_state.m[i].client){
  ((ClientMessageFn)(coop_image+0xf8d90u))(coop_state.m[i].client,kind,seconds,text);sent++;
 }
 if(!sent)((ClientMessageFn)(coop_image+0xf8d90u))(client,kind,seconds,text);
 coop_log("secret","found; message sent to every player");
}

/* Keys and tickets: the campaign has no key item objects. A CPickupTrigger
 * runs "inventory add key" (20 worlds) and a door runs "inventory remove
 * key". The inventory event's Fire b6730 has no game-type gate but uses the
 * FIRST player only (b673c list 0x10157724 head -> ee7d0), so in co-op the
 * key went to whichever player was first, not the finder. Its add call
 * b676f f3440(player, id, 1, 1) (thiscall, ret 0xc) is redirected to give
 * the item to every player and to announce keys and tickets with the
 * finder's name (the pickup trigger's activator). Removal (b6789, the
 * inventory's own vt+4) stays with the first player: the others keep a
 * spare key, which only matters on worlds with two key doors. */
#define COOP_ITEM_KEY 0x24u
#define COOP_ITEM_TICKET 0x25u
#define COOP_KEY_FINDER_MS 3000u
static U32 __attribute__((thiscall)) coop_inventory_give(void *head,U32 id,U32 a,U32 b){
 U32 live[64],n=coop_live_objects(live,64),i,given=0,result=0,now=GetTickCount();
 for(i=0;i<n;i++){
  U32 h=live[i],player=((ToPlayerFn)(coop_image+0xee7d0u))(&h);
  if(!player)continue;
  result=((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,a,b);given++;
 }
 if(!given&&head)result=((GiveItemFn)(coop_image+0xf3440u))(head,id,a,b);
 {
  char line[120],name[24];U32 m=0,np=0;
  name[0]=0;if(coop_rd((U32)coop_image+0x141e40u+id*0x20u,&np)&&np)coop_copy_str(name,np,sizeof name);
  coop_text(line,&m,"inventory add item=",sizeof line);coop_text(line,&m,name[0]?name:"?",sizeof line);
  coop_text(line,&m," id=",sizeof line);coop_dec(line,&m,id,sizeof line);
  coop_text(line,&m," players=",sizeof line);coop_dec(line,&m,given,sizeof line);
  coop_log("item",line);
 }
 if(id==COOP_ITEM_KEY||id==COOP_ITEM_TICKET){
  CoopResult r;U32 m=0;int who=-1;
  if(coop_activator&&now-coop_activator_ms<COOP_KEY_FINDER_MS)who=coop_member_of(coop_activator);
  coop_result(&r,COOP_ACT_ANNOUNCE,"");
  coop_text(r.msg,&m,"[Co-op] ",sizeof r.msg);
  coop_text(r.msg,&m,who>=0?coop_state.m[who].name:"Someone",sizeof r.msg);
  coop_text(r.msg,&m,id==COOP_ITEM_KEY?" found the key (everyone has it)":" found a ticket (everyone has it)",sizeof r.msg);
  coop_execute(&r);
 }
 return result;
}

/* ---- Waiting player placement (with CoopSinglePlayerStart) ----
 * object.lto client enter fdfb0: single player places the new player at once
 * (fe010 -> f15d0); multiplayer only attaches the object to its client
 * (fdfdb: [g_pServerDE+0xf0](client, player)) and sends 0x17. The object is
 * not placed until the client's 0x17 reply runs the spawn (fec56 -> f12a0).
 * While a KPCFix client holds that reply behind its loading picture, the
 * unplaced player fell out of the world and died (the death sound heard behind
 * the picture; f12a0 then respawned it). After the original call the player
 * is moved to the single-player start (f08a0 with game type 0, as the spawn's
 * fallback does; GetObjectPos [de+0x1d0], SetObjectPos [de+0x1e0], cdecl, as
 * in f1355/f13c4). Nothing else changes: the real spawn still happens on the
 * reply, so intros, music and the session see it as before. */
typedef void (CDECL *ClientObjFn)(U32,U32);
typedef void (CDECL *ObjVecFn)(U32,float *);
static U32 coop_staged_epoch=0xffffffffu,coop_staged_logged;
static void CDECL coop_client_enter_place(U32 client,U32 player){
 U32 de=0,shell=0,saved=0,start=0,start_obj=0,player_obj=0,fn=0;float pos[3];
 if(coop_rd((U32)coop_image+0x16de80u,&de)&&de&&coop_rd(de+0xf0u,&fn)&&fn)((ClientObjFn)fn)(client,player);
 if(!coop_session_enabled||!coop_sp_start||!player||!de||coop_world_is_deathmatch())return;
 if(!coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)||!shell||!coop_rd(shell+0x1d0u,&saved)||!saved)return;
 *(U32 *)(shell+0x1d0u)=0;
 start=((ThisNameFn)(coop_image+0xf08a0u))((void *)player,(const char *)(shell+0x4cu));
 *(U32 *)(shell+0x1d0u)=saved;
 if(!start||!coop_rd(start+8u,&start_obj)||!start_obj||!coop_rd(player+8u,&player_obj)||!player_obj)return;
 if(!coop_rd(de+0x1d0u,&fn)||!fn)return;
 ((ObjVecFn)fn)(start_obj,pos);
 if(!coop_rd(de+0x1e0u,&fn)||!fn)return;
 ((ObjVecFn)fn)(player_obj,pos);
 if(coop_staged_epoch!=coop_state.epoch){coop_staged_epoch=coop_state.epoch;coop_staged_logged=0;}
 if(coop_staged_logged<8u){coop_staged_logged++;coop_log("enter","player placed at the single-player start until it spawns");}
}

/* Player pointer (CPlayer*) of a session member in the current world, or 0. */
static U32 coop_member_player(int i){
 U32 live[64],n,k;
 if(i<0||(U32)i>=COOP_SESSION_MAX_MEMBERS||!coop_state.m[i].used||!coop_state.m[i].object)return 0;
 n=coop_live_objects(live,64);
 for(k=0;k<n;k++)if(live[k]==coop_state.m[i].object){U32 h=live[k];return ((ToPlayerFn)(coop_image+0xee7d0u))(&h);}
 return 0;
}
/* CPlayer vtable +0x60: 1 while dead (the pickup path cc13b skips such a toucher). */
typedef U32 (__attribute__((thiscall)) *ThisBoolFn)(void *);
static int coop_player_dead(U32 player){
 U32 vt=0,fn=0;
 if(!player||!coop_rd(player,&vt)||!vt||!coop_rd(vt+0x60u,&fn)||!fn)return 1;
 return (((ThisBoolFn)fn)((void *)player)&0xffu)!=0;
}

/* ---- CoopSpawnerNearest (session, default 1) ----
 * Spawners (CMonsterSpawner 93cd9, CSpawner 9b97e, CBaseSpawner d4ef8 all
 * call d5050 then d53e0 every update) keep a reference object at +0x170:
 * activation (d5490) stores the FIRST player of the list (d54c8: 7d220 on
 * 0x10157724), a trigger message 0x27 the sender (d501a). Each update d5050
 * measures it (GetObjectPos d51fb, distance^2 to the spawner +0x190) and
 * d53e0 spawns only while +0x190 <= ActiveRadius^2 (+0x178; UseActiveRadius
 * +0x174; ActivatorVisible +0x17c uses +0x170 too). In co-op the first player
 * is whoever joined first: when another player set off a spawner room (R4M1A
 * "topheadspawns", radius 1024) the reference stood elsewhere and nothing
 * spawned. The position read at d51fb (ESI = spawner) is replaced: when the
 * reference is a player, the nearest live player is used and stored as the
 * new reference. A non-player reference (a trigger) is measured as before. */
static U32 coop_spawner_logged_epoch=0xffffffffu,coop_spawner_logged_n;
static void __attribute__((used,noinline)) CDECL coop_spawner_target(U32 obj,float *pos,U32 spawner){
 U32 de=0,fn=0,live[64],n,i,self=0,best=0,is_player=0;float sp[3],p[3],bp[3],bd=0.f;
 if(!coop_rd((U32)coop_image+0x16de80u,&de)||!de||!coop_rd(de+0x1d0u,&fn)||!fn)return;
 n=coop_spawner_nearest?coop_live_objects(live,64):0;
 for(i=0;i<n;i++)if(live[i]==obj){is_player=1;break;}
 if(!is_player||n<2||!coop_rd(spawner+8u,&self)||!self){((ObjVecFn)fn)(obj,pos);return;}
 ((ObjVecFn)fn)(self,sp);
 for(i=0;i<n;i++){
  U32 h=live[i];float dx,dy,dz,d;
  if(!((ToPlayerFn)(coop_image+0xee7d0u))(&h))continue;
  ((ObjVecFn)fn)(live[i],p);
  dx=p[0]-sp[0];dy=p[1]-sp[1];dz=p[2]-sp[2];d=dx*dx+dy*dy+dz*dz;
  if(!best||d<bd){best=live[i];bd=d;bp[0]=p[0];bp[1]=p[1];bp[2]=p[2];}
 }
 if(!best){((ObjVecFn)fn)(obj,pos);return;}
 pos[0]=bp[0];pos[1]=bp[1];pos[2]=bp[2];
 if(best!=obj&&bsp_mem_readable(spawner+0x170u,4)){
  *(U32 *)(spawner+0x170u)=best;
  if(coop_spawner_logged_epoch!=coop_state.epoch){coop_spawner_logged_epoch=coop_state.epoch;coop_spawner_logged_n=0;}
  if(coop_spawner_logged_n<16u){
   char line[120];U32 k=0;int mi=coop_member_of(best);coop_spawner_logged_n++;
   coop_text(line,&k,"reference moved to the nearest player ",sizeof line);
   coop_text(line,&k,mi>=0?coop_state.m[mi].name:"?",sizeof line);
   coop_text(line,&k," spawner=0x",sizeof line);
   {static const char hex[]="0123456789abcdef";int b;for(b=7;b>=0&&k+1<sizeof line;b--)line[k++]=hex[(spawner>>(b*4))&15u];line[k]=0;}
   coop_log("spawner",line);
  }
 }
}
/* Site d51fb: "call [eax+0x1d0]" (cdecl GetObjectPos(obj, pos), caller pops). */
void coop_spawner_pos_entry(void);
__asm__(".global coop_spawner_pos_entry\n"
        "coop_spawner_pos_entry:\n"
        " pushl %esi\n"
        " pushl 12(%esp)\n"
        " pushl 12(%esp)\n"
        " call coop_spawner_target\n"
        " addl $12,%esp\n"
        " ret\n");

/* ---- Inventory persistence (CoopKeepInventory / CoopKeepOnDeath) ----
 * Snapshot per session member (keyed by the member's client, so it survives
 * world changes and !abort; a new client in the slot drops it), taken every
 * second while the member is alive and right before every world change.
 * A spawn queues a restore that runs COOP_INV_RESTORE_MS later from the
 * per-frame hook, after the spawn function (f12a0: position, start items)
 * has finished. The restore only gives what is missing (coop_inv_plan). */
#define COOP_INV_RESTORE_MS 400u
#define COOP_INV_SNAPSHOT_MS 1000u
static CoopInvSnap coop_inv_snap[COOP_SESSION_MAX_MEMBERS];
static U32 coop_inv_pending[COOP_SESSION_MAX_MEMBERS],coop_inv_due[COOP_SESSION_MAX_MEMBERS],coop_inv_last_ms;
static U32 coop_has_item(U32 player,U32 id){return ((HasItemFn)(coop_image+0xf25f0u))((void *)player,id)&0xffu;}
static int coop_inv_read(U32 player,CoopInvState *st){
 U32 inv=0,id,k;
 if(!player||!coop_rd(player+0x54u,&inv)||!inv||!coop_rd(player+0x264u,&st->cls))return 0;
 st->weapons=0;st->armour=0;
 for(id=COOP_INV_WEAPON_FIRST;id<=COOP_INV_WEAPON_LAST;id++)if(coop_has_item(player,id))st->weapons|=(unsigned short)(1u<<(id-COOP_INV_WEAPON_FIRST));
 for(id=COOP_INV_ARMOUR_FIRST;id<=COOP_INV_ARMOUR_LAST;id++)if(coop_has_item(player,id))st->armour|=(unsigned char)(1u<<(id-COOP_INV_ARMOUR_FIRST));
 for(k=0;k<4u;k++)st->ammo[k]=((InvCountFn)(coop_image+0xdd190u))((void *)inv,COOP_INV_AMMO_FIRST+k);
 st->shards=((InvCountFn)(coop_image+0xdd190u))((void *)inv,COOP_INV_SHARD);
 return 1;
}
/* A weapon belongs to one class unless its definition allows any class
 * (f3482-f349b: [player+0x54+id*4] weapon object, +8 definition, def+8 class,
 * def+0xc any-class byte). One kept from another class (class cycle) is
 * given with the player's class set to the weapon's for the call. */
static U32 coop_give_weapon(U32 player,U32 id){
 U32 obj=0,def=0,need=0,cls=0,r;U8 any=1;
 if(coop_rd(player+0x54u+id*4u,&obj)&&obj&&coop_rd(obj+8u,&def)&&def&&coop_rd(def+8u,&need)&&bsp_mem_readable(def+0xcu,1))any=*(const U8 *)(def+0xcu);
 if(!coop_rd(player+0x264u,&cls)||any||need>=COOP_INV_CLASSES||need==cls)
  return ((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,1,1);
 *(U32 *)(player+0x264u)=need;
 r=((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,1,1);
 *(U32 *)(player+0x264u)=cls;
 return r;
}
typedef void (__attribute__((thiscall)) *ClassApplyFn)(void *,U32);
static void coop_set_class(U32 player,U32 cls){
 *(U32 *)(player+0x264u)=cls&3u;
 ((ClassApplyFn)(coop_image+0xf2090u))((void *)player,1);   /* as the stock cheat fab30 */
}
/* Give what `saved` has and the player lacks; returns the number of gives. */
static U32 coop_inv_apply(U32 player,const CoopInvState *saved){
 CoopInvState now;CoopInvPlan p;U32 i,n=0;
 if(!coop_inv_read(player,&now))return 0;
 coop_inv_plan(saved,&now,&p);
 if(p.set_class>=0){coop_set_class(player,(U32)p.set_class);n++;}
 for(i=0;i<p.count;i++){
  U32 id=p.give[i].id;
  if(coop_inv_armour(id)){((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,1,1);n++;}
  else if(coop_inv_weapon(id)){coop_give_weapon(player,id);n++;}
 }
 /* Shards and ammo after armour (ammo cap) and weapons (their own ammo). */
 if(!coop_inv_read(player,&now))return n;
 coop_inv_plan(saved,&now,&p);
 for(i=0;i<p.count;i++){
  U32 id=p.give[i].id;
  if(id==COOP_INV_SHARD||coop_inv_ammo(id)){((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,p.give[i].amount,1);n++;}
 }
 return n;
}
static void coop_inv_take(int i,U32 player){
 CoopInvState st;
 if(i<0||(U32)i>=COOP_SESSION_MAX_MEMBERS||coop_inv_pending[i]||!player||coop_player_dead(player)||!coop_inv_read(player,&st))return;
 coop_inv_snap[i].valid=1;coop_inv_snap[i].client=coop_state.m[i].client;coop_inv_snap[i].s=st;
}
static void coop_inv_log(const char *event,int i,const CoopInvState *s,U32 gives){
 char line[200];U32 k=0;
 coop_text(line,&k,coop_state.m[i].name,sizeof line);
 coop_text(line,&k," class=",sizeof line);coop_text(line,&k,coop_class_name(s->cls),sizeof line);
 coop_text(line,&k," weapons=",sizeof line);coop_dec(line,&k,coop_inv_bits(s->weapons),sizeof line);
 coop_text(line,&k," armour=",sizeof line);coop_dec(line,&k,coop_inv_bits(s->armour),sizeof line);
 coop_text(line,&k," shards=",sizeof line);coop_dec(line,&k,s->shards,sizeof line);
 coop_text(line,&k," ammo=",sizeof line);
 {U32 a;for(a=0;a<4u;a++){if(a)coop_text(line,&k,"/",sizeof line);coop_dec(line,&k,s->ammo[a],sizeof line);}}
 if(gives!=0xffffffffu){coop_text(line,&k," gives=",sizeof line);coop_dec(line,&k,gives,sizeof line);}
 coop_log(event,line);
}
static void coop_inv_world_unload(void){
 U32 i;
 if(!coop_keep_inventory)return;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  U32 pl;
  if(!coop_state.m[i].used||coop_state.m[i].seen_epoch!=coop_state.epoch||!coop_state.m[i].spawned)continue;
  pl=coop_member_player((int)i);
  coop_inv_take((int)i,pl);
  if(coop_inv_snap[i].valid&&coop_inv_snap[i].client==coop_state.m[i].client)coop_inv_log("inventory-saved",(int)i,&coop_inv_snap[i].s,0xffffffffu);
 }
}
static void coop_inv_spawned(int slot,U32 client,int respawn,U32 now){
 if(slot<0||(U32)slot>=COOP_SESSION_MAX_MEMBERS)return;
 if(coop_inv_snap[slot].valid&&coop_inv_snap[slot].client!=client)coop_inv_snap[slot].valid=0;   /* another player now */
 if(!coop_keep_inventory||(respawn&&!coop_keep_on_death)||!coop_inv_snap[slot].valid){coop_inv_pending[slot]=0;return;}
 coop_inv_pending[slot]=1;coop_inv_due[slot]=now+COOP_INV_RESTORE_MS;
}
static void coop_inv_frame(U32 now){
 U32 i;
 if(!coop_keep_inventory)return;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  U32 pl,gives;
  if(!coop_inv_pending[i])continue;
  if(!coop_state.m[i].used||coop_inv_snap[i].client!=coop_state.m[i].client){coop_inv_pending[i]=0;continue;}
  if((int)(now-coop_inv_due[i])<0)continue;
  pl=coop_member_player((int)i);
  if(!pl||coop_player_dead(pl)){if(now-coop_inv_due[i]>15000u)coop_inv_pending[i]=0;continue;}
  coop_inv_pending[i]=0;
  gives=coop_inv_apply(pl,&coop_inv_snap[i].s);
  coop_inv_log("inventory-restored",(int)i,&coop_inv_snap[i].s,gives);
 }
 if(now-coop_inv_last_ms<COOP_INV_SNAPSHOT_MS)return;
 coop_inv_last_ms=now;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)
  if(coop_state.m[i].used&&coop_state.m[i].spawned&&coop_state.m[i].seen_epoch==coop_state.epoch)
   coop_inv_take((int)i,coop_member_player((int)i));
}

/* ---- More per-player chat commands ----
 *   !kill            kills the player who typed it (no cheat permission): the
 *                    damage entry vtable +0x50 with the record layout of the
 *                    damage brush 2f1c0 (amount, direction, position, damager
 *                    +0x1c/+0x20 = the player itself, type 0x400, +0x2c kill
 *                    invulnerable). Self damage passes CoopFriendlyFire.
 *   !all             (kpc_cheats 1) every armour piece, every weapon (any class)
 *                    and full ammo of each type (f3440 caps it).
 *   !class [name]    (kpc_cheats 1) next class, or starbearer|beastking|
 *                    celestial|demon (water/earth/air/fire, 1-4). Inventory
 *                    is kept (snapshot before, missing items given after). */
typedef void (__attribute__((thiscall)) *DamageFn)(void *,void *);
static int coop_kill_player(U32 player){
 U32 self=0,vt=0,fn=0,de=0,gp=0,rec[13],i;float pos[3]={0.f,0.f,0.f};
 if(!player||!coop_rd(player+8u,&self)||!self||!coop_rd(player,&vt)||!vt||!coop_rd(vt+0x50u,&fn)||!fn)return 0;
 if(coop_rd((U32)coop_image+0x16de80u,&de)&&de&&coop_rd(de+0x1d0u,&gp)&&gp)((ObjVecFn)gp)(self,pos);
 for(i=0;i<13u;i++)rec[i]=0;
 rec[0]=collision_bits(100000.f);
 rec[4]=collision_bits(pos[0]);rec[5]=collision_bits(pos[1]);rec[6]=collision_bits(pos[2]);
 rec[7]=rec[8]=self;rec[9]=0x400u;rec[10]=1u;rec[11]=1u;
 ((DamageFn)fn)((void *)player,rec);
 return 1;
}
static void coop_say(const char *a,const char *b,const char *c){
 CoopResult r;U32 m=0;
 coop_result(&r,COOP_ACT_ANNOUNCE,"");
 coop_text(r.msg,&m,a,sizeof r.msg);coop_text(r.msg,&m,b,sizeof r.msg);coop_text(r.msg,&m,c,sizeof r.msg);
 coop_log("cheat",r.msg);coop_execute(&r);
}
static void coop_give_all(U32 player){
 U32 id;
 for(id=COOP_INV_ARMOUR_FIRST;id<=COOP_INV_ARMOUR_LAST;id++)if(!coop_has_item(player,id))((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,1,1);
 for(id=COOP_INV_WEAPON_FIRST;id<=COOP_INV_WEAPON_LAST;id++)if(!coop_has_item(player,id))coop_give_weapon(player,id);
 for(id=COOP_INV_AMMO_FIRST;id<=COOP_INV_AMMO_LAST;id++)((GiveItemFn)(coop_image+0xf3440u))((void *)player,id,1000,1);
}
static void coop_change_class(U32 player,int cls){
 int slot=-1;U32 client=0;CoopInvState before;int have=0;
 if(coop_rd(player+0x120u,&client))slot=coop_find_client(&coop_state,client);
 have=coop_inv_read(player,&before);
 if(cls<0){U32 cur=0;coop_rd(player+0x264u,&cur);cls=(int)((cur+1u)&3u);}
 coop_set_class(player,(U32)cls);
 if(have){before.cls=(U32)cls;coop_inv_apply(player,&before);}
 if(slot>=0&&coop_inv_snap[slot].valid&&coop_inv_snap[slot].client==client)coop_inv_snap[slot].s.cls=(U32)cls;
}
static int coop_more_command(U32 player,const char *cmd,const char *who){
 int kill=coop_cheat_word(cmd,"kill"),all=coop_cheat_word(cmd,"all"),cls=coop_cheat_word(cmd,"class");
 if(!kill&&!all&&!cls)return 0;
 if(kill){
  if(coop_player_dead(player))return 1;
  if(coop_kill_player(player))coop_say("[Co-op] ",who," used !kill");
  return 1;
 }
 if(!coop_cheats_allowed()){coop_say("[Co-op] Cheats are disabled on this server (host console: kpc_cheats 1)","","");return 1;}
 if(all){coop_give_all(player);coop_say("[Co-op] ",who," used !all: every armour piece, weapon and full ammo");return 1;}
 {
  const char *a=cmd+5;int want=-1;U32 now_cls=0;
  while(*a==' '||*a=='\t')a++;
  if(*a){want=coop_class_parse(a);if(want<0){coop_say("[Co-op] Use: !class, or !class starbearer|beastking|celestial|demon","","");return 1;}}
  coop_change_class(player,want);
  coop_rd(player+0x264u,&now_cls);
  {CoopResult r;U32 m=0;coop_result(&r,COOP_ACT_ANNOUNCE,"");coop_text(r.msg,&m,"[Co-op] ",sizeof r.msg);coop_text(r.msg,&m,who,sizeof r.msg);
   coop_text(r.msg,&m," is now ",sizeof r.msg);coop_text(r.msg,&m,coop_class_name(now_cls),sizeof r.msg);
   coop_log("cheat",r.msg);coop_execute(&r);}
 }
 return 1;
}

/* ---- CoopCheatKeys (session, default 1): stock cheat keybinds per player ----
 * The client sends message 0x3b with a cheat id (cshell 0xc3680):
 * GimmieGimmieGimmie (action 98) 0x2f, CyclePlayerClass (97) 0x31, NextArmor
 * (100) 0x39. The server's handler fb910 (called at fead7 from the client
 * message switch, fb910(client, msg), caller pops 8) reads the id, then
 * checks the per-cheat flags at 0x1016d8a0: flag 1 = engine test
 * [g_pServerDE+0xec](client), flag 2 = refused in game types 1-4. All three
 * carry flags 3, so multiplayer refused them silently. Their handlers
 * (table 0x1016d98c: fa8f0, fab30, fb7b0; cdecl (client, float, byte, int))
 * already act on the sender's own player (f4370(client)). Around the call,
 * these three entries get flags & ~3 and Server Tools wrappers, which check
 * kpc_cheats (the permission of the stock Invuln/NoClip cheats), run the
 * stock handler and announce it; the table is restored right after. */
typedef void (CDECL *CheatMsgFn)(U32,U32);
typedef void (CDECL *CheatFn)(U32,U32,U32,U32);
typedef U32 (CDECL *ClientPlayerFn)(U32);
#define COOP_CHEAT_GIMMIE 0x2fu
#define COOP_CHEAT_CLASS 0x31u
#define COOP_CHEAT_ARMOR 0x39u
static U32 coop_stock_cheat[3],coop_cheat_denied_ms;
static void coop_key_cheat(U32 client,U32 k,U32 f,U32 b,U32 v){
 static const char *const what[3]={"gimmie gimmie gimmie","cycle player class","next armor"};
 U32 player=((ClientPlayerFn)(coop_image+0xf4370u))(client),now=GetTickCount(),before=0,after=0,id;
 char name[COOP_NAME_MAX];int mi=coop_find_client(&coop_state,client);
 name[0]=0;
 if(mi>=0){U32 q=0;coop_text(name,&q,coop_state.m[mi].name,sizeof name);}
 else if(player)coop_copy_str(name,player+0x1e8u,sizeof name);
 if(!coop_cheats_allowed()){
  if(!coop_cheat_denied_ms||now-coop_cheat_denied_ms>3000u){coop_cheat_denied_ms=now|1u;coop_say("[Co-op] Cheats are disabled on this server (host console: kpc_cheats 1)","","");}
  return;
 }
 if(!player||!coop_stock_cheat[k])return;
 if(k==2u)for(id=COOP_INV_ARMOUR_FIRST;id<=COOP_INV_ARMOUR_LAST;id++)if(coop_has_item(player,id))before|=1u<<(id-COOP_INV_ARMOUR_FIRST);
 if(k==1u){coop_change_class(player,-1);}
 else ((CheatFn)coop_stock_cheat[k])(client,f,b,v);
 {CoopResult r;U32 m=0;coop_result(&r,COOP_ACT_ANNOUNCE,"");
  coop_text(r.msg,&m,"[Co-op] ",sizeof r.msg);coop_text(r.msg,&m,name[0]?name:"?",sizeof r.msg);
  coop_text(r.msg,&m," used ",sizeof r.msg);coop_text(r.msg,&m,what[k],sizeof r.msg);
  if(k==0u)coop_text(r.msg,&m,": all weapons and ammo",sizeof r.msg);
  else if(k==1u){U32 c=0;coop_rd(player+0x264u,&c);coop_text(r.msg,&m,": now ",sizeof r.msg);coop_text(r.msg,&m,coop_class_name(c),sizeof r.msg);}
  else{
   char item[24];
   for(id=COOP_INV_ARMOUR_FIRST;id<=COOP_INV_ARMOUR_LAST;id++)if(coop_has_item(player,id))after|=1u<<(id-COOP_INV_ARMOUR_FIRST);
   after&=~before;
   for(id=COOP_INV_ARMOUR_FIRST;after&&id<=COOP_INV_ARMOUR_LAST;id++)if(after&(1u<<(id-COOP_INV_ARMOUR_FIRST)))break;
   coop_text(r.msg,&m,": ",sizeof r.msg);
   coop_text(r.msg,&m,after&&coop_item_name(id,item,sizeof item)?item:"all armour owned",sizeof r.msg);
  }
  coop_text(r.msg,&m," (key)",sizeof r.msg);
  coop_log("cheat",r.msg);coop_execute(&r);}
}
static void CDECL coop_key_gimmie(U32 c,U32 f,U32 b,U32 v){coop_key_cheat(c,0,f,b,v);}
static void CDECL coop_key_class(U32 c,U32 f,U32 b,U32 v){coop_key_cheat(c,1,f,b,v);}
static void CDECL coop_key_armor(U32 c,U32 f,U32 b,U32 v){coop_key_cheat(c,2,f,b,v);}
static void CDECL coop_cheat_msg(U32 client,U32 msg){
 static const U32 ids[3]={COOP_CHEAT_GIMMIE,COOP_CHEAT_CLASS,COOP_CHEAT_ARMOR};
 const U32 mine[3]={(U32)coop_key_gimmie,(U32)coop_key_class,(U32)coop_key_armor};
 U32 *flags=(U32 *)(coop_image+0x16d8a0u),*fns=(U32 *)(coop_image+0x16d98cu),sf[3],k;
 if(!bsp_mem_readable((U32)flags,0x100)||!bsp_mem_readable((U32)fns,0x100)){((CheatMsgFn)(coop_image+0xfb910u))(client,msg);return;}
 for(k=0;k<3u;k++){
  sf[k]=flags[ids[k]];coop_stock_cheat[k]=fns[ids[k]];
  flags[ids[k]]=sf[k]&~3u;fns[ids[k]]=mine[k];
 }
 ((CheatMsgFn)(coop_image+0xfb910u))(client,msg);
 for(k=0;k<3u;k++){flags[ids[k]]=sf[k];fns[ids[k]]=coop_stock_cheat[k];}
}

/* ---- CoopItemRespawnSec (session, default 15 s) ----
 * See coop_inventory.h (coop_item_respawnable). At the pickup's hide call
 * cc29e (cc570(item, 0), thiscall, ret 4) a respawnable item without the
 * authored Respawn flag gets Respawn +0xb4 = 1, Respawn Time +0xb8 = the
 * setting and +0x81 = 1 (constructor default), so cc570 hides it and starts
 * the stock countdown instead of removing it. Items with an expiry (+0xe4,
 * spawner drops) and every forced removal keep the stock behaviour. The
 * countdown's respawn call cbfff (cc080(item), thiscall) is held back while a
 * player stands on the item's spot: it retries every 0.5 s. */
typedef void (__attribute__((thiscall)) *ItemHideFn)(void *,U32);
typedef void (__attribute__((thiscall)) *ItemRespawnFn)(void *);
static U32 coop_respawn_epoch=0xffffffffu,coop_respawn_logged,coop_respawn_scheduled,coop_respawn_done,coop_respawn_waits;
static void coop_respawn_log(const char *event,U32 item){
 char line[120],name[24];U32 k=0,def=0,id=0;
 if(coop_respawn_epoch!=coop_state.epoch){coop_respawn_epoch=coop_state.epoch;coop_respawn_logged=0;}
 if(coop_respawn_logged>=24u)return;
 coop_respawn_logged++;
 name[0]=0;if(coop_rd(item+0x54u,&def)&&def&&coop_rd(def,&id))coop_item_name(id,name,sizeof name);
 coop_text(line,&k,event,sizeof line);coop_text(line,&k," item=",sizeof line);coop_text(line,&k,name[0]?name:"?",sizeof line);
 coop_log("item-respawn",line);
}
static void __attribute__((thiscall)) coop_item_hide(void *item,U32 force){
 U32 it=(U32)item,def=0,id=0;
 if(coop_item_respawn_sec&&!(force&0xffu)&&bsp_mem_readable(it+0x54u,0x94)&&!*(const U8 *)(it+0xb4u)&&!*(const U8 *)(it+0xe4u)&&
    coop_rd(it+0x54u,&def)&&def&&coop_rd(def,&id)&&coop_item_respawnable(id)){
  *(U8 *)(it+0xb4u)=1;*(float *)(it+0xb8u)=(float)(I32)coop_item_respawn_sec;*(U8 *)(it+0x81u)=1;
  coop_respawn_scheduled++;coop_respawn_log("scheduled",it);
 }
 ((ItemHideFn)(coop_image+0xcc570u))(item,force);
}
static void __attribute__((thiscall)) coop_item_respawn(void *item){
 U32 de=0,fn=0,self=0,live[64],n,i,count=0;float ip[3],pp[3*16];
 if(coop_rd((U32)coop_image+0x16de80u,&de)&&de&&coop_rd(de+0x1d0u,&fn)&&fn&&coop_rd((U32)item+8u,&self)&&self){
  ((ObjVecFn)fn)(self,ip);
  n=coop_live_objects(live,64);
  for(i=0;i<n&&count<16u;i++){
   U32 h=live[i];
   if(!((ToPlayerFn)(coop_image+0xee7d0u))(&h))continue;
   ((ObjVecFn)fn)(live[i],pp+3u*count);count++;
  }
  if(!coop_respawn_spot_clear(ip,pp,count)){
   *(float *)((U32)item+0x7cu)=COOP_RESPAWN_RETRY_SEC;
   if(!coop_respawn_waits++||!(coop_respawn_waits&63u))coop_respawn_log("waiting (a player is on the spot)",(U32)item);
   return;
  }
 }
 ((ItemRespawnFn)(coop_image+0xcc080u))(item);
 coop_respawn_done++;coop_respawn_log("respawned",(U32)item);
}

/* ---- CoopPowerupResend: send the activator's powerup effect to observers ----
 * Same message f6ff0 builds (object.lto message helper: 52140 create,
 * 522b0 WriteObject, 52190 WriteByte, 52350(client, id) send to one client
 * through [g_pServerDE+0xc4], 52160 release), addressed to each other member. */
typedef void (__attribute__((thiscall)) *SMsgCtorFn)(void *,U32);
typedef void (__attribute__((thiscall)) *SMsgArgFn)(void *,U32);
typedef void (__attribute__((thiscall)) *SMsgSendFn)(void *,U32,U32);
typedef void (__attribute__((thiscall)) *SMsgDtorFn)(void *);
static void coop_powerup_frame(U32 now){
 U32 obj=coop_powerup_obj,live[64],n,i,found=0,sent=0;
 if(!obj||(int)(now-coop_powerup_due)<0)return;
 coop_powerup_obj=0;
 n=coop_live_objects(live,64);
 for(i=0;i<n;i++)if(live[i]==obj){found=1;break;}
 if(!found)return;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  struct {U32 owner,msg;} m;
  if(!coop_state.m[i].used||!coop_state.m[i].client||coop_state.m[i].object==obj)continue;
  m.owner=0;m.msg=0;
  ((SMsgCtorFn)(coop_image+0x52140u))(&m,0);
  if(!m.msg)continue;
  ((SMsgArgFn)(coop_image+0x522b0u))(&m,obj);
  ((SMsgArgFn)(coop_image+0x52190u))(&m,0x35u);
  ((SMsgSendFn)(coop_image+0x52350u))(&m,coop_state.m[i].client,0x39u);
  ((SMsgDtorFn)(coop_image+0x52160u))(&m);
  sent++;
 }
 {char line[80];U32 k=0;coop_text(line,&k,"effect 0x35 re-sent to ",sizeof line);coop_dec(line,&k,sent,sizeof line);
  coop_text(line,&k," observer(s)",sizeof line);coop_log("powerup",line);}
}

/* ---- TraceScripts=1 (session diagnostic) ----
 * Script start c5340(name) refuses silently when the script is running
 * (c55b0) or was never loaded ("Tried to play script \"%s\" without loading
 * it first." through f8f70, which prints nothing in multiplayer). Logged:
 *   c4238  command list play (switches, triggers, script objects) -> c5340
 *   b8677  "scriptplay" event -> c5340
 *   c5358  c55b0 already-running check
 *   c53a3/c53c0  the muted not-loaded warning
 *   44dd6  CSwitch reached state 2 and plays its command
 * Bounded per world epoch; behaviour is unchanged. */
typedef U32 (CDECL *NameFn)(const char *);
#define COOP_SCRIPT_LOG_PER_EPOCH 120u
static U32 coop_script_log_epoch=0xffffffffu,coop_script_log_count;
static void coop_script_log(const char *what,const char *name,const char *suffix){
 char line[200],copy_name[0x80];U32 n=0;
 if(coop_script_log_epoch!=coop_state.epoch){coop_script_log_epoch=coop_state.epoch;coop_script_log_count=0;}
 if(coop_script_log_count>=COOP_SCRIPT_LOG_PER_EPOCH)return;
 coop_script_log_count++;
 copy_name[0]=0;if(name)coop_copy_str(copy_name,(U32)name,sizeof copy_name);
 coop_text(line,&n,what,sizeof line);coop_text(line,&n," ",sizeof line);
 coop_text(line,&n,copy_name[0]?copy_name:"-",sizeof line);
 if(suffix)coop_text(line,&n,suffix,sizeof line);
 coop_log("script",line);
}
static U32 CDECL coop_trace_command_play(const char *name){
 coop_script_log("command-play",name,0);
 return ((NameFn)(coop_image+0xc5340u))(name);
}
static U32 CDECL coop_trace_scriptplay(const char *name){
 coop_script_log("scriptplay",name,0);
 return ((NameFn)(coop_image+0xc5340u))(name);
}
static U32 CDECL coop_trace_running(const char *name){
 U32 r=((NameFn)(coop_image+0xc55b0u))(name);
 if((r&0xffu)==1u)coop_script_log("refused",name," (already running)");
 return r;
}
static void CDECL coop_trace_not_loaded(const char *fmt,const char *name){
 (void)fmt;coop_script_log("refused",name," (not loaded)");
}
static U32 __attribute__((thiscall)) coop_trace_switch(void *command){
 coop_script_log("switch-on",(const char *)command,0);
 return ((ThisNoArgFn)(coop_image+0xc4230u))(command);
}

static int campaign_trace;
static U8 *campaign_image;

/* Replace only the native six-byte game-type load at 2d455. Preserve the
 * receiver, original registers and flags, then replay the load before
 * returning. The handler 2d450 is a thiscall with one stack argument (pointer
 * to the activator HOBJECT); after our return address, pushfl and pushal it is
 * at 44(%esp). Nothing in 2d450 is pushed before 2d455. */
static void __attribute__((used,noinline)) campaign_exit_seen(U32 mode,U32 trigger,U32 activator_ptr){
 if(campaign_trace&&__sync_fetch_and_add(&campaign_log_count,1)<32)
  log_file(mode?"[KPCFix Campaign] exit handler reached: multiplayer gate closed":
                "[KPCFix Campaign] exit handler reached: single-player gate open");
 if(coop_session_enabled&&coop_image&&mode)coop_exit_trigger(trigger,activator_ptr);
}
static void __attribute__((naked)) campaign_exit_load(void){
 __asm__ volatile("pushfl; pushal; pushl 44(%esp); pushl %ecx; pushl 0x1d0(%eax); call campaign_exit_seen;"
                  "addl $12,%esp; popal; popfl; movl 0x1d0(%eax),%edx; ret");
}
typedef void *(CDECL *CampaignLookup)(const char *);
/* WorldProperties' property reader (1045b1) looks up its starting_cinematic
 * script with c50c0(path); in multiplayer it never plays it (104830 gates the
 * c5340 call on game type 0). Snapshot the name per epoch for CoopIntro. */
static void *CDECL campaign_lookup(const char *path){
 void *result=((CampaignLookup)(campaign_image+0xc50c0))(path);
 coop_intro_snap.valid=0;
 if(path&&result){
  U32 i;
  for(i=0;i+1<sizeof coop_intro_snap.name&&path[i];i++)coop_intro_snap.name[i]=path[i];
  coop_intro_snap.name[i]=0;
  coop_intro_snap.epoch=coop_state.epoch;coop_intro_snap.valid=i>0&&!path[i];
 }
 if(campaign_trace&&__sync_fetch_and_add(&campaign_log_count,1)<32){
  log_file("[KPCFix Campaign] starting_cinematic lookup");
  log_file(path);
  log_file(result?"[KPCFix Campaign] script object found":"[KPCFix Campaign] script lookup returned null");
 }
 return result;
}
/* Original script player (object.lto, all cdecl, one argument): c5340(name)
 * starts a loaded script ("Playing Script: %s") by adding it to the playing
 * list that the server Update advances every frame (ffba5 -> c5630);
 * c55b0(name) returns 1 while it is in that list. Cinematic helpers c5a10
 * (user input, 1 = on) and c59a0 (letterbox, 0 = off) broadcast MID_CINEMATIC
 * 0x3d to all clients, exactly as the script's own release events do. */
typedef void (CDECL *ScriptPlayFn)(const char *);
typedef U8 (CDECL *ScriptIsPlayingFn)(const char *);
typedef void (CDECL *CinematicFlagFn)(U32);
/* Script clock site ffba5 (push esi; call c5630; esi = the frame's dt as
 * float bits). c5630 is cdecl with one argument; the caller pops it.
 * Script object fields used by the trace (verified in c6260/c6380): +0x104
 * event count, +0x108 event array, +0x110 script time (float), +0x114 next
 * event index; event +4 = authored time (float). Playing list: count at
 * 15a0a4, array at 15a0a8; c5340 resets the script (c61c0) and appends it. */
typedef void (CDECL *ScriptUpdateFn)(U32);
static float coop_f(U32 bits){union{U32 b;float f;}v;v.b=bits;return v.f;}
static U32 coop_us_from_bits(U32 bits){
 float f=coop_f(bits);
 if(!(f>0.f))return 0;
 if(f>1.f)return 1000000u;
 return (U32)(I32)(f*1000000.f+0.5f);
}
static U32 coop_bits_from_us(U32 us){union{U32 b;float f;}v;v.f=(float)(I32)us*0.000001f;return v.b;}
static U32 coop_ms_from_float_bits(U32 bits){
 float f=coop_f(bits);
 if(!(f>0.f))return 0;
 if(f>100000.f)return 100000000u;
 return (U32)(I32)(f*1000.f+0.5f);
}
static int coop_script_playing(U32 script){
 U32 count,array,i,entry;
 if(!coop_rd((U32)coop_image+0x15a0a4u,&count)||!coop_rd((U32)coop_image+0x15a0a8u,&array)||count>256u)return 0;
 for(i=0;i<count;i++)if(coop_rd(array+i*4u,&entry)&&entry==script)return 1;
 return 0;
}
static void coop_clock_line(const char *event,U32 frames,U32 engine_us,U32 given_us,U32 wall_ms){
 char line[160];U32 n=0;
 coop_text(line,&n,"frames=",sizeof line);coop_dec(line,&n,frames,sizeof line);
 coop_text(line,&n," engine_ms=",sizeof line);coop_dec(line,&n,engine_us/1000u,sizeof line);
 coop_text(line,&n," script_ms=",sizeof line);coop_dec(line,&n,given_us/1000u,sizeof line);
 coop_text(line,&n," wall_ms=",sizeof line);coop_dec(line,&n,wall_ms,sizeof line);
 coop_text(line,&n,coop_script_clock?" governor=on":" governor=off",sizeof line);
 coop_log(event,line);
}
/* Log events the intro script dispatched since the last frame (bounded). */
static void coop_intro_trace_events(U32 now){
 U32 s=coop_intro_trace.script,count,array,next,time_bits,ev,at_bits;
 if(!coop_rd(s+0x104u,&count)||!coop_rd(s+0x108u,&array)||!coop_rd(s+0x110u,&time_bits)||
    !coop_rd(s+0x114u,&next)||count>4096u||next>count){coop_intro_trace.script=0;return;}
 while(coop_intro_trace.next<next&&coop_intro_trace.logged<COOP_INTRO_EVENT_LOG_MAX){
  char line[160];U32 n=0,i=coop_intro_trace.next++;
  if(!coop_rd(array+i*4u,&ev)||!coop_rd(ev+4u,&at_bits))at_bits=0;
  coop_text(line,&n,"i=",sizeof line);coop_dec(line,&n,i,sizeof line);
  coop_text(line,&n," at_ms=",sizeof line);coop_dec(line,&n,coop_ms_from_float_bits(at_bits),sizeof line);
  coop_text(line,&n," script_ms=",sizeof line);coop_dec(line,&n,coop_ms_from_float_bits(time_bits),sizeof line);
  coop_text(line,&n," wall_ms=",sizeof line);coop_dec(line,&n,now-coop_intro_trace.t0,sizeof line);
  coop_log("intro-event",line);coop_intro_trace.logged++;
 }
 coop_intro_trace.next=next;
}
static void CDECL coop_script_update(U32 dt_bits){
 U32 now=GetTickCount(),engine_us=coop_us_from_bits(dt_bits),given_us=engine_us,bits=dt_bits;
 int traced=0;
 if(coop_script_clock){
  given_us=coop_script_clock_step(&coop_clock,now,engine_us);
  if(given_us!=engine_us)bits=coop_bits_from_us(given_us);
 }
 if(coop_trace_timing&&coop_intro_trace.script)traced=coop_script_playing(coop_intro_trace.script);
 ((ScriptUpdateFn)(coop_image+0xc5630u))(bits);
 coop_clock_sum.frames++;coop_clock_sum.engine_us+=engine_us;coop_clock_sum.given_us+=given_us;
 if(!coop_trace_timing)return;
 if(traced)coop_intro_trace_events(now);
 if(!coop_clock_cal.done){
  if(!coop_clock_cal.started){
   coop_clock_cal.started=1;coop_clock_cal.t0=now;coop_clock_cal.frames0=coop_clock_sum.frames;
   coop_clock_cal.engine0=coop_clock_sum.engine_us;coop_clock_cal.given0=coop_clock_sum.given_us;
  }else if(now-coop_clock_cal.t0>=10000u){
   coop_clock_cal.done=1;
   coop_clock_line("clock-calibration",coop_clock_sum.frames-coop_clock_cal.frames0,
     coop_clock_sum.engine_us-coop_clock_cal.engine0,coop_clock_sum.given_us-coop_clock_cal.given0,now-coop_clock_cal.t0);
  }
 }
}
/* Sound-track timer site server.dll 2a1fc (see CoopSoundClock): thiscall
 * 29f0b(server manager, float dt), callee pops 4 bytes. */
typedef void (__attribute__((thiscall)) *SoundTracksFn)(void *,U32);
static void __attribute__((thiscall)) coop_sound_update(void *manager,U32 dt_bits){
 U32 engine_us=coop_us_from_bits(dt_bits),given_us=engine_us,bits=dt_bits;
 if(coop_sound_clock){
  given_us=coop_script_clock_step(&coop_sound,GetTickCount(),engine_us);
  if(given_us!=engine_us)bits=coop_bits_from_us(given_us);
 }
 coop_sound_sum.frames++;coop_sound_sum.engine_us+=engine_us;coop_sound_sum.given_us+=given_us;
 ((SoundTracksFn)((U8 *)backend+SOUND_TRACKS_TARGET_RVA))(manager,bits);
}
/* Install the call redirect at server.dll 2a1fc once, only on the verified
 * Europe-family image and only if the original bytes are present. Failure
 * leaves the engine original and is logged; it is never fatal. */
/* ---- CoopServerFrameMs (session, default 16): minimum server simulation step ----
 * server.dll 83b7bce8: the interface Update (0x2358c) calls the frame
 * function 0x2a05f(flags, time) at 0x235b0 (thiscall, ret 8, returns 1).
 * 0x2a05f computes the step itself from its own timestamp:
 *   [srv+0x214] = (time + [srv+0x1f8]) - [srv+0x218]; [srv+0x218] = time + [srv+0x1f8]
 * and clamps it to [0.01, 0.2] (0x2a19c-0x2a1d0). dedicated.exe calls it about
 * every 8 ms, so every server frame simulated 10 ms: game time ran ~1.25x real
 * time and physics/AI stepped at a 100 Hz-like 10 ms. Monster MoveTo stops each
 * think tick while the ground test fails (object.lto 0x97fac); with the short
 * step the Ballbuster in R2M2A stalls in the garage door (same as a KPCFix
 * single-player client at high FPS, which removes the client's 10 ms floor).
 * A call that comes less than CoopServerFrameMs after the last simulated frame
 * is skipped (returns 1, nothing runs); because the timestamp was not advanced,
 * the next frame simulates the whole elapsed time. Frames are then >= the
 * minimum, real time, never inflated. Network handling inside the frame runs
 * at the same reduced rate (62.5 Hz at 16 ms). */
#define SERVER_FRAME_CALL_RVA 0x235b0u
#define SERVER_FRAME_RVA 0x2a05fu
static const U8 original_frame_call[5]={0xe8,0xaa,0x6a,0x00,0x00};
static U32 coop_frame_ms,coop_frame_installed,coop_frame_skipped,coop_frame_run;
typedef U32 (__attribute__((thiscall)) *ServerFrameFn)(void *,U32,float);
static U32 __attribute__((thiscall)) coop_server_frame(void *server,U32 flags,float time){
 const U8 *srv=(const U8 *)server;float now,last,elapsed;
 now=time+*(const float *)(srv+0x1f8u);last=*(const float *)(srv+0x218u);elapsed=now-last;
 if(last>0.0f&&elapsed>=0.0f&&elapsed*1000.0f+0.5f<(float)coop_frame_ms){coop_frame_skipped++;return 1;}
 coop_frame_run++;
 return ((ServerFrameFn)((U8 *)backend+SERVER_FRAME_RVA))(server,flags,time);
}
static void coop_install_server_frame(const U8 back_sha[32]){
 U8 *site=(U8 *)backend+SERVER_FRAME_CALL_RVA,hooked[5];U32 rel,old,restore;
 if(coop_frame_installed||!coop_frame_ms)return;
 if(!same(back_sha,bsp_original_hash,32)){log_file("[KPCFix Co-op] CoopServerFrameMs unsupported for this original server.dll hash; server frames unchanged.");return;}
 if(!same(site,original_frame_call,5)){log_file("[KPCFix Co-op] CoopServerFrameMs: original bytes not found at server.dll 235b0; server frames unchanged.");return;}
 rel=(U32)coop_server_frame-((U32)site+5);hooked[0]=0xe8;copy(hooked+1,(const U8 *)&rel,4);
 if(!VirtualProtect(site,5,0x40,&old)){log_file("[KPCFix Co-op] CoopServerFrameMs: cannot unprotect server.dll 235b0; server frames unchanged.");return;}
 copy(site,hooked,5);
 if(!VirtualProtect(site,5,old,&restore)||!FlushInstructionCache(GetCurrentProcess(),site,5))
  fatal("[KPCFix Server Tools] ERROR: could not finalize the server frame hook.");
 coop_frame_installed=1;
 {char line[120];U32 n=0;coop_text(line,&n,"[KPCFix Co-op] server frame hook installed (server.dll 235b0 -> 2a05f), minimum step ms=",sizeof line);
  coop_dec(line,&n,coop_frame_ms,sizeof line);log_file(line);}
}
static void coop_install_sound_clock(const U8 back_sha[32]){
 U8 *site=(U8 *)backend+SOUND_TRACKS_CALL_RVA,hooked[5];U32 rel,old,restore;
 if(coop_sound_clock_installed)return;
 if(!same(back_sha,bsp_original_hash,32)){log_file("[KPCFix Co-op] CoopSoundClock unsupported for this original server.dll hash; sound timers unchanged.");return;}
 rel=(U32)coop_sound_update-((U32)site+5);hooked[0]=0xe8;copy(hooked+1,(const U8 *)&rel,4);
 if(!same(site,original_sound_call,5)){log_file("[KPCFix Co-op] CoopSoundClock: original bytes not found at server.dll 2a1fc; sound timers unchanged.");return;}
 if(!VirtualProtect(site,5,0x40,&old)){log_file("[KPCFix Co-op] CoopSoundClock: cannot unprotect server.dll 2a1fc; sound timers unchanged.");return;}
 copy(site,hooked,5);
 if(!VirtualProtect(site,5,old,&restore)||!FlushInstructionCache(GetCurrentProcess(),site,5))
  fatal("[KPCFix Server Tools] ERROR: could not finalize the sound clock hook.");
 coop_script_clock_reset(&coop_sound);coop_sound_clock_installed=1;
 log_file("[KPCFix Co-op] sound clock hook installed (server.dll 2a1fc -> 29f0b)");
}
static void coop_intro_trace_begin(void){
 U32 count,array,script;
 coop_intro_trace.active=0;coop_intro_trace.script=0;
 if(!coop_trace_timing)return;
 coop_intro_trace.active=1;coop_intro_trace.next=0;coop_intro_trace.logged=0;
 coop_intro_trace.t0=GetTickCount();coop_intro_trace.frames0=coop_clock_sum.frames;
 coop_intro_trace.engine0=coop_clock_sum.engine_us;coop_intro_trace.given0=coop_clock_sum.given_us;
 coop_intro_trace.sframes0=coop_sound_sum.frames;coop_intro_trace.sengine0=coop_sound_sum.engine_us;coop_intro_trace.sgiven0=coop_sound_sum.given_us;
 if(coop_rd((U32)coop_image+0x15a0a4u,&count)&&count&&count<=256u&&
    coop_rd((U32)coop_image+0x15a0a8u,&array)&&coop_rd(array+(count-1u)*4u,&script))coop_intro_trace.script=script;
 else coop_log("intro-trace","script object not found; event lines unavailable");
}
static void coop_intro_trace_end(void){
 if(!coop_intro_trace.active)return;
 coop_clock_line("intro-clock",coop_clock_sum.frames-coop_intro_trace.frames0,
   coop_clock_sum.engine_us-coop_intro_trace.engine0,coop_clock_sum.given_us-coop_intro_trace.given0,
   GetTickCount()-coop_intro_trace.t0);
 if(coop_sound_clock_installed){
  char line[160];U32 n=0;
  coop_text(line,&n,"frames=",sizeof line);coop_dec(line,&n,coop_sound_sum.frames-coop_intro_trace.sframes0,sizeof line);
  coop_text(line,&n," engine_ms=",sizeof line);coop_dec(line,&n,(coop_sound_sum.engine_us-coop_intro_trace.sengine0)/1000u,sizeof line);
  coop_text(line,&n," sound_ms=",sizeof line);coop_dec(line,&n,(coop_sound_sum.given_us-coop_intro_trace.sgiven0)/1000u,sizeof line);
  coop_text(line,&n," wall_ms=",sizeof line);coop_dec(line,&n,GetTickCount()-coop_intro_trace.t0,sizeof line);
  coop_text(line,&n,coop_sound_clock?" governor=on":" governor=off",sizeof line);
  coop_log("sound-clock",line);
 }
 coop_intro_trace.script=0;coop_intro_trace.active=0;
}
static void coop_intro_frame(U32 now){
 CoopResult r;int act=coop_session_intro_tick(&coop_state,now,&r);
 if(act==COOP_ACT_INTRO_START){
  int playing;
  coop_log("intro-start",coop_intro_snap.name);
  coop_sb_pump(now);   /* close the start scoreboard before the cinematic */
  runtime_broadcast(coop_image,r.msg);
  ((ScriptPlayFn)(coop_image+0xc5340u))(coop_intro_snap.name);
  playing=((ScriptIsPlayingFn)(coop_image+0xc55b0u))(coop_intro_snap.name)!=0;
  if(!playing){
   coop_session_intro_done(&coop_state,0,&r);
   coop_log("intro-failed","script did not enter the playing list");
   runtime_broadcast(coop_image,r.msg);
  }else{coop_log("intro-playing",r.msg);coop_intro_trace_begin();}
 }else if(act==COOP_ACT_INTRO_ABORT){
  ((CinematicFlagFn)(coop_image+0xc5a10u))(1u);
  ((CinematicFlagFn)(coop_image+0xc59a0u))(0u);
  coop_log("intro-abort","overran; user_input on and letterbox off broadcast");
  coop_intro_trace_end();
  runtime_broadcast(coop_image,r.msg);
 }else if(coop_state.phase==COOP_PHASE_INTRO_RUNNING&&
          !((ScriptIsPlayingFn)(coop_image+0xc55b0u))(coop_intro_snap.name)){
  coop_session_intro_done(&coop_state,1,&r);
  coop_log("intro-finished",coop_intro_snap.name);
  coop_intro_trace_end();
  runtime_broadcast(coop_image,r.msg);
 }
}

/* WorldProperties (object.lto 104940-104950) splits the world's music property
 * into tokens and calls the music parser bad50(label, argc, argv) (cdecl,
 * caller pops 12 bytes). bad50 builds the music event (vtable 11d240) and
 * broadcasts MID_MUSIC_LOAD 0x4d / LOAD_STRING 0x4a. WorldProperties then runs
 * the event's vtbl+8 = baa90, which broadcasts [GROOVE 0x52] [TRANSITION 0x51]
 * PLAY 0x4e or READY 0x4f (0x4e + intensity set; string forms 0x4b/0x4c)
 * [INTENSITY 0x50] [FADEOUT 0x53]. All with hClient NULL, once, at world
 * initialisation. The hook forwards unchanged, snapshots the event's fields
 * for per-player delivery (CoopMusic) and optionally logs (TraceMusic). */
typedef void *(CDECL *MusicEventFn)(const char *,U32,U32);
static void *CDECL coop_music_event(const char *label,U32 argc,U32 argv){
 void *result=((MusicEventFn)(coop_image+0xbad50u))(label,argc,argv);
 U32 ev=(U32)result;
 coop_music_snap.valid=0;
 if(ev&&bsp_mem_readable(ev+0x108u,0x85u)){
  U32 i;const char *src=(const char *)(ev+0x108u);
  for(i=0;i+1<sizeof coop_music_snap.song&&src[i];i++){char c=src[i];coop_music_snap.song[i]=c=='\\'?'/':c;}
  coop_music_snap.song[i]=0;
  coop_music_snap.has_song=coop_music_snap.song[0]!=0;
  coop_music_snap.play=*(const U8 *)(ev+0x188u)!=0;
  coop_music_snap.intensity=*(const U8 *)(ev+0x189u);
  coop_music_snap.groove=*(const U8 *)(ev+0x18au);
  coop_music_snap.transition=*(const U8 *)(ev+0x18bu);
  coop_music_snap.fadeout=*(const U8 *)(ev+0x18cu);
  coop_music_snap.epoch=coop_state.epoch;
  coop_music_snap.valid=coop_music_snap.has_song;
 }
 if(coop_trace_music){
  char line[220],token[40];U32 n=0,i,p;
  coop_text(line,&n,"argc=",sizeof line);coop_dec(line,&n,argc,sizeof line);
  for(i=0;i<argc&&i<6;i++){
   if(!coop_rd(argv+i*4u,&p)||!p)break;
   coop_copy_str(token,p,sizeof token);
   coop_text(line,&n," ",sizeof line);coop_text(line,&n,token,sizeof line);
  }
  coop_text(line,&n,result?" event=created":" event=none",sizeof line);
  if(coop_music_snap.valid){
   coop_text(line,&n," song=",sizeof line);coop_text(line,&n,coop_music_snap.song,sizeof line);
   coop_text(line,&n,coop_music_snap.play?" play=1":" play=0",sizeof line);
   coop_text(line,&n," intensity=",sizeof line);coop_dec(line,&n,coop_music_snap.intensity,sizeof line);
   coop_text(line,&n," transition=",sizeof line);coop_dec(line,&n,coop_music_snap.transition,sizeof line);
  }
  coop_log("music-world",line);
 }
 return result;
}
/* Per-frame tick: the server shell Update (ffb70) calls 100720 (pending
 * NextLevel check) every frame at ffbbf with ECX = shell. Forward unchanged,
 * then deliver due per-player music starts. Installed only with CoopMusic. */
static void __attribute__((thiscall)) coop_frame(void *shell){
 ((ShellFn)(coop_image+0x100720u))(shell);
 if(coop_session_enabled){coop_inv_frame(GetTickCount());coop_powerup_frame(GetTickCount());}
 if(coop_session_enabled&&coop_music)coop_music_frame(GetTickCount());
 if(coop_session_enabled&&coop_intro)coop_intro_frame(GetTickCount());
 if(coop_session_enabled&&coop_start_scoreboard){
  U32 now=GetTickCount(),sh;
  if(coop_sb_take_change(&coop_sb,&coop_state,now)&&coop_rd((U32)coop_image+COOP_SHELL_RVA,&sh)&&sh)
   coop_set_pending_next_level(sh);
  coop_sb_pump(now);
 }
}
/* [Coop] CoopHealthBars (default 1 in a session): the original enemy health
 * bar manager in multiplayer. Evidence (object.lto 3a03f879, cshell 703813a4):
 *  - f8fe0 returns 1 only when the game type [shell+0x1d0] is 0 (single
 *    player). Its only four callers are the bar manager: Update ea6d0 (ea6d2,
 *    each frame from ffbab), OnDamage ea7c0 (ea7c3, 69 AI/monster call sites),
 *    Remove ea810/eaa10 (eaa10, on death) and Send eab10 (eab11, only from
 *    Update ea783). In multiplayer every path returns at its gate, so no bar
 *    data is ever built or sent: a missing show-event, not missing health data.
 *  - Send broadcasts MID_MONSTER_HEALTH_BAR_DATA 0x56 to all clients
 *    (vtbl+0xc4, hClient 0, guaranteed) with the global 10-entry table
 *    (101642d8): style, monster key, name, health/max from the server's own
 *    values. The stock client handles 0x56 (cshell cb998 -> b8c80) with no
 *    multiplayer test; the bar draw (b8dc0 via cdb48) is gated only by its UI
 *    state, so the original client UI and scale are used unchanged.
 * The four calls are redirected to session predicates (1 = proceed as in
 * single player). Send is limited to 10 Hz: in single player it runs once per
 * frame, which on a dedicated server (up to 125 frames/s) would be a
 * guaranteed broadcast storm. Player health uses other messages and is not
 * touched. CoopHealthBars=0 leaves all four sites original. */
static int coop_health_bars=1;
static U32 coop_bars_last_send,coop_bars_sends,coop_bars_damage_epoch=0xffffffffu;
static U32 CDECL coop_bars_gate(void){return 1;}
static U32 CDECL coop_bars_damage_gate(void){
 if(coop_bars_damage_epoch!=coop_state.epoch){
  coop_bars_damage_epoch=coop_state.epoch;
  coop_log("health-bar","first monster damage in this epoch; original bar data enabled");
 }
 return 1;
}
#define COOP_BARS_SEND_MS 100u
static U32 CDECL coop_bars_send_gate(void){
 U32 now=GetTickCount();
 if(coop_bars_sends&&now-coop_bars_last_send<COOP_BARS_SEND_MS)return 0;
 coop_bars_last_send=now;coop_bars_sends++;return 1;
}
/* Optional in-memory call/load redirects inside the original object image.
 * Each site's original bytes are verified; disabled sites must stay original. */
typedef struct {U32 rva,size;U8 original[6],hooked[6];int enabled;} OptionalSite;
static void make_call(OptionalSite *s,U8 *b,const void *target){
 U32 i,rel=(U32)target-((U32)b+s->rva+5);
 s->hooked[0]=0xe8;copy(s->hooked+1,(const U8 *)&rel,4);
 for(i=5;i<s->size;i++)s->hooked[i]=0x90;
}
#define OPTIONAL_SITES 47u
static void optional_sites(OptionalSite *s,U8 *b){
 static const struct {U32 rva,size;U8 original[6];} table[OPTIONAL_SITES]={
  {SPAWN_POS_CALL_RVA,6,{0xff,0x91,0xe0,0x01,0x00,0x00}},
  {SPAWN_PREPARE_CALL_RVA,5,{0xe8,0x17,0xfd,0xff,0xff}},
  {0x2d455,6,{0x8b,0x90,0xd0,0x01,0x00,0x00}},
  {0x1045b1,5,{0xe8,0x0a,0x0b,0xfc,0xff}},
  {0xfeec4,6,{0x81,0xc6,0xe8,0x01,0x00,0x00}},   /* chat relay: add esi,0x1e8 */
  {0xf167a,5,{0xe8,0x41,0xfa,0xff,0xff}},        /* second spawn path telefrag */
  {0x100744,5,{0xe8,0x77,0xfe,0xff,0xff}},       /* pending change -> NextLevel */
  {0xfde97,5,{0xe8,0x24,0x27,0x00,0x00}},        /* console nextlevel -> NextLevel */
  {0x10494b,5,{0xe8,0x00,0x64,0xfb,0xff}},       /* WorldProperties -> music event bad50 */
  {0xffbbf,5,{0xe8,0x5c,0x0b,0x00,0x00}},        /* shell Update -> 100720 (per frame) */
  {0xffba5,5,{0xe8,0x86,0x5a,0xfc,0xff}},        /* shell Update -> script clock c5630 */
  {0xea6d2,5,{0xe8,0x09,0xe9,0x00,0x00}},        /* bar Update -> single-player test f8fe0 */
  {0xea7c3,5,{0xe8,0x18,0xe8,0x00,0x00}},        /* bar OnDamage -> f8fe0 */
  {0xeaa10,5,{0xe8,0xcb,0xe5,0x00,0x00}},        /* bar Remove -> f8fe0 */
  {0xeab11,5,{0xe8,0xca,0xe4,0x00,0x00}},        /* bar Send -> f8fe0 */
  {0x93c5d,2,{0x0c,0x10}},                       /* CMonsterSpawner: or al,0x10 (forced MP-off) */
  {0x93c65,5,{0xe8,0x96,0x53,0x06,0x00}},        /* CMonsterSpawner -> skill/game-type filter f9000 */
  {0xf12be,5,{0xe8,0xad,0xfd,0xff,0xff}},        /* spawn path 1 -> multiplayer start strategy f1070 */
  {0xf15f2,5,{0xe8,0xa9,0xf2,0xff,0xff}},        /* spawn path 2 -> start lookup f08a0 */
  {0xc4238,5,{0xe8,0x03,0x11,0x00,0x00}},        /* command list play -> script start c5340 */
  {0xb8677,5,{0xe8,0xc4,0xcc,0x00,0x00}},        /* scriptplay event -> c5340 */
  {0xc5358,5,{0xe8,0x53,0x02,0x00,0x00}},        /* c5340 -> already-running check c55b0 */
  {0xc53a3,5,{0xe8,0xc8,0x3b,0x03,0x00}},        /* c5340 -> muted "not loaded" print f8f70 */
  {0xc53c0,5,{0xe8,0xab,0x3b,0x03,0x00}},        /* c5340 -> muted "not loaded" print f8f70 */
  {0x44dd6,5,{0xe8,0x55,0xf4,0x07,0x00}},        /* CSwitch state 2 -> command play c4230 */
  {0xc1a89,6,{0x8b,0x88,0xd0,0x01,0x00,0x00}},   /* player event: game-type gate */
  {0xc1abe,5,{0xe8,0xdd,0x32,0x03,0x00}},        /* player event -> f4da0 (first player only) */
  {0xc28a5,6,{0x8b,0x90,0xd0,0x01,0x00,0x00}},   /* disable_ai: game-type gate */
  {0xc269b,6,{0x8b,0x88,0xd0,0x01,0x00,0x00}},   /* wake_monster_group: game-type gate */
  {0xc2a75,6,{0x8b,0x90,0xd0,0x01,0x00,0x00}},   /* disable targetting: game-type gate */
  {0xb8ad8,6,{0x8b,0x88,0xd0,0x01,0x00,0x00}},   /* cheatscript: game-type gate */
  {0x2e205,5,{0xe8,0xf6,0x06,0x0c,0x00}},        /* CTriggerCommand filter -> IsPlayer ee900 */
  {0x2ffea,5,{0xe8,0x11,0xe9,0x0b,0x00}},        /* trigger touch -> IsPlayer ee900 */
  {0x3005a,5,{0xe8,0xa1,0xe8,0x0b,0x00}},        /* trigger touch -> IsPlayer ee900 */
  {0xf3c08,6,{0x39,0xb8,0xf0,0x01,0x00,0x00}},   /* player damage: teamplay test */
  {0xcc145,5,{0xe8,0x36,0x00,0x00,0x00}},        /* pickup -> cc180(item, player) */
  {0x30c9e,5,{0xe8,0xed,0x80,0x0c,0x00}},        /* secret found -> message to the finder f8d90 */
  {0xfdfdb,6,{0xff,0x90,0xf0,0x00,0x00,0x00}},   /* client enter -> attach object [de+0xf0] */
  {0xc8348,6,{0x8b,0x88,0xd0,0x01,0x00,0x00}},   /* armour touch: game-type gate (give at touch in MP) */
  {0x409bb,5,{0xe8,0x70,0x38,0x08,0x00}},        /* CModelDeco use -> command list play c4230 */
  {0xb676f,5,{0xe8,0xcc,0xcc,0x03,0x00}},        /* inventory add (first player) -> f3440 */
  {0xc2c9c,5,{0xe8,0x7f,0xcc,0x02,0x00}},        /* camera_path: first player's view position ef920 */
  {0xc2c56,5,{0xe8,0xf5,0xcc,0x02,0x00}},        /* camera_reset: first player's view reset ef950 */
  {0xd51fb,6,{0xff,0x90,0xd0,0x01,0x00,0x00}},   /* spawner update: GetObjectPos of the reference +0x170 */
  {0xfead7,5,{0xe8,0x34,0xce,0xff,0xff}},        /* client message 0x3b -> cheat handler fb910 */
  {0xcc29e,5,{0xe8,0xcd,0x02,0x00,0x00}},        /* pickup taken -> hide/remove cc570(item, 0) */
  {0xcbfff,5,{0xe8,0x7c,0x00,0x00,0x00}},        /* item respawn countdown done -> cc080 */
 };
 U32 i;
 for(i=0;i<OPTIONAL_SITES;i++){s[i].rva=table[i].rva;s[i].size=table[i].size;copy(s[i].original,table[i].original,6);}
 make_call(&s[0],b,(const void *)trace_spawn_position);
 make_call(&s[1],b,(const void *)coop_before_telefrag);
 make_call(&s[2],b,(const void *)campaign_exit_load);
 make_call(&s[3],b,(const void *)campaign_lookup);
 make_call(&s[4],b,(const void *)coop_chat_entry);
 make_call(&s[5],b,(const void *)coop_respawn_telefrag);
 make_call(&s[6],b,(const void *)coop_next_level);
 make_call(&s[7],b,(const void *)coop_next_level);
 make_call(&s[8],b,(const void *)coop_music_event);
 make_call(&s[9],b,(const void *)coop_frame);
 make_call(&s[10],b,(const void *)coop_script_update);
 make_call(&s[11],b,(const void *)coop_bars_gate);
 make_call(&s[12],b,(const void *)coop_bars_damage_gate);
 make_call(&s[13],b,(const void *)coop_bars_gate);
 make_call(&s[14],b,(const void *)coop_bars_send_gate);
 s[15].hooked[0]=0x90;s[15].hooked[1]=0x90;   /* pass the authored MiscFlags */
 make_call(&s[16],b,(const void *)coop_spawner_filter);
 make_call(&s[17],b,(const void *)coop_mp_start_strategy);
 make_call(&s[18],b,(const void *)coop_find_start);
 make_call(&s[19],b,(const void *)coop_trace_command_play);
 make_call(&s[20],b,(const void *)coop_trace_scriptplay);
 make_call(&s[21],b,(const void *)coop_trace_running);
 make_call(&s[22],b,(const void *)coop_trace_not_loaded);
 make_call(&s[23],b,(const void *)coop_trace_not_loaded);
 make_call(&s[24],b,(const void *)coop_trace_switch);
 make_call(&s[25],b,(const void *)coop_gate_zero_ecx);
 make_call(&s[26],b,(const void *)coop_player_event_all);
 make_call(&s[27],b,(const void *)coop_gate_zero_edx);
 make_call(&s[28],b,(const void *)coop_gate_zero_ecx);
 make_call(&s[29],b,(const void *)coop_gate_zero_edx);
 make_call(&s[30],b,(const void *)coop_gate_zero_ecx);
 make_call(&s[31],b,(const void *)coop_trigger_filter_entry);
 make_call(&s[32],b,(const void *)coop_record_activator);
 make_call(&s[33],b,(const void *)coop_record_activator);
 make_call(&s[34],b,(const void *)coop_ff_entry);
 make_call(&s[35],b,(const void *)coop_item_pickup);
 make_call(&s[36],b,(const void *)coop_secret_message);
 make_call(&s[37],b,(const void *)coop_client_enter_place);
 make_call(&s[38],b,(const void *)coop_gate_zero_ecx);
 make_call(&s[39],b,(const void *)coop_deco_use_entry);
 make_call(&s[40],b,(const void *)coop_inventory_give);
 make_call(&s[41],b,(const void *)coop_view_set_all);
 make_call(&s[42],b,(const void *)coop_view_reset_all);
 make_call(&s[43],b,(const void *)coop_spawner_pos_entry);
 make_call(&s[44],b,(const void *)coop_cheat_msg);
 make_call(&s[45],b,(const void *)coop_item_hide);
 make_call(&s[46],b,(const void *)coop_item_respawn);
 s[0].enabled=spawn_trace_enabled;
 s[1].enabled=spawn_separation_enabled||coop_session_enabled;
 s[2].enabled=campaign_trace||coop_session_enabled;
 s[3].enabled=campaign_trace||(coop_session_enabled&&coop_intro);
 for(i=4;i<8;i++)s[i].enabled=coop_session_enabled;
 s[8].enabled=coop_session_enabled&&(coop_trace_music||coop_music);
 s[9].enabled=coop_session_enabled;   /* music, intro, scoreboard, inventory restore, powerup re-send */
 s[10].enabled=coop_session_enabled&&(coop_script_clock||coop_trace_timing);
 for(i=11;i<15;i++)s[i].enabled=coop_session_enabled&&coop_health_bars;
 s[15].enabled=coop_session_enabled&&coop_spawners;
 s[16].enabled=coop_session_enabled&&coop_trace_spawners;
 s[17].enabled=s[18].enabled=coop_session_enabled&&coop_sp_start;
 for(i=19;i<25;i++)s[i].enabled=coop_session_enabled&&coop_trace_scripts;
 for(i=25;i<34;i++)s[i].enabled=coop_session_enabled&&coop_script_events;
 s[34].enabled=coop_session_enabled&&!coop_friendly_fire;
 s[35].enabled=s[36].enabled=coop_session_enabled&&coop_shared_items;
 s[37].enabled=coop_session_enabled&&coop_sp_start;
 s[38].enabled=coop_session_enabled&&coop_script_events;
 s[39].enabled=coop_session_enabled;
 s[40].enabled=coop_session_enabled&&coop_shared_items;
 s[41].enabled=s[42].enabled=coop_session_enabled;
 s[43].enabled=coop_session_enabled&&coop_spawner_nearest;
 s[44].enabled=coop_session_enabled&&coop_cheat_keys;
 s[45].enabled=s[46].enabled=coop_session_enabled&&coop_item_respawn_sec;
 coop_ff_skip_target=(U32)b+0xf3f1eu;
 coop_c4230_target=(U32)b+0xc4230u;
}
static int apply_object(HMODULE module){
 U8 *b=(U8 *)module;U8 hash[32],expected[8];U32 i,j,old,restore,already=1;
 U32 mbi[7],delta=(U32)b-0x10000000u;OptionalSite site[OPTIONAL_SITES];
 if(!module_hash(module,hash)||!same(hash,object_hash,32)||!valid_image(b,0x196000))return 0;
 optional_sites(site,b);
 for(i=0;i<PATCH_COUNT;i++)if(!same(b+patches[i].rva,patches[i].after,patches[i].size))already=0;
 if(already){
  for(i=0;i<OPTIONAL_SITES;i++)
   if(!same(b+site[i].rva,site[i].enabled?site[i].hooked:site[i].original,site[i].size))return 0;
  if(b[HOOK_RVA+INITIAL_OFFSET]>1||!same(b+0x116550,noclip_init,sizeof(noclip_init)))return 0;
  for(i=0;i<sizeof(hook_code);i++)if(i!=INITIAL_OFFSET&&b[HOOK_RVA+i]!=hook_code[i])return 0;
  return 1;
 }
 for(i=0;i<PATCH_COUNT;i++){
  copy(expected,patches[i].before,patches[i].size);
  if(patches[i].reloc>=0)*(U32 *)(expected+patches[i].reloc)+=delta;
  if(!same(b+patches[i].rva,expected,patches[i].size))return 0;
 }
 for(i=0;i<OPTIONAL_SITES;i++)if(!same(b+site[i].rva,site[i].original,site[i].size))return 0;
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
 campaign_image=b;
 if(campaign_trace)log_file("[KPCFix Campaign] startup and exit trace armed");
 copy(b+HOOK_RVA,hook_code,sizeof(hook_code));
 b[HOOK_RVA+INITIAL_OFFSET]=(U8)initial_cheats;
 copy(b+0x116550,noclip_init,sizeof(noclip_init));
 for(i=0;i<PATCH_COUNT;i++)for(j=0;j<patches[i].size;j++)b[patches[i].rva+j]=patches[i].after[j];
 if(spawn_trace_enabled||spawn_separation_enabled||coop_session_enabled){
  spawn_image=b;clearance_started=0;spawn_trace_count=0;collision_trace_count=0;
  for(i=0;i<COOP_TRIAL_SLOTS;i++)spawn_trial_reservations.player[i]=0;
 }
 if(coop_session_enabled){
  coop_image=b;coop_session_init(&coop_state,coop_min_players,coop_max_players);coop_t0=GetTickCount();
  coop_script_clock_reset(&coop_clock);coop_sb_reset(&coop_sb);
  if(coop_campaign)coop_session_enable_campaign(&coop_state);
  {U32 k;for(k=0;coop_lobby_world[k]&&k+1<COOP_WORLD_MAX;k++)coop_state.lobby_world[k]=coop_lobby_world[k];coop_state.lobby_world[k]=0;}
  coop_state.test_label=coop_start_label;coop_resolve_test_start(&coop_state);
  coop_state.intro_settle_ms=coop_intro_settle_ms;
  coop_state.skill=coop_skill;
  coop_apply_skill();
 }
 for(i=0;i<OPTIONAL_SITES;i++)if(site[i].enabled)copy(b+site[i].rva,site[i].hooked,site[i].size);
 /* LoadLibrary has already applied relocations and may have run table init. */
 *(U32 *)(b+0x16d950)&=~3u;
 if(!VirtualProtect(b+0x1000,0x116000,old,&restore)||
    !FlushInstructionCache(GetCurrentProcess(),b+0x1000,0x116000))
  fatal("[KPCFix Server Tools] ERROR: could not finalize runtime hooks.");
 object_installs++;
 {const char *message="[KPCFix Server Tools] Runtime hooks installed; original object.lto unchanged.";runtime_print(b,message,len(message));}
 log_file("[KPCFix Server Tools] Runtime object hooks installed; file unchanged.");
 if(coop_session_enabled){
  char msg[96];U32 n=0;
  coop_text(msg,&n,"session armed kpc-coop/1 route=R1M2B>R1M2C>R1M3A min=",sizeof msg);
  coop_dec(msg,&n,coop_min_players,sizeof msg);coop_text(msg,&n," max=",sizeof msg);
  coop_dec(msg,&n,coop_max_players,sizeof msg);
  coop_log("armed",msg);
 }
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
 campaign_trace=active&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceCampaign",0,ini_path)!=0;
 initial_cheats=GetPrivateProfileIntW((const WCHAR *)L"ServerTools",(const WCHAR *)L"AllowCheats",0,ini_path)!=0;
 spawn_separation_enabled=active&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TrialSeparateSpawns",0,ini_path)!=0;
 collision_trace_enabled=active&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceCollisionABI",0,ini_path)!=0;
 bsp_volume_enabled=active&&same(back_sha,bsp_original_hash,32)&&
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceBspVolume",0,ini_path)!=0;
 if(active&&!same(back_sha,bsp_original_hash,32)&&
    GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceBspVolume",0,ini_path)!=0)
   log_file("[KPCFix Co-op] BSP trace unsupported for this original server.dll hash; disabled.");
 model_registry_enabled=active&&same(back_sha,bsp_original_hash,32)&&
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceModelRegistry",0,ini_path)!=0;
 if(active&&!same(back_sha,bsp_original_hash,32)&&
    GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceModelRegistry",0,ini_path)!=0)
   log_file("[KPCFix Co-op] Model registry trace unsupported for this original server.dll hash; disabled.");
 model_state_enabled=model_registry_enabled&&
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceModelState",0,ini_path)!=0;
 clearance_enabled=active&&(bsp_volume_enabled||model_registry_enabled||
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TracePlayerClearance",0,ini_path)!=0);
 spawn_trace_enabled=active&&(clearance_enabled||spawn_separation_enabled||collision_trace_enabled||
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceSpawns",0,ini_path)!=0);
 coop_session_enabled=active&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSession",0,ini_path)!=0;
 coop_min_players=GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopMinPlayers",2,ini_path);
 coop_max_players=GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopMaxPlayers",3,ini_path);
 if(coop_max_players<1||coop_max_players>3)coop_max_players=3;
 if(coop_min_players<1||coop_min_players>coop_max_players)coop_min_players=coop_max_players<2?coop_max_players:2;
 coop_trace_timing=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceTiming",0,ini_path)!=0;
 coop_intermission=GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopIntermission",0,ini_path)!=0;
 coop_trace_music=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceMusic",0,ini_path)!=0;
 coop_music=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopMusic",1,ini_path)!=0;
 coop_intro=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopIntro",0,ini_path)!=0;
 coop_script_clock=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopScriptClock",coop_intro,ini_path)!=0;
 coop_campaign=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopCampaign",0,ini_path)!=0;
 /* Campaign loads show the original loading picture on KPCFix clients
  * (client [Multiplayer] CampaignLoadingArt), so the start scoreboard is off
  * by default there; the MVP route keeps it on. */
 coop_start_scoreboard=coop_session_enabled&&!coop_intermission&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopStartScoreboard",coop_campaign?0:1,ini_path)!=0;
 /* Intro settle after the last spawn. KPCFix clients now hold their spawn
  * until the loading picture's minimum has passed (CampaignLoadingHoldSpawn),
  * so campaign mode starts the intro almost at once. */
 coop_intro_settle_ms=GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopIntroSettleMs",coop_campaign?COOP_CAMPAIGN_SETTLE_MS:COOP_INTRO_SETTLE_MS,ini_path);
 if(coop_intro_settle_ms>30000u)coop_intro_settle_ms=30000u;
 coop_spawners=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSpawners",1,ini_path)!=0;
 coop_trace_spawners=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceSpawners",0,ini_path)!=0;
 coop_sp_start=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSinglePlayerStart",1,ini_path)!=0;
 coop_trace_scripts=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceScripts",0,ini_path)!=0;
 coop_script_events=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopScriptEvents",1,ini_path)!=0;
 coop_friendly_fire=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopFriendlyFire",0,ini_path)!=0;
 coop_shared_items=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSharedItems",1,ini_path)!=0;
 coop_host_sequences=coop_session_enabled&&coop_script_events&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopHostSequences",1,ini_path)!=0;
 coop_spawner_nearest=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSpawnerNearest",1,ini_path)!=0;
 coop_cheat_keys=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopCheatKeys",1,ini_path)!=0;
 coop_keep_inventory=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopKeepInventory",1,ini_path)!=0;
 coop_keep_on_death=coop_keep_inventory&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopKeepOnDeath",1,ini_path)!=0;
 coop_powerup_resend=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopPowerupResend",1,ini_path)!=0;
 coop_item_respawn_sec=coop_session_enabled?GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopItemRespawnSec",15,ini_path):0;
 if(coop_item_respawn_sec>600u)coop_item_respawn_sec=600u;
 coop_spread=coop_session_enabled&&same(back_sha,bsp_original_hash,32)&&
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSpawnSpread",1,ini_path)!=0;
 coop_spread_bsp=coop_spread;
 coop_skill=-1;
 if(coop_session_enabled){U32 v=GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSkill",0xffffffffu,ini_path);if(v<=4u)coop_skill=(I32)v;}
 coop_health_bars=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopHealthBars",1,ini_path)!=0;
 coop_sound_clock=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopSoundClock",coop_script_clock,ini_path)!=0;
 coop_frame_ms=coop_session_enabled?GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopServerFrameMs",16,ini_path):0;
 if(coop_frame_ms>50u)coop_frame_ms=50u;
 /* Test-only start world: accepted only if it names a route world exactly
  * (case-insensitive ASCII); anything else is ignored (start stays R1M2B). */
 coop_start_label=0;coop_start_state=0;
 /* Campaign: every world plays its starting_cinematic, as in single player.
  * The intro scripts also preload the level's other scripts ("scriptload"
  * lines, e.g. R1M1A's r1m1a_bigdoor.scr for the lever), so skipping them
  * leaves those scripts refused as "not loaded". */
 coop_intro_all=coop_session_enabled&&GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopIntroAll",coop_campaign,ini_path)!=0;
 coop_lobby_world[0]=0;
 if(coop_session_enabled){
  WCHAR w[0x80];U32 wn=GetPrivateProfileStringW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopLobbyWorld",(const WCHAR *)L"",w,0x80,ini_path),k;
  char a[0x80];int ascii=wn>0&&wn<0x7f;
  for(k=0;ascii&&k<wn;k++){if(w[k]<33||w[k]>126)ascii=0;a[k]=(char)w[k];}
  if(ascii){a[wn]=0;if(!coop_world_normalize(a,coop_lobby_world))coop_lobby_world[0]=0;}
 }
 if(coop_session_enabled){
  WCHAR w[16];U32 wn=GetPrivateProfileStringW((const WCHAR *)L"Coop",(const WCHAR *)L"CoopTestStartWorld",(const WCHAR *)L"",w,16,ini_path),k,pos;
  if(wn>0&&wn<15){
   char a[16];int ascii=1;
   for(k=0;k<wn;k++){if(w[k]<32||w[k]>126)ascii=0;a[k]=(char)coop_lower((char)w[k]);}
   a[wn]=0;coop_start_state=2;
   {const CoopWorld *table=coop_campaign?coop_campaign_worlds:coop_mvp_worlds;
    U32 len=coop_campaign?COOP_CAMPAIGN_LEN:COOP_MVP_LEN;
    for(pos=0;ascii&&pos<len;pos++){
     const char *label=table[pos].label;int same_label=1;
     for(k=0;label[k]||a[k];k++)if(coop_lower(label[k])!=a[k]){same_label=0;break;}
     if(same_label){coop_start_label=label;coop_start_state=1;break;}
    }}
  }else if(wn)coop_start_state=2;
 }
 if(active&&(spawn_trace_enabled||coop_session_enabled||campaign_trace)){
  /* Diagnostic identity tied to the exact bytes of this DLL file. */
  U8 own[32];WCHAR own_path[1024];U32 k,n=GetModuleFileNameW(self,own_path,1024);
  char line[128];static const char hex[]="0123456789abcdef";U32 m=0;
  coop_text(line,&m,"[KPCFix Co-op] build=coop-session-v2 proto=kpc-coop/1 sha256=",sizeof line);
  if(n&&n<1024&&file_hash(own_path,own))for(k=0;k<32&&m+3<sizeof line;k++){line[m++]=hex[own[k]>>4];line[m++]=hex[own[k]&15];}
  else coop_text(line,&m,"unavailable",sizeof line);
  line[m]=0;log_file(line);
  log_file(coop_session_enabled?"[KPCFix Co-op] effective CoopSession=1":"[KPCFix Co-op] effective CoopSession=0");
  if(coop_trace_timing)log_file("[KPCFix Co-op] effective TraceTiming=1 (t= ms since session armed)");
  if(coop_session_enabled)log_file(coop_intermission?
    "[KPCFix Co-op] effective CoopIntermission=1 (native scoreboard; continues on a player's fire/use)":
    "[KPCFix Co-op] effective CoopIntermission=0 (world change without intermission scoreboard)");
  if(coop_trace_music)log_file("[KPCFix Co-op] effective TraceMusic=1");
  if(coop_session_enabled)log_file(coop_music?
    "[KPCFix Co-op] effective CoopMusic=1 (per-player world music start after spawn)":
    "[KPCFix Co-op] effective CoopMusic=0");
  if(coop_session_enabled)log_file(coop_start_scoreboard?
    "[KPCFix Co-op] effective CoopStartScoreboard=1 (stock scoreboard from !start until the intro begins)":
    "[KPCFix Co-op] effective CoopStartScoreboard=0");
  if(coop_session_enabled)log_file(coop_spawners?
    "[KPCFix Co-op] effective CoopSpawners=1 (authored monster spawner flags)":
    "[KPCFix Co-op] effective CoopSpawners=0 (stock: monster spawners off in multiplayer)");
  if(coop_trace_spawners)log_file("[KPCFix Co-op] effective TraceSpawners=1");
  if(coop_session_enabled){
   char line[120];U32 m=0;coop_text(line,&m,"[KPCFix Co-op] effective CoopServerFrameMs=",sizeof line);coop_dec(line,&m,coop_frame_ms,sizeof line);
   coop_text(line,&m,coop_frame_ms?" (minimum server simulation step; shorter frames carried)":" (stock 8 ms frames simulated as 10 ms)",sizeof line);log_file(line);
  }
  if(coop_session_enabled)log_file(coop_sp_start?
    "[KPCFix Co-op] effective CoopSinglePlayerStart=1 (players start at the single-player start)":
    "[KPCFix Co-op] effective CoopSinglePlayerStart=0 (stock: multiplayer starts when the world has them)");
  if(coop_trace_scripts)log_file("[KPCFix Co-op] effective TraceScripts=1 (script starts, refusals, switch commands)");
  if(coop_session_enabled)log_file(coop_spread?
    "[KPCFix Co-op] effective CoopSpawnSpread=1 (a taken start moves the next player to a checked spot: clear line, floor, body)":
    "[KPCFix Co-op] effective CoopSpawnSpread=0 (players share the start point)");
  if(coop_session_enabled)log_file(coop_shared_items?
    "[KPCFix Co-op] effective CoopSharedItems=1 (weapons, armour, pamphlets, keys and tickets go to every player)":
    "[KPCFix Co-op] effective CoopSharedItems=0 (stock: pickups only for whoever takes them)");
  if(coop_session_enabled)log_file(coop_host_sequences?
    "[KPCFix Co-op] effective CoopHostSequences=1 (only the host can start a level-ending sequence)":
    "[KPCFix Co-op] effective CoopHostSequences=0 (any player can start a level-ending sequence)");
  if(coop_session_enabled){
   char line[200];U32 m=0;
   log_file(coop_spawner_nearest?
     "[KPCFix Co-op] effective CoopSpawnerNearest=1 (monster spawner radius measured from the nearest player)":
     "[KPCFix Co-op] effective CoopSpawnerNearest=0 (stock: from the first player)");
   log_file(coop_cheat_keys?
     "[KPCFix Co-op] effective CoopCheatKeys=1 (GimmieGimmieGimmie/CyclePlayerClass/NextArmor keys per player with kpc_cheats 1)":
     "[KPCFix Co-op] effective CoopCheatKeys=0 (stock: those keys do nothing in multiplayer)");
   log_file(coop_keep_inventory?(coop_keep_on_death?
     "[KPCFix Co-op] effective CoopKeepInventory=1 CoopKeepOnDeath=1 (weapons, armour, shards, ammo and class kept through world changes, lobby and deaths)":
     "[KPCFix Co-op] effective CoopKeepInventory=1 CoopKeepOnDeath=0 (kept through world changes and lobby; a death loses them)"):
     "[KPCFix Co-op] effective CoopKeepInventory=0 (stock: fresh inventory after every world change)");
   coop_text(line,&m,"[KPCFix Co-op] effective CoopItemRespawnSec=",sizeof line);coop_dec(line,&m,coop_item_respawn_sec,sizeof line);
   coop_text(line,&m,coop_item_respawn_sec?" (taken weapons, ammo, shards and power-ups come back when the spot is clear)":" (stock: taken items stay gone)",sizeof line);
   log_file(line);
   log_file(coop_powerup_resend?"[KPCFix Co-op] effective CoopPowerupResend=1":"[KPCFix Co-op] effective CoopPowerupResend=0");
  }
  if(coop_session_enabled)log_file(coop_friendly_fire?
    "[KPCFix Co-op] effective CoopFriendlyFire=1 (players can hurt each other)":
    "[KPCFix Co-op] effective CoopFriendlyFire=0 (no damage between players; self and monster damage unchanged)");
  if(coop_session_enabled)log_file(coop_script_events?
    "[KPCFix Co-op] effective CoopScriptEvents=1 (player/disable_ai/wake_monster_group/targetting/cheatscript script events run; player commands apply to every player)":
    "[KPCFix Co-op] effective CoopScriptEvents=0 (stock: those script events do nothing in multiplayer)");
  if(coop_session_enabled){
   char line[80];U32 m=0;
   coop_text(line,&m,"[KPCFix Co-op] effective CoopSkill=",sizeof line);
   if(coop_skill<0)coop_text(line,&m,"unchanged",sizeof line);else coop_dec(line,&m,(U32)coop_skill,sizeof line);
   log_file(line);
  }
  if(coop_session_enabled)log_file(coop_health_bars?
    "[KPCFix Co-op] effective CoopHealthBars=1 (original enemy health bars, 10 Hz broadcast)":
    "[KPCFix Co-op] effective CoopHealthBars=0");
  if(coop_intro)log_file("[KPCFix Co-op] effective CoopIntro=1 (original starting_cinematic after all players are in; verified worlds, or every world with CoopIntroAll)");
  if(coop_session_enabled&&(coop_intro||coop_script_clock))log_file(coop_script_clock?
    "[KPCFix Co-op] effective CoopScriptClock=1 (script clock limited to wall-clock time)":
    "[KPCFix Co-op] effective CoopScriptClock=0 (script clock = engine frame time)");
  if(coop_session_enabled&&(coop_script_clock||coop_sound_clock))log_file(coop_sound_clock?
    "[KPCFix Co-op] effective CoopSoundClock=1 (server sound timers limited to wall-clock time)":
    "[KPCFix Co-op] effective CoopSoundClock=0 (sound timers = engine frame time)");
  if(coop_start_state==1){
   char line[96];U32 m=0;coop_text(line,&m,"[KPCFix Co-op] effective CoopTestStartWorld=",sizeof line);
   coop_text(line,&m,coop_start_label,sizeof line);coop_text(line,&m," (test start)",sizeof line);log_file(line);
  }else if(coop_start_state==2)log_file(coop_campaign?"[KPCFix Co-op] CoopTestStartWorld ignored: not a WORLDORDER world":
                                        "[KPCFix Co-op] CoopTestStartWorld ignored: not a route world (R1M2B, R1M2C, R1M3A)");
  if(coop_session_enabled){
   char line[200];U32 m=0;
   coop_text(line,&m,coop_campaign?"[KPCFix Co-op] effective CoopCampaign=1 (WORLDORDER graph, 48 worlds; default selection full campaign from R1M0)":
                                   "[KPCFix Co-op] effective CoopCampaign=0 (MVP route R1M2B>R1M2C>R1M3A)",sizeof line);log_file(line);
   if(coop_lobby_world[0]){m=0;coop_text(line,&m,"[KPCFix Co-op] effective CoopLobbyWorld=",sizeof line);coop_text(line,&m,coop_lobby_world,sizeof line);log_file(line);}
   if(coop_intro&&coop_intro_settle_ms!=COOP_INTRO_SETTLE_MS){m=0;coop_text(line,&m,"[KPCFix Co-op] effective CoopIntroSettleMs=",sizeof line);coop_dec(line,&m,coop_intro_settle_ms,sizeof line);log_file(line);}
   if(coop_intro_all)log_file("[KPCFix Co-op] effective CoopIntroAll=1 (every route world's starting_cinematic plays)");
  }
  log_file(model_registry_enabled?
    "[KPCFix Co-op] effective TraceModelRegistry=1":
    "[KPCFix Co-op] effective TraceModelRegistry=0 (set [Coop] TraceModelRegistry=1; restart)");
  log_file(model_state_enabled?
    "[KPCFix Co-op] effective TraceModelState=1":
    "[KPCFix Co-op] effective TraceModelState=0 (requires both TraceModelState=1 and TraceModelRegistry=1)");
 }
 if(active&&!hooked){
  if(!intercept_backend())fatal("[KPCFix Server Tools] ERROR: could not install engine loading hook.");
  hooked=1;
 }
 if(active&&coop_sound_clock)coop_install_sound_clock(back_sha);
 if(active&&coop_frame_ms)coop_install_server_frame(back_sha);
 log_file(active?"[KPCFix Server Tools] Dedicated DLL active.":"[KPCFix Server Tools] Original engine pass-through.");
}
I32 CDECL CreateServer(U32 version,const char *name,void **out){
 initialize();return create_real(version,name,out);
}
void CDECL DeleteServer(void *server){if(delete_real)delete_real(server);}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void *reserved){
 (void)reserved;if(reason==1){self=module;DisableThreadLibraryCalls(module);}return 1;
}
