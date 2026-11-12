typedef unsigned int U32;
#include "coop_volume_probe.h"
#include <assert.h>
static CoopProbeResult adapter(void *ctx,const CoopVec3 *p,CoopVec3 *out){
 CoopProbeResult r=coop_volume_assess(p,(const CoopVolumeObservation *)ctx);
 if(r==COOP_PROBE_SAFE)*out=*p;
 return r;
}
int main(void){
 CoopVec3 p={1600.f,-256.f,-2944.f},out;
 CoopVolumeObservation o={0};
 CoopAllocator a;
 o.bounds_min=(CoopVec3){-20.f,-46.f,-20.f};
 o.bounds_max=(CoopVec3){20.f,46.f,20.f};
 o.ground_hit=1;o.ground_point=(CoopVec3){1600.f,-320.f,-2944.f};
 o.ground_normal=(CoopVec3){0.f,1.f,0.f};
 o.world_hull=COOP_VOLUME_UNKNOWN;o.other_actors=COOP_VOLUME_UNKNOWN;
 coop_allocator_reset(&a,1);
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_UNSUPPORTED);
 assert(coop_allocator_assign(&a,123,&p,adapter,&o,&out,0)==COOP_ALLOC_NO_SAFE_POSITION);
 assert(coop_allocator_count(&a)==0);
 o.sampled_body_hit=1;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_BLOCKED);
 o.sampled_body_hit=0;
 o.world_hull=COOP_VOLUME_BLOCKED;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_BLOCKED);
 o.world_hull=COOP_VOLUME_VERIFIED_CLEAR;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_UNSUPPORTED);
 o.other_actors=COOP_VOLUME_BLOCKED;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_BLOCKED);
 o.other_actors=COOP_VOLUME_VERIFIED_CLEAR;
 /* Synthetic native results only: real-world API still unverified. */
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_SAFE);
 assert(coop_allocator_assign(&a,123,&p,adapter,&o,&out,0)==COOP_ALLOC_SUCCESS);
 assert(coop_allocator_count(&a)==1);
 assert(coop_allocator_find(&a,123)>=0);
 assert(coop_allocator_release(&a,123)==1);
 o.ground_hit=0;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_UNSUPPORTED);
 o.ground_hit=1;o.ground_normal.y=0.2f;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_BLOCKED);
 o.ground_normal.y=1.f;o.ground_point.y=-400.f;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_UNSUPPORTED);
 o.ground_point.y=-300.f;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_BLOCKED);
 o.ground_point.y=-320.f;o.bounds_max.x=0.f;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_UNSUPPORTED);
 o.bounds_max.x=20.f;p.x=1e9f;
 assert(coop_volume_assess(&p,&o)==COOP_PROBE_UNSUPPORTED);
 assert(coop_volume_assess(0,&o)==COOP_PROBE_UNSUPPORTED);
 assert(coop_volume_assess(&p,0)==COOP_PROBE_UNSUPPORTED);
 return 0;
}
