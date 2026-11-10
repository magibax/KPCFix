#include "coop_model_registry.h"
#include <assert.h>
typedef struct {unsigned int key,value;} Pair;
typedef struct {Pair cells[24];unsigned int count,reads;} Fixture;
static void put(Fixture *f,unsigned int address,unsigned int value){
 assert(f->count<24);f->cells[f->count++]=(Pair){address,value};
}
static int read_cell(void *context,unsigned int address,unsigned int *value){
 Fixture *f=(Fixture*)context;
 unsigned int i;
 f->reads++;
 for(i=0;i<f->count;i++)if(f->cells[i].key==address){
  *value=f->cells[i].value;return 1;
 }
 return 0;
}
static Fixture standard(void){
 Fixture f={0};
 /* world state 0x1000: sentinel 0x10b0; 2 list entries */
 put(&f,0x10b4,0x2000);
 put(&f,0x2004,0x2100);put(&f,0x2008,0x3000);
 put(&f,0x2104,0x10b0);put(&f,0x2108,0x3100);
 put(&f,0x3000,0x4000);put(&f,0x3100,0x4100);
 put(&f,0x4044,0x5000);put(&f,0x40dc,0x6000);
 put(&f,0x4144,0x5100);put(&f,0x41dc,0);
 put(&f,0x5000,0x8);put(&f,0x5100,0x8);
 return f;
}
static void change(Fixture *f,unsigned int address,unsigned int value){
 unsigned int i;
 for(i=0;i<f->count;i++)if(f->cells[i].key==address){
  f->cells[i].value=value;return;
 }
 assert(!"Missing fixture cell");
}
int main(void){
 Fixture f=standard();
 CoopModelRegistrySnapshot s;
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.entries==2&&s.nonzero_models==2&&s.root_candidates==2);
 assert(s.associated_candidates==1&&s.first_root==0x5000);
 assert(s.first_associated==0x6000&&f.reads<=20);
 /* A traversed registry is NOT proof of any live-solid actor or clearance. */
 f=(Fixture){0};put(&f,0x10b4,0x10b0);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.entries==0&&s.root_candidates==0);
 f=standard();change(&f,0x2104,0x2000);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 assert(s.entries==COOP_MODEL_REGISTRY_LIMIT);
 f=standard();change(&f,0x2008,0);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 f=standard();change(&f,0x3000,0);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 f=standard();change(&f,0x4044,0x7000);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 f=standard();change(&f,0x4044,0);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,&s)==COOP_MODEL_REGISTRY_TRAVERSED);
 assert(s.root_candidates==1&&s.entries==2);
 f=standard();
 assert(coop_model_registry_scan(0,read_cell,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 assert(coop_model_registry_scan(0xfffffff0u,read_cell,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 assert(coop_model_registry_scan(0x1000,0,&f,&s)==COOP_MODEL_REGISTRY_UNKNOWN);
 assert(coop_model_registry_scan(0x1000,read_cell,&f,0)==COOP_MODEL_REGISTRY_UNKNOWN);
 return 0;
}
