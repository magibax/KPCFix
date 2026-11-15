#ifndef KPC_COOP_AABB_OVERLAP_H
#define KPC_COOP_AABB_OVERLAP_H
/* Read-only full-box vs full-box overlap broadphase. It cannot inspect BSP
 * geometry, actors not supplied to it, or the loaded map. A disjoint pair
 * NEVER proves a spawn safe. Native 1.13 helper RVA 0x18991 compares six
 * float boundaries; with tolerance zero, all three axes must overlap. */
#include "coop_allocator.h"
typedef struct { CoopVec3 min,max; } CoopAabb;
typedef enum { COOP_AABB_UNKNOWN=-1,COOP_AABB_DISJOINT=0,COOP_AABB_OVERLAP=1 } CoopAabbResult;
static int coop_aabb_valid(const CoopAabb *box){
 return box&&coop_valid_vec(&box->min)&&coop_valid_vec(&box->max)&&
   box->min.x<box->max.x&&box->min.y<box->max.y&&box->min.z<box->max.z;
}
/* Player centre + actual server-relative bounds. Never normalize invalid
 * bounds; malformed/unknown game physics must fail closed. */
static int coop_aabb_from_relative(const CoopVec3 *centre,
                                   const CoopVec3 *lo,const CoopVec3 *hi,
                                   CoopAabb *out){
 CoopAabb box;
 if(!centre||!lo||!hi||!out||!coop_valid_vec(centre)||
    !coop_valid_vec(lo)||!coop_valid_vec(hi)||
    !(lo->x<0&&lo->y<0&&lo->z<0&&
      hi->x>0&&hi->y>0&&hi->z>0&&
      lo->x>=-128&&lo->y>=-128&&lo->z>=-128&&
      hi->x<=128&&hi->y<=128&&hi->z<=128))return 0;
 box.min=(CoopVec3){centre->x+lo->x,centre->y+lo->y,centre->z+lo->z};
 box.max=(CoopVec3){centre->x+hi->x,centre->y+hi->y,centre->z+hi->z};
 if(!coop_aabb_valid(&box))return 0;
 *out=box;
 return 1;
}
/* Native positive overlap tolerance HIDES shallow collisions: use 0.f
 * for conservative co-op collision rejection. Touching faces at tolerance
 * zero are disjoint under the original routine's strict comparison.
 * INVALID is UNKNOWN, not DISJOINT. */
static CoopAabbResult coop_aabb_overlap(const CoopAabb *a,
                                        const CoopAabb *b,float tolerance){
 if(!coop_aabb_valid(a)||!coop_aabb_valid(b)||
    !(tolerance>=0.f&&tolerance<=128.f))return COOP_AABB_UNKNOWN;
 return (a->min.x < b->max.x-tolerance && a->max.x > b->min.x+tolerance &&
         a->min.y < b->max.y-tolerance && a->max.y > b->min.y+tolerance &&
         a->min.z < b->max.z-tolerance && a->max.z > b->min.z+tolerance)
         ?COOP_AABB_OVERLAP:COOP_AABB_DISJOINT;
}
#endif
