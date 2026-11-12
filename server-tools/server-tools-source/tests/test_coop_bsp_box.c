#include "coop_bsp_box.h"
#include <assert.h>
typedef struct {CoopBspNode nodes[8];unsigned int count;} Fixture;
static int read_fixture(void *ctx,unsigned int key,CoopBspNode *out){
 const Fixture *f=(const Fixture*)ctx;
 if(!f||!out||key>=f->count)return 0;
 *out=f->nodes[key];return 1;
}
static CoopBspBox box(float xmin,float xmax){
 CoopBspBox b={{xmin,-302.f,-2964.f},{xmax,-210.f,-2924.f}};
 return b;
}
int main(void){
 Fixture f={0};
 CoopBspBox b;
 unsigned int n=0;
 /* Native point-classifier branch orientation:
  * class one -> inside status 0x40; zero -> outside status 0x41. */
 f.count=3;
 f.nodes[0]=(CoopBspNode){0u,{1.f,0.f,0.f},1600.f,1u,2u};
 f.nodes[1].flags=1u;f.nodes[2].flags=2u;
 b=box(1550.f,1580.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_INTERIOR_ONLY);
 assert(n==2);
 b=box(1610.f,1630.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_TOUCHES_CLASS_ZERO);
 b=box(1580.f,1620.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_TOUCHES_CLASS_ZERO);
 /* Exact touching is conservative: both partitions explored. */
 b=box(1580.f,1600.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_TOUCHES_CLASS_ZERO);
 /* The thin outside slab is wholly INSIDE the player AABB but no
  * corner is in that slab. Point-/corner-only probes miss it. */
 f.count=4;
 f.nodes[0]=(CoopBspNode){0u,{1.f,0.f,0.f},1596.f,1u,2u};
 f.nodes[1].flags=1u;
 f.nodes[2]=(CoopBspNode){0u,{1.f,0.f,0.f},1604.f,3u,1u};
 f.nodes[3].flags=2u;
 b=box(1580.f,1620.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_TOUCHES_CLASS_ZERO);
 b=box(1560.f,1580.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_INTERIOR_ONLY);
 b=box(1610.f,1630.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_INTERIOR_ONLY);
 /* Same technique detects a wall crossing the known control box X=1664. */
 f.nodes[0].distance=1664.f;
 f.nodes[2].distance=1700.f;
 b=box(1634.f,1674.f);
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_TOUCHES_CLASS_ZERO);
 /* Unknown/malformed tree never approves an interior. */
 f.nodes[0].normal=(CoopBspVec){0,0,0};
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_UNKNOWN);
 f.nodes[0].normal=(CoopBspVec){1,0,0};
 f.nodes[0].child_neg=99u;
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_UNKNOWN);
 f.nodes[0].child_neg=0u;f.nodes[0].child_pos=0u;
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_UNKNOWN);
 b.min.x=10000000.f;
 assert(coop_bsp_box_classify(&b,0,read_fixture,&f,&n)==COOP_BSP_UNKNOWN);
 assert(coop_bsp_box_classify(0,0,read_fixture,&f,&n)==COOP_BSP_UNKNOWN);
 assert(coop_bsp_box_classify(&b,0,0,&f,&n)==COOP_BSP_UNKNOWN);
 return 0;
}
