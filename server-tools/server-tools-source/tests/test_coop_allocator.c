typedef unsigned int U32;
#include "coop_allocator.h"
#include <assert.h>

typedef struct { int checked; int mode; } ProbeCtx;
static CoopProbeResult known_good(void *p,const CoopVec3 *v,CoopVec3 *verified){
 ProbeCtx *ctx=(ProbeCtx *)p;ctx->checked++;
 if(ctx->mode==0)return COOP_PROBE_UNSUPPORTED;
 if(ctx->mode==2)return COOP_PROBE_BLOCKED;
 if(ctx->mode==3){
  *verified=*v;verified->x=2000000.f;
  return COOP_PROBE_SAFE; /* faulty/invalid engine response must fail */
 }
 if(ctx->mode==4){
  *verified=*v;verified->x=1600.f;verified->z=-2944.f;
  return COOP_PROBE_SAFE; /* floor adjustment onto an occupied slot */
 }
 /* Synthetic map with known clear corridor, not an engine simulation. */
 if(v->x==1600.f&&v->y==-256.f&&
    (v->z==-2944.f||v->z==-2848.f||v->z==-3040.f)){
  *verified=*v;
  if(ctx->mode==5)verified->y=-240.f; /* grounded/floor-snapped */
  return COOP_PROBE_SAFE;
 }
 return COOP_PROBE_BLOCKED;
}
static int same(CoopVec3 a,CoopVec3 b){
 return a.x==b.x&&a.y==b.y&&a.z==b.z;
}
int main(void){
 CoopAllocator a;
 CoopVec3 home={1600.f,-256.f,-2944.f},out,first,second,third;
 ProbeCtx probe={0,1};U32 cand=0;
 coop_allocator_reset(&a,1);
 assert(a.world_generation==1);
 assert(coop_allocator_count(&a)==0);
 assert(coop_allocator_assign(&a,0,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_BAD_INPUT);
 assert(coop_allocator_assign(&a,123,&home,0,&probe,&out,&cand)==COOP_ALLOC_BAD_INPUT);
 assert(coop_allocator_assign(&a,123,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 assert(cand==0&&same(out,home));first=out;
 assert(coop_allocator_assign(&a,456,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 assert(cand==1&&out.z==-2848.f);second=out;
 assert(coop_allocator_assign(&a,789,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 assert(cand==2&&out.z==-3040.f);third=out;
 assert(!same(first,second)&&!same(first,third)&&!same(second,third));
 assert(coop_allocator_count(&a)==3);
 assert(coop_allocator_find(&a,456)>=0);
 probe.checked=0;
 assert(coop_allocator_assign(&a,456,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 assert(same(out,second)&&cand==COOP_ALLOCATOR_CANDIDATES&&probe.checked==0);
 /* Fourth player must fail instead of teleporting onto an occupied start. */
 assert(coop_allocator_assign(&a,999,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_NO_SAFE_POSITION);
 assert(coop_allocator_count(&a)==3);
 /* A disconnect event releases its own reservation, not other players. */
 assert(coop_allocator_release(&a,456)==1);
 assert(coop_allocator_release(&a,456)==0);
 assert(coop_allocator_count(&a)==2);
 assert(coop_allocator_assign(&a,999,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 assert(same(out,second));
 assert(coop_allocator_count(&a)==3);
 /* Unknown and blocked probes can NEVER manufacture a safe placement. */
 coop_allocator_reset(&a,2);
 probe.mode=0;
 assert(coop_allocator_assign(&a,123,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_NO_SAFE_POSITION);
 assert(coop_allocator_count(&a)==0);
 probe.mode=2;
 assert(coop_allocator_assign(&a,123,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_NO_SAFE_POSITION);
 assert(coop_allocator_count(&a)==0);
 /* An invalid callback response cannot allocate a position. */
 coop_allocator_reset(&a,3);
 probe.mode=3;
 assert(coop_allocator_assign(&a,123,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_NO_SAFE_POSITION);
 assert(coop_allocator_count(&a)==0);
 /* A snapped candidate must be tested again against existing reservations. */
 coop_allocator_reset(&a,4);
 probe.mode=1;
 assert(coop_allocator_assign(&a,123,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 probe.mode=4;
 assert(coop_allocator_assign(&a,456,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_NO_SAFE_POSITION);
 assert(coop_allocator_count(&a)==1);
 /* Safe adjusted floor level is propagated to the caller. */
 coop_allocator_reset(&a,5);
 probe.mode=5;
 assert(coop_allocator_assign(&a,123,&home,known_good,&probe,&out,&cand)==COOP_ALLOC_SUCCESS);
 assert(out.y==-240.f);
 /* NaN and out-of-range input anchors fail closed before probing. */
 {
  CoopVec3 invalid={1600.f,1000000000.f,-2944.f};
  assert(coop_allocator_assign(&a,456,&invalid,known_good,&probe,&out,&cand)==COOP_ALLOC_BAD_INPUT);
 }
 probe.mode=1;
 /* Candidate generation is deterministic and bounded. */
 assert(coop_candidate(&home,0).z==home.z);
 assert(coop_candidate(&home,1).z==home.z+96.f);
 assert(coop_candidate(&home,2).z==home.z-96.f);
 assert(coop_candidate(&home,9).z==home.z+192.f);
 assert(coop_candidate(&home,COOP_ALLOCATOR_CANDIDATES-1).x==home.x-192.f);
 assert(coop_allocator_count(0)==0);
 return 0;
}
