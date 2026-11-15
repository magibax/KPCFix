typedef unsigned int U32;
#include "coop_aabb_overlap.h"
#include "coop_volume_probe.h"
#include <assert.h>
static CoopProbeResult unverified_world(void *ctx,const CoopVec3 *p,CoopVec3 *out){
 (void)ctx;(void)p;(void)out;
 return COOP_PROBE_UNSUPPORTED;
}
int main(void){
 CoopVec3 centre={1600.f,-256.f,-2944.f};
 CoopVec3 lo={-20.f,-46.f,-20.f},hi={20.f,46.f,20.f};
 CoopAabb player={0},other={0};
 assert(coop_aabb_from_relative(&centre,&lo,&hi,&player));
 assert(player.min.x==1580.f&&player.max.x==1620.f);
 assert(player.min.y==-302.f&&player.max.y==-210.f);
 assert(player.min.z==-2964.f&&player.max.z==-2924.f);
 assert(coop_aabb_overlap(&player,&player,0)==COOP_AABB_OVERLAP);
 /* Completely enclosed obstacle missed by edge ray samples. */
 other=(CoopAabb){{1596.f,-265.f,-2940.f},{1604.f,-260.f,-2935.f}};
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_OVERLAP);
 assert(coop_aabb_overlap(&other,&player,0)==COOP_AABB_OVERLAP);
 /* Known wall x=1664 intersects control player's box x=[1634,1674]. */
 centre.x=1654.f;
 assert(coop_aabb_from_relative(&centre,&lo,&hi,&player));
 other=(CoopAabb){{1664.f,-400.f,-3000.f},{1680.f,-180.f,-2900.f}};
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_OVERLAP);
 centre=(CoopVec3){1600.f,-256.f,-2944.f};
 assert(coop_aabb_from_relative(&centre,&lo,&hi,&player));
 centre.z=-2848.f;
 assert(coop_aabb_from_relative(&centre,&lo,&hi,&other));
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_DISJOINT);
 other.min.z=player.max.z;other.max.z=other.min.z+40.f;
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_DISJOINT);
 other.min.z=player.max.z-1.f;
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_OVERLAP);
 /* Positive tolerance is NOT appropriate for clearance: hides overlap. */
 assert(coop_aabb_overlap(&player,&other,1.f)==COOP_AABB_DISJOINT);
 other.min.x=9999999.f;
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_UNKNOWN);
 other=(CoopAabb){{10,10,10},{0,0,0}};
 assert(coop_aabb_overlap(&player,&other,0)==COOP_AABB_UNKNOWN);
 assert(coop_aabb_overlap(&player,&player,-1.f)==COOP_AABB_UNKNOWN);
 assert(coop_aabb_overlap(&player,&player,129.f)==COOP_AABB_UNKNOWN);
 lo.y=1.f;
 assert(!coop_aabb_from_relative(&centre,&lo,&hi,&other));
 assert(coop_aabb_overlap(0,&player,0)==COOP_AABB_UNKNOWN);
 /* Exercise the generic allocator's conservative UNKNOWN path as well. */
 {CoopAllocator allocator;
  CoopVec3 out;
  coop_allocator_reset(&allocator,3);
  assert(coop_allocator_count(&allocator)==0);
  assert(coop_allocator_assign(&allocator,456,&centre,unverified_world,0,&out,0)==COOP_ALLOC_NO_SAFE_POSITION);
  assert(coop_allocator_release(&allocator,456)==0);
  assert(coop_allocator_count(&allocator)==0);
 }
 /* Box-to-box checks cannot certify full world/BSP clearance. */
 {CoopVolumeObservation v={0};
  v.bounds_min=(CoopVec3){-20,-46,-20};
  v.bounds_max=(CoopVec3){20,46,20};
  v.ground_hit=1;v.ground_point=(CoopVec3){1600,-320,-2944};
  v.ground_normal=(CoopVec3){0,1,0};
  v.world_hull=COOP_VOLUME_UNKNOWN;v.other_actors=COOP_VOLUME_UNKNOWN;
  assert(coop_volume_assess(&centre,&v)==COOP_PROBE_UNSUPPORTED);
 }
 return 0;
}
