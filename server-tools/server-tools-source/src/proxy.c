#include "winmini.h"
#include "sha256.h"
#include "runtime_data.h"
#include "coop_spawn.h"
#include "coop_intersect_abi.h"
#include "coop_volume_probe.h"
#include "coop_bsp_box.h"
#include "coop_model_registry.h"
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

static int clearance_enabled,bsp_volume_enabled,model_registry_enabled;
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
static void registry_trace_loaded(void){
 const U32 global_ptr=(U32)backend+0x4a250u;
 U32 manager=0,state=0,n=0;
 CoopModelRegistrySnapshot snap={0};
 CoopModelRegistryStatus verdict=COOP_MODEL_REGISTRY_UNKNOWN;
 char msg[116];
 if(model_registry_enabled&&registry_read32(0,global_ptr,&manager)&&
    registry_read32(0,manager+0x19cu,&state))
  verdict=coop_model_registry_scan(state,registry_read32,0,&snap);
 trace_append(msg,&n,"[KPCFix Co-op] model registry=");
 trace_append(msg,&n,verdict==COOP_MODEL_REGISTRY_TRAVERSED?"TRAVERSED":"UNKNOWN");
 trace_append(msg,&n," entries=");trace_hex(msg,&n,snap.entries);
 trace_append(msg,&n," roots=");trace_hex(msg,&n,snap.root_candidates);
 trace_append(msg,&n," attached=");trace_hex(msg,&n,snap.associated_candidates);
 msg[n]=0;log_file(msg);
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
static void sample_candidate(KpcIntersectFn intersect,KpcIntersectVec center,KpcIntersectVec lo,KpcIntersectVec hi){
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
  sample_candidate(intersect,candidate,lo,hi);
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
   sample_candidate(intersect,candidate,lo,hi);
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
  spawn_image=b;clearance_started=0;spawn_trace_count=0;collision_trace_count=0;
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
 clearance_enabled=active&&(bsp_volume_enabled||model_registry_enabled||
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TracePlayerClearance",0,ini_path)!=0);
 spawn_trace_enabled=active&&(clearance_enabled||spawn_separation_enabled||collision_trace_enabled||
   GetPrivateProfileIntW((const WCHAR *)L"Coop",(const WCHAR *)L"TraceSpawns",0,ini_path)!=0);
 if(active&&spawn_trace_enabled){
  log_file("[KPCFix Co-op] diagnostics build=registry-status-v2 (reads KPCServerTools.ini)");
  log_file(model_registry_enabled?
    "[KPCFix Co-op] effective TraceModelRegistry=1":
    "[KPCFix Co-op] effective TraceModelRegistry=0 (set [Coop] TraceModelRegistry=1; restart)");
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
