#ifndef KPC_COOP_MODEL_REGISTRY_H
#define KPC_COOP_MODEL_REGISTRY_H
/* Conservative READ-ONLY survey of the original server's named WorldModel
 * RESOURCE registry, NOT a live-solid-object inventory.
 * Original 1.13 server.dll RVA 0x8f69 traverses world state +0xb0:
 * sentinel+4 is next link, node+8 is resource entry, entry[0] is the
 * model, model+0x44 is a candidate BSP root (as on the base model).
 * se_InitWorldModel (RVA 0x22958) stores model+0xdc = live-object pointer.
 * These identities and transforms are not verified by this snapshot.
 */
#define COOP_MODEL_REGISTRY_LIMIT 256u
typedef int (*CoopModelRead32)(void *ctx,unsigned int address,unsigned int *value);
typedef struct {
 unsigned int entries,nonzero_models,root_candidates,associated_candidates;
 unsigned int first_root,first_associated;
 unsigned int reciprocal_matches,reciprocal_mismatches,reciprocal_unreadable;
} CoopModelRegistrySnapshot;
typedef enum { COOP_MODEL_REGISTRY_UNKNOWN=-1,COOP_MODEL_REGISTRY_TRAVERSED=1 } CoopModelRegistryStatus;

static int coop_model_address_plus(unsigned int address,
                                   unsigned int delta,unsigned int *sum){
 if(!address||address>0xffffffffu-delta||!sum)return 0;
 *sum=address+delta;return 1;
}
static int coop_model_read_offset(CoopModelRead32 read,void *ctx,
                       unsigned int address,unsigned int delta,unsigned int *value){
 unsigned int offset;
 return coop_model_address_plus(address,delta,&offset)&&read(ctx,offset,value);
}
typedef void (*CoopModelAssociationVisitor)(void *visit_ctx,
 unsigned int entry,unsigned int model,unsigned int object,
 int reference_matches,CoopModelRead32 reader,void *read_ctx);
static CoopModelRegistryStatus coop_model_registry_scan_visit(
 unsigned int world_state,CoopModelRead32 read,void *ctx,
 CoopModelRegistrySnapshot *summary,CoopModelAssociationVisitor visitor,
 void *visit_ctx){
 unsigned int sentinel,next,entry,model,root,associated,i;
 if(!summary)return COOP_MODEL_REGISTRY_UNKNOWN;
 *summary=(CoopModelRegistrySnapshot){0};
 if(!read||!coop_model_address_plus(world_state,0xb0u,&sentinel)||
    !coop_model_read_offset(read,ctx,sentinel,4u,&next))
    return COOP_MODEL_REGISTRY_UNKNOWN;
 for(i=0;next!=sentinel;i++){
  unsigned int new_next;
  if(i>=COOP_MODEL_REGISTRY_LIMIT||!next||
     !coop_model_read_offset(read,ctx,next,4u,&new_next)||
     !coop_model_read_offset(read,ctx,next,8u,&entry)||
     !coop_model_read_offset(read,ctx,entry,0u,&model))
     return COOP_MODEL_REGISTRY_UNKNOWN;
  summary->entries++;
  if(!model)return COOP_MODEL_REGISTRY_UNKNOWN;
  summary->nonzero_models++;
  if(!coop_model_read_offset(read,ctx,model,0x44u,&root)||
     !coop_model_read_offset(read,ctx,model,0xdcu,&associated))
     return COOP_MODEL_REGISTRY_UNKNOWN;
  if(root){
   unsigned int first_word;
   if(!coop_model_read_offset(read,ctx,root,0u,&first_word))
    return COOP_MODEL_REGISTRY_UNKNOWN;
   if(!summary->first_root)summary->first_root=root;
   summary->root_candidates++;
  }
  if(associated){
   unsigned int backref=0;
   int matched=0;
   if(!summary->first_associated)summary->first_associated=associated;
   summary->associated_candidates++;
   /* Original 1.13 se_InitWorldModel sets object+0x140 to the resource
    * record and model+0xdc to the object (RVAs 0x22990, 0x22998).
    * Even matching pointers do not prove the object is active/solid.
    */
   if(!coop_model_read_offset(read,ctx,associated,0x140u,&backref))
    summary->reciprocal_unreadable++;
   else if(backref==entry){
    summary->reciprocal_matches++;matched=1;
   }else summary->reciprocal_mismatches++;
   if(visitor)visitor(visit_ctx,entry,model,associated,matched,read,ctx);
  }
  next=new_next;
 }
 /* TRAVERSED certifies bounded RESOURCE linkage only. Even a reciprocal
  * pointer does not prove object liveness, solidity or world transforms. */
 return COOP_MODEL_REGISTRY_TRAVERSED;
}
static CoopModelRegistryStatus coop_model_registry_scan(
 unsigned int world_state,CoopModelRead32 read,void *ctx,
 CoopModelRegistrySnapshot *summary){
 return coop_model_registry_scan_visit(world_state,read,ctx,summary,0,0);
}
#endif
