#ifndef KPC_COOP_ALLOCATOR_H
#define KPC_COOP_ALLOCATOR_H

/* Portable co-op spawn assignment policy. No engine calls or game assets.
 *
 * The engine adapter MUST provide a real geometry/ground/collision probe.
 * UNKNOWN is never silently treated as SAFE. This is intentionally separate
 * from the existing R1M2B-only trial hook (which uses three live-vetted points).
 *
 * Local U32 typedef is provided by winmini.h in the DLL; standalone native
 * tests typedef unsigned int U32 before including this header.
 */

#define COOP_ALLOCATOR_CAPACITY 8u
#define COOP_ALLOCATOR_CANDIDATES 25u
#define COOP_ALLOCATOR_MIN_GAP 64.0f
#define COOP_ALLOCATOR_STEP 96.0f

typedef struct { float x,y,z; } CoopVec3;
typedef struct {
 U32 owner; /* player object pointer in the current world generation */
 CoopVec3 position;
} CoopAllocation;
typedef struct {
 U32 world_generation;
 CoopAllocation slots[COOP_ALLOCATOR_CAPACITY];
} CoopAllocator;

typedef enum {
 COOP_PROBE_UNSUPPORTED=-1,
 COOP_PROBE_BLOCKED=0,
 COOP_PROBE_SAFE=1
} CoopProbeResult;

/* A candidate is SAFE only if the host engine has checked that the player
 * body fits, is grounded and in the playable world. The callback must be
 * read-only and side-effect-free. It writes the *actual verified* placement
 * to out (allowing floor-adjusted Y) only on COOP_PROBE_SAFE.
 * A segment ray alone is NOT enough.
 */
typedef CoopProbeResult (*CoopProbe)(void *ctx,const CoopVec3 *candidate,CoopVec3 *out);

typedef enum {
 COOP_ALLOC_NO_SAFE_POSITION=0,
 COOP_ALLOC_SUCCESS=1,
 COOP_ALLOC_FULL=-1,
 COOP_ALLOC_BAD_INPUT=-2
} CoopAllocationStatus;

static void coop_allocator_reset(CoopAllocator *a,U32 generation){
 U32 i;
 if(!a)return;
 a->world_generation=generation;
 for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++){
  a->slots[i].owner=0;
  a->slots[i].position.x=0.f;
  a->slots[i].position.y=0.f;
  a->slots[i].position.z=0.f;
 }
}
static int coop_allocator_find(const CoopAllocator *a,U32 owner){
 U32 i;
 if(!a||!owner)return -1;
 for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++)
  if(a->slots[i].owner==owner)return (int)i;
 return -1;
}
static int coop_allocator_release(CoopAllocator *a,U32 owner){
 int slot=coop_allocator_find(a,owner);
 if(slot<0)return 0;
 a->slots[slot].owner=0;
 return 1;
}
static int coop_allocator_count(const CoopAllocator *a){
 U32 i;int n=0;
 if(!a)return 0;
 for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++)
  if(a->slots[i].owner)n++;
 return n;
}

/* Deterministic centre-first, expanding square rings in the XZ plane.
 * The Y coordinate comes from the campaign entry anchor and can be changed
 * ONLY by the validated host collision/ground probe, not by this generator.
 */
static CoopVec3 coop_candidate(const CoopVec3 *anchor,U32 index){
 static const signed char vectors[COOP_ALLOCATOR_CANDIDATES][2]={
  {0,0}, {0,1}, {0,-1}, {1,0}, {-1,0},
  {1,1}, {-1,1}, {1,-1}, {-1,-1},
  {0,2}, {0,-2}, {2,0}, {-2,0},
  {2,1}, {-2,1}, {2,-1}, {-2,-1},
  {1,2}, {-1,2}, {1,-2}, {-1,-2},
  {2,2}, {-2,2}, {2,-2}, {-2,-2}
 };
 CoopVec3 result={0,0,0};
 if(!anchor||index>=COOP_ALLOCATOR_CANDIDATES)return result;
 result.x=anchor->x+COOP_ALLOCATOR_STEP*vectors[index][0];
 result.y=anchor->y;
 result.z=anchor->z+COOP_ALLOCATOR_STEP*vectors[index][1];
 return result;
}
/* Bound candidate coordinates to finite, plausible world coordinates, even
 * if a faulty physics callback reports them as safe. This comparison rejects
 * NaN and infinities without relying on libm or host floating-point helpers.
 */
static int coop_valid_vec(const CoopVec3 *p){
 return p&&p->x>-1000000.f&&p->x<1000000.f&&
           p->y>-1000000.f&&p->y<1000000.f&&
           p->z>-1000000.f&&p->z<1000000.f;
}
static int coop_candidate_clear(const CoopAllocator *a,const CoopVec3 *candidate){
 U32 i;
 for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++){
  const CoopAllocation *slot=&a->slots[i];
  if(slot->owner){
   float dx=candidate->x-slot->position.x;
   float dz=candidate->z-slot->position.z;
   /* Conservative horizontal exclusion, regardless of vertical difference.
    * The host's physics callback must still verify full-body clearance. */
   if(dx*dx+dz*dz<COOP_ALLOCATOR_MIN_GAP*COOP_ALLOCATOR_MIN_GAP)
    return 0;
  }
 }
 return 1;
}
static CoopAllocationStatus coop_allocator_assign(
 CoopAllocator *a,U32 owner,const CoopVec3 *anchor,
 CoopProbe probe,void *ctx,CoopVec3 *out,U32 *candidate_index){
 U32 i;
 int slot,existing;
 if(!a||!owner||!coop_valid_vec(anchor)||!probe||!out)return COOP_ALLOC_BAD_INPUT;
 existing=coop_allocator_find(a,owner);
 if(existing>=0){
  *out=a->slots[existing].position;
  if(candidate_index)*candidate_index=COOP_ALLOCATOR_CANDIDATES;
  return COOP_ALLOC_SUCCESS;
 }
 slot=-1;
 for(i=0;i<COOP_ALLOCATOR_CAPACITY;i++)
  if(!a->slots[i].owner){slot=(int)i;break;}
 if(slot<0)return COOP_ALLOC_FULL;
 for(i=0;i<COOP_ALLOCATOR_CANDIDATES;i++){
  CoopVec3 candidate=coop_candidate(anchor,i),validated;
  if(!coop_valid_vec(&candidate)||!coop_candidate_clear(a,&candidate))
   continue;
  /* Only a POSITIVE, independently validated result is eligible.
   * Recheck the adjusted position; a floor snap must not cause overlap.
   */
  if(probe(ctx,&candidate,&validated)!=COOP_PROBE_SAFE)continue;
  if(!coop_valid_vec(&validated)||!coop_candidate_clear(a,&validated))
   continue;
  a->slots[slot].owner=owner;
  a->slots[slot].position=validated;
  *out=validated;
  if(candidate_index)*candidate_index=i;
  return COOP_ALLOC_SUCCESS;
 }
 return COOP_ALLOC_NO_SAFE_POSITION;
}
#endif
