#ifndef KPC_COOP_MODEL_STATE_H
#define KPC_COOP_MODEL_STATE_H
#include "coop_model_registry.h"
/* Original 1.13 x86 DLL: typed object constructor at RVA 0x1d403
 * copies initial flags into object+0x28 and type into object+0x76.
 * Physics broadphase at RVA 0x19d0c checks object flags & 0x12000;
 * NO single flag bit has yet been proven to mean all-world solidity.
 * se_InitWorldModel copies model+0x11c..124 into object+0x40..48.
 * These vectors alone do not establish a world-to-model transform.
 * This is read-only observational research: NO SAFE spawn inference.
 */
typedef struct {
 unsigned int matched,skipped_unverified;
 unsigned int type_1,type_other,type_unreadable;
 unsigned int flags_2000,flags_10000,flags_any_12000,flags_unreadable;
 unsigned int init_vector_equal,init_vector_different,init_vector_unreadable;
} CoopModelRawState;
static void coop_model_state_observe(void *user,
 unsigned int entry,unsigned int model,unsigned int object,
 int matched,CoopModelRead32 read,void *ctx){
 CoopModelRawState *s=(CoopModelRawState *)user;
 unsigned int type_word,flags,object_vec[3],model_vec[3],i;
 int vectors_valid=1,equal=1;
 (void)entry;
 if(!s)return;
 if(!matched){s->skipped_unverified++;return;}
 s->matched++;
 if(coop_model_read_offset(read,ctx,object,0x74u,&type_word)){
  if(((type_word>>16)&255u)==1u)s->type_1++;
  else s->type_other++;
 }else s->type_unreadable++;
 if(coop_model_read_offset(read,ctx,object,0x28u,&flags)){
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
 else s->init_vector_different++;
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
