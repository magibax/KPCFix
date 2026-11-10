#ifndef KPC_COOP_MODEL_STATE_H
#define KPC_COOP_MODEL_STATE_H
#include "coop_model_registry.h"
/* Original 1.13 x86 DLL: typed object constructor at RVA 0x1d403
 * copies initial flags into object+0x28 and type into object+0x76.
 * Physics broadphase at RVA 0x19d0c checks object flags & 0x12000;
 * NO single flag bit has yet been proven to mean all-world solidity.
 * se_InitWorldModel copies model+0x11c..124 into object+0x40..48.
 * These vectors alone do not establish a world-to-model transform.
 * Type 2 follows the WorldModel-specific native path at original server
 * RVAs 0x18435 and 0x20719; type 1 is NOT a WorldModel identifier.
 * Source/current vector samples are raw IEEE-754 bit patterns, not
 * proof of motion, world transforms, or safe placement.
 * This is read-only observational research: NO SAFE spawn inference.
 */
#define COOP_MODEL_VECTOR_SAMPLE_LIMIT 2u
typedef struct {
 unsigned int object,resource,type,flags;
 unsigned int model_vector[3],object_vector[3];
} CoopModelVectorSample;
typedef struct {
 unsigned int matched,skipped_unverified;
 unsigned int type_1,type_2,type_other,type_unreadable;
 unsigned int flags_2000,flags_10000,flags_any_12000,flags_unreadable;
 unsigned int init_vector_equal,init_vector_different,init_vector_unreadable;
 unsigned int sample_count;
 CoopModelVectorSample samples[COOP_MODEL_VECTOR_SAMPLE_LIMIT];
} CoopModelRawState;
static void coop_model_state_observe(void *user,
 unsigned int entry,unsigned int model,unsigned int object,
 int matched,CoopModelRead32 read,void *ctx){
 CoopModelRawState *s=(CoopModelRawState *)user;
 unsigned int type_word=0,flags=0,object_vec[3],model_vec[3],i;
 unsigned int raw_type=0xffffffffu;
 int type_ok,flags_ok,vectors_valid=1,equal=1;
 (void)entry;
 if(!s)return;
 if(!matched){s->skipped_unverified++;return;}
 s->matched++;
 type_ok=coop_model_read_offset(read,ctx,object,0x74u,&type_word);
 if(type_ok){
  raw_type=(type_word>>16)&255u;
  if(raw_type==2u)s->type_2++;
  else if(raw_type==1u)s->type_1++;
  else s->type_other++;
 }else s->type_unreadable++;
 flags_ok=coop_model_read_offset(read,ctx,object,0x28u,&flags);
 if(flags_ok){
  if(flags&0x2000u)s->flags_2000++;
  if(flags&0x10000u)s->flags_10000++;
  if(flags&0x12000u)s->flags_any_12000++;
 }else s->flags_unreadable++;
 for(i=0;i<3u;i++){
  if(!coop_model_read_offset(read,ctx,object,0x40u+i*4u,&object_vec[i])||
     !coop_model_read_offset(read,ctx,model,0x11cu+i*4u,&model_vec[i])){
    vectors_valid=0;break;
  }
  if(object_vec[i]!=model_vec[i])equal=0;
 }
 if(!vectors_valid)s->init_vector_unreadable++;
 else if(equal)s->init_vector_equal++;
 else{
  s->init_vector_different++;
  if(s->sample_count<COOP_MODEL_VECTOR_SAMPLE_LIMIT){
   CoopModelVectorSample *sample=&s->samples[s->sample_count++];
   sample->object=object;sample->resource=entry;
   sample->type=type_ok?raw_type:0xffffffffu;
   sample->flags=flags_ok?flags:0xffffffffu;
   for(i=0;i<3u;i++){
    sample->model_vector[i]=model_vec[i];
    sample->object_vector[i]=object_vec[i];
   }
  }
 }
}
static CoopModelRegistryStatus coop_model_state_scan(
 unsigned int world_state,CoopModelRead32 read,void *ctx,
 CoopModelRegistrySnapshot *registry,CoopModelRawState *state){
 if(!state)return COOP_MODEL_REGISTRY_UNKNOWN;
 *state=(CoopModelRawState){0};
 return coop_model_registry_scan_visit(world_state,read,ctx,registry,
                                      coop_model_state_observe,state);
}
#endif
