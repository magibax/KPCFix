#ifndef KPC_COOP_BSP_BOX_H
#define KPC_COOP_BSP_BOX_H

/* Read-only conservative AABB-vs-BSP leaf traversal.
 * Inferred from the *original* 1.13 server.dll RVA 0x1068f point walker.
 * This policy consumes DECODED nodes via a caller-owned read callback.
 *
 * INTERIOR_ONLY does not mean safe spawn: moving WorldModels, actors,
 * trigger volumes and map-specific scripting are not covered.
 * No engine pointer is accessed by this header.
 */
#define COOP_BSP_MAX_STACK 256u
#define COOP_BSP_MAX_VISITS 16384u
#define COOP_BSP_MAX_DEPTH 128u

typedef struct {float x,y,z;} CoopBspVec;
typedef struct {CoopBspVec min,max;} CoopBspBox;
typedef struct {
 unsigned int flags;                /* bit 1: native class-1 leaf; bit 2: class-0 */
 CoopBspVec normal; float distance; /* plane at native node +8 */
 unsigned int child_neg,child_pos;   /* native node +0x2c, +0x30 */
} CoopBspNode;
typedef enum {
 COOP_BSP_UNKNOWN=-1,
 COOP_BSP_TOUCHES_CLASS_ZERO=0,
 COOP_BSP_INTERIOR_ONLY=1
} CoopBspResult;
typedef int (*CoopBspReadNode)(void *ctx,unsigned int node,CoopBspNode *out);
typedef struct {unsigned int key,depth;} CoopBspTask;

static int coop_bsp_finite(float x){
 return x>-1000000.f&&x<1000000.f;
}
static int coop_bsp_vec_valid(CoopBspVec v){
 return coop_bsp_finite(v.x)&&coop_bsp_finite(v.y)&&coop_bsp_finite(v.z);
}
static int coop_bsp_box_valid(const CoopBspBox *b){
 return b&&coop_bsp_vec_valid(b->min)&&coop_bsp_vec_valid(b->max)&&
    b->min.x<b->max.x&&b->min.y<b->max.y&&b->min.z<b->max.z;
}
/* Conservative interval traversal. Crossing / touching a splitting plane
 * examines BOTH children; even a very thin BSP leaf enclosed by the player
 * volume is detected (unlike probes limited to rays through box edges).
 * Any unreadable node, invalid plane, cycle or exhausted budget is UNKNOWN.
 * Child 0 is signed distance <=0, child 1 is >0 in native point walker.
 */
static CoopBspResult coop_bsp_box_classify(const CoopBspBox *box,
  unsigned int root,CoopBspReadNode reader,void *ctx,unsigned int *visited){
 CoopBspTask todo[COOP_BSP_MAX_STACK];
 unsigned int count=1,steps=0;
 if(visited)*visited=0;
 if(!coop_bsp_box_valid(box)||!reader)return COOP_BSP_UNKNOWN;
 todo[0]=(CoopBspTask){root,0};
 while(count){
  CoopBspTask t=todo[--count];
  CoopBspNode n;
  float pmin,pmax;
  if(t.depth>=COOP_BSP_MAX_DEPTH || steps>=COOP_BSP_MAX_VISITS ||
     !reader(ctx,t.key,&n))return COOP_BSP_UNKNOWN;
  steps++;
  if(visited)*visited=steps;
  /* Identical precedence to the original 1.13 RVA 0x1068f. */
  if(n.flags&2u)return COOP_BSP_TOUCHES_CLASS_ZERO;
  if(n.flags&1u)continue;
  if(!coop_bsp_vec_valid(n.normal)||!coop_bsp_finite(n.distance)||
     (n.normal.x==0.f&&n.normal.y==0.f&&n.normal.z==0.f))
    return COOP_BSP_UNKNOWN;
  pmin=(n.normal.x>=0.f?n.normal.x*box->min.x:n.normal.x*box->max.x)+
       (n.normal.y>=0.f?n.normal.y*box->min.y:n.normal.y*box->max.y)+
       (n.normal.z>=0.f?n.normal.z*box->min.z:n.normal.z*box->max.z)-n.distance;
  pmax=(n.normal.x>=0.f?n.normal.x*box->max.x:n.normal.x*box->min.x)+
       (n.normal.y>=0.f?n.normal.y*box->max.y:n.normal.y*box->min.y)+
       (n.normal.z>=0.f?n.normal.z*box->max.z:n.normal.z*box->min.z)-n.distance;
  if(!coop_bsp_finite(pmin)||!coop_bsp_finite(pmax))
    return COOP_BSP_UNKNOWN;
  if(pmax<0.f){
   if(count>=COOP_BSP_MAX_STACK)return COOP_BSP_UNKNOWN;
   todo[count++]=(CoopBspTask){n.child_neg,t.depth+1};
  }else if(pmin>0.f){
   if(count>=COOP_BSP_MAX_STACK)return COOP_BSP_UNKNOWN;
   todo[count++]=(CoopBspTask){n.child_pos,t.depth+1};
  }else{
   if(count+2>COOP_BSP_MAX_STACK)return COOP_BSP_UNKNOWN;
   todo[count++]=(CoopBspTask){n.child_pos,t.depth+1};
   todo[count++]=(CoopBspTask){n.child_neg,t.depth+1};
  }
 }
 return COOP_BSP_INTERIOR_ONLY;
}
#endif
