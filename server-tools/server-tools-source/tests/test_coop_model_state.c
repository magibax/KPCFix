#include "coop_model_state.h"
#include <assert.h>
typedef struct {unsigned int key,val;} Cell;
typedef struct {Cell items[64];unsigned int count;} Fixture;
static void set(Fixture *f,unsigned int key,unsigned int val){
 unsigned int i;
 for(i=0;i<f->count;i++)if(f->items[i].key==key){
  f->items[i].val=val;return;
 }
 assert(f->count<64u);f->items[f->count++]=(Cell){key,val};
}
static int get(void *ctx,unsigned int address,unsigned int *value){
 Fixture *f=(Fixture *)ctx;
 unsigned int i;
 for(i=0;i<f->count;i++)if(f->items[i].key==address){
  *value=f->items[i].val;return 1;
 }
 return 0;
}
int main(void){
 Fixture f={0};
 CoopModelRegistrySnapshot r={0};
 CoopModelRawState s={0};
 CoopModelRegistryStatus st;
 /* One isolated verified WorldModel association. */
 set(&f,0x10b4,0x2000);set(&f,0x2004,0x10b0);
 set(&f,0x2008,0x3000);set(&f,0x3000,0x4000);
 set(&f,0x4044,0x5000);set(&f,0x5000,0x2);
 set(&f,0x40dc,0x6000);set(&f,0x6140,0x3000);
 set(&f,0x6028,0x12000);set(&f,0x6074,0x00020000);
 set(&f,0x6040,1);set(&f,0x6044,2);set(&f,0x6048,3);
 set(&f,0x411c,1);set(&f,0x4120,2);set(&f,0x4124,3);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(r.entries==1&&r.reciprocal_matches==1);
 assert(s.matched==1&&s.type_2==1&&s.type_1==0&&s.type_other==0);
 assert(s.flags_2000==1&&s.flags_10000==1&&s.flags_any_12000==1);
 assert(s.init_vector_equal==1&&s.init_vector_different==0&&s.sample_count==0);
 /* The constructor places a byte at object+0x76; preserve its exact code.
  * The old v4 test incorrectly treated type one as WorldModel. */
 set(&f,0x6074,0x00010000);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.type_1==1&&s.type_2==0&&s.type_other==0);
 set(&f,0x6074,0x00030000);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.type_other==1&&s.type_1==0&&s.type_2==0);
 set(&f,0x6074,0x00020000);
 /* Raw flags masked independently, and a changed initial vector is
  * evidence of divergence only, NOT a verified world transform. */
 set(&f,0x6028,0);
 set(&f,0x6040,4);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.matched==1&&s.flags_any_12000==0&&s.init_vector_different==1);
 assert(s.sample_count==1&&s.samples[0].object==0x6000);
 assert(s.samples[0].resource==0x3000&&s.samples[0].type==2);
 assert(s.samples[0].flags==0&&s.samples[0].model_vector[0]==1);
 assert(s.samples[0].object_vector[0]==4);
 /* Broken reciprocal links are never evaluated as valid object state. */
 set(&f,0x6140,0x9999);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(r.reciprocal_mismatches==1&&s.skipped_unverified==1&&s.matched==0);
 assert(s.sample_count==0);
 /* Unreadable object state should remain observable as unreadable. */
 f=(Fixture){0};
 set(&f,0x10b4,0x2000);set(&f,0x2004,0x10b0);
 set(&f,0x2008,0x3000);set(&f,0x3000,0x4000);
 set(&f,0x4044,0x5000);set(&f,0x5000,0x2);
 set(&f,0x40dc,0x6000);set(&f,0x6140,0x3000);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.type_unreadable==1&&s.flags_unreadable==1);
 assert(s.init_vector_unreadable==1);
 /* A cycle remains UNKNOWN even when state data is partially readable. */
 set(&f,0x2004,0x2000);
 st=coop_model_state_scan(0x1000,get,&f,&r,&s);
 assert(st==COOP_MODEL_REGISTRY_UNKNOWN);
 assert(coop_model_state_scan(0x1000,get,&f,&r,0)==COOP_MODEL_REGISTRY_UNKNOWN);
 assert(coop_model_registry_scan(0x1000,get,&f,&r)==COOP_MODEL_REGISTRY_UNKNOWN);
 return 0;
}
