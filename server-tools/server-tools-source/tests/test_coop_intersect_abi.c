typedef unsigned int U32;
#include "coop_intersect_abi.h"
#include <assert.h>

int main(void){
 KpcIntersectQuery q={0};
 KpcIntersectInfo hit={0};
 assert(KPC_INTERSECT_SERVER_SLOT==0x90u);
 assert(KPC_INTERSECT_OBJECTS==1u);
 assert(KPC_INTERSECT_HPOLY==4u);
 assert(KPC_INTERSECT_QUERY_MIN_BYTES==0x30u);
 assert(sizeof(q)==0x40u);
 assert(sizeof(hit)==0x40u);
 q.from.x=1600.f;q.from.y=-240.f;q.from.z=-2944.f;
 q.to=q.from;q.to.y=-512.f;
 q.flags=KPC_INTERSECT_OBJECTS|KPC_INTERSECT_HPOLY;
 q.object_filter=0x12345678u;
 q.filter_userdata=0x87654321u;
 assert(q.flags==5u);
 assert(q.from.x==q.to.x&&q.from.z==q.to.z);
 assert(q.from.y>q.to.y);
 assert(q.object_filter==0x12345678u);
 assert(q.filter_userdata==0x87654321u);
 assert(hit.hit_object==0u&&hit.surface_flags==0u);
 return 0;
}
