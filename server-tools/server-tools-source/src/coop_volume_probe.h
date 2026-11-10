#ifndef KPC_COOP_VOLUME_PROBE_H
#define KPC_COOP_VOLUME_PROBE_H
/* Pure evidence gate. A line-sample miss NEVER certifies a player-sized hull.
 * Native full-hull and active-actor clearance must be independently verified.
 * This header cannot move players or change the original engine.
 */
#include "coop_allocator.h"
typedef enum {COOP_VOLUME_UNKNOWN=-1,COOP_VOLUME_BLOCKED=0,COOP_VOLUME_VERIFIED_CLEAR=1} CoopVolumeEvidence;
typedef struct {
 CoopVec3 bounds_min,bounds_max,ground_point,ground_normal;
 int ground_hit,sampled_body_hit;
 CoopVolumeEvidence world_hull,other_actors;
} CoopVolumeObservation;
static CoopProbeResult coop_volume_assess(const CoopVec3 *center,const CoopVolumeObservation *o){
 float gap;
 if(!center||!o||!coop_valid_vec(center)||!coop_valid_vec(&o->bounds_min)||
    !coop_valid_vec(&o->bounds_max)||
    !(o->bounds_min.x<0&&o->bounds_min.y<0&&o->bounds_min.z<0&&
      o->bounds_max.x>0&&o->bounds_max.y>0&&o->bounds_max.z>0&&
      o->bounds_min.x>=-128&&o->bounds_min.y>=-128&&o->bounds_min.z>=-128&&
      o->bounds_max.x<=128&&o->bounds_max.y<=128&&o->bounds_max.z<=128))
  return COOP_PROBE_UNSUPPORTED;
 if(o->sampled_body_hit||o->world_hull==COOP_VOLUME_BLOCKED||
    o->other_actors==COOP_VOLUME_BLOCKED)return COOP_PROBE_BLOCKED;
 if(!o->ground_hit||!coop_valid_vec(&o->ground_point)||
    !coop_valid_vec(&o->ground_normal))return COOP_PROBE_UNSUPPORTED;
 if(o->ground_normal.y<0.70f)return COOP_PROBE_BLOCKED;
 gap=center->y+o->bounds_min.y-o->ground_point.y;
 if(gap < -1.f)return COOP_PROBE_BLOCKED;
 if(gap>32.f)return COOP_PROBE_UNSUPPORTED;
 if(o->world_hull!=COOP_VOLUME_VERIFIED_CLEAR||
    o->other_actors!=COOP_VOLUME_VERIFIED_CLEAR)return COOP_PROBE_UNSUPPORTED;
 return COOP_PROBE_SAFE;
}
#endif
