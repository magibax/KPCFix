#include "winmini.h"
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

static int clearance_enabled,bsp_volume_enabled,model_registry_enabled,model_state_enabled;
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
 if(!backend||!bsp_volume_enabled||
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
static int coop_prepare_maplist(U32 target){
 U32 shell,count,i,mbi[7];char name[0x80];
 if(target>=COOP_ROUTE_LEN||!coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)||
    !coop_rd(shell+0x1ae0u,&count)||!count||count>64)return 0;
 for(i=0;i<count;i++){
  coop_copy_str(name,shell+0x210u+i*0x7fu,0x7f);
  if(coop_world_equal(name,coop_route[target])){
   U32 *cursor=(U32 *)(coop_image+COOP_MAPCURSOR_RVA);
   if(VirtualQuery(cursor,mbi,sizeof(mbi))!=sizeof(mbi)||mbi[4]!=0x1000||!(mbi[5]&0xccu)||(mbi[5]&0x100))return 0;
   *cursor=i?i-1:count-1;   /* fc640 pre-increments and wraps */
   return 1;
  }
 }
 return 0;
}
static void coop_current_world(char out[0x80]){
 U32 shell,count,cursor;out[0]=0;
 if(coop_rd((U32)coop_image+COOP_SHELL_RVA,&shell)&&coop_rd(shell+0x1ae0u,&count)&&
    coop_rd((U32)coop_image+COOP_MAPCURSOR_RVA,&cursor)&&count&&count<=64&&cursor<count)
  coop_copy_str(out,shell+0x210u+cursor*0x7fu,0x7f);
}
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
   /* Default co-op path: set the shell's native pending-NextLevel flag +0x1ae9,
    * which the server's own Update (ffb70 -> 100720) consumes on the next frame
    * through the hooked NextLevel site 100744. This is the native post-
    * intermission path without the scoreboard broadcast or the click wait,
    * the same client flow as a console "nextlevel". Refused if a native
    * intermission is already active (+0x1ae8), so no state is left half set. */
   if(!bsp_mem_readable(shell+0x1ae8u,2)||*(const U8 *)(shell+0x1ae8u)){
    CoopResult failed;coop_session_change_failed(&coop_state,&failed);
    coop_log("change-failed","native intermission already active or shell flags unreadable");
    runtime_broadcast(coop_image,failed.msg);return;
   }
   *(U8 *)(shell+0x1ae9u)=1;
   coop_log("end-level","native pending NextLevel set (no intermission scoreboard)");
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
/* Both original spawn paths: register/refresh the member, place on R1M2B,
 * and SKIP the native telefrag loop (co-op players are teammates). */
static void coop_spawn_hook(U32 player,U32 *pos,int first_path){
 U32 client=0,object=0,now=GetTickCount();char name[COOP_NAME_MAX],line[160];U32 n=0;
 int slot=-1;CoopResult r;
 coop_tick(now);coop_presence(now);
 if(coop_identify(player,&client,&object,name)){
  slot=coop_session_seen(&coop_state,client,player,object,name,now,&r);
  coop_execute(&r);
  if(coop_music)coop_session_music_schedule(&coop_state,slot,now);
 }
 if(first_path&&slot>=0&&slot<(int)COOP_TRIAL_SLOTS&&coop_is_r1m2b_anchor(pos))
  coop_assign_trial_position(pos,slot);
 coop_text(line,&n,first_path?"path=f13a4 slot=":"path=f167a slot=",sizeof line);
 if(slot<0)coop_text(line,&n,"none",sizeof line);else coop_dec(line,&n,(U32)slot,sizeof line);
 coop_text(line,&n," client=",sizeof line);coop_dec(line,&n,client,sizeof line);
 coop_text(line,&n," name=",sizeof line);coop_text(line,&n,name,sizeof line);
 if(pos){coop_text(line,&n," zbits=",sizeof line);coop_dec(line,&n,pos[2],sizeof line);}
 coop_text(line,&n," telefrag=skipped",sizeof line);
 coop_log("spawn",line);
}
static void __attribute__((used,noinline)) coop_chat_observe(U32 player,U32 text){
 U32 client=0,object=0,now;char name[COOP_NAME_MAX],said[COOP_COMMAND_MAX+2];CoopResult r;U32 i=0;
 if(!coop_session_enabled||!coop_image||!text)return;
 coop_copy_str(said,text,sizeof said);
 while(said[i]==' '||said[i]=='\t')i++;
 if(said[i]!='!')return;
 now=GetTickCount();coop_tick(now);coop_presence(now);
 if(!coop_identify(player,&client,&object,name))return;
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
 coop_session_exit(&coop_state,next,now,&r);
 if(r.action==COOP_ACT_CHANGE_WORLD)coop_log("exit",line);
 else if(logged&&r.action==COOP_ACT_REPLY)coop_log("exit-refused",r.msg);
 coop_execute(&r);
}
typedef void (__attribute__((thiscall)) *SpawnPrepareFn)(void *,U32 *,const U32 *,const U32 *);
static void __attribute__((thiscall)) coop_next_level(void *shell){
 char world[0x80];U32 now=GetTickCount();
 if(coop_session_enabled){coop_session_world_loading(&coop_state,now);coop_log("world-loading","native NextLevel");}
 ((ShellFn)(coop_image+COOP_NEXT_LEVEL_RVA))(shell);
 if(coop_session_enabled){
  coop_current_world(world);
  coop_session_world_loaded(&coop_state,world);
  coop_log("world-loaded",world[0]?world:"(unknown)");
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
static void *CDECL campaign_lookup(const char *path){
 void *result=((CampaignLookup)(campaign_image+0xc50c0))(path);
 if(__sync_fetch_and_add(&campaign_log_count,1)<32){
  log_file("[KPCFix Campaign] starting_cinematic lookup");
  log_file(path);
  log_file(result?"[KPCFix Campaign] script object found":"[KPCFix Campaign] script lookup returned null");
 }
 return result;
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
 if(coop_session_enabled&&coop_music)coop_music_frame(GetTickCount());
}
/* Optional in-memory call/load redirects inside the original object image.
 * Each site's original bytes are verified; disabled sites must stay original. */
typedef struct {U32 rva,size;U8 original[6],hooked[6];int enabled;} OptionalSite;
static void make_call(OptionalSite *s,U8 *b,const void *target){
 U32 i,rel=(U32)target-((U32)b+s->rva+5);
 s->hooked[0]=0xe8;copy(s->hooked+1,(const U8 *)&rel,4);
 for(i=5;i<s->size;i++)s->hooked[i]=0x90;
}
#define OPTIONAL_SITES 10u
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
 s[0].enabled=spawn_trace_enabled;
 s[1].enabled=spawn_separation_enabled||coop_session_enabled;
 s[2].enabled=campaign_trace||coop_session_enabled;
 s[3].enabled=campaign_trace;
 for(i=4;i<8;i++)s[i].enabled=coop_session_enabled;
 s[8].enabled=coop_session_enabled&&(coop_trace_music||coop_music);
 s[9].enabled=coop_session_enabled&&coop_music;
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
 log_file(active?"[KPCFix Server Tools] Dedicated DLL active.":"[KPCFix Server Tools] Original engine pass-through.");
}
I32 CDECL CreateServer(U32 version,const char *name,void **out){
 initialize();return create_real(version,name,out);
}
void CDECL DeleteServer(void *server){if(delete_real)delete_real(server);}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void *reserved){
 (void)reserved;if(reason==1){self=module;DisableThreadLibraryCalls(module);}return 1;
}
