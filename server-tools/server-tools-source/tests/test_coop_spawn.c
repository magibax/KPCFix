#include "winmini.h"
#include "coop_spawn.h"
#include <assert.h>

int main(void){
 CoopSpawnReservations r={{0,0}};
 U32 a[]={0x44c80000u,0xc3800000u,0xc5380000u};
 U32 b[]={0x44c80000u,0xc3800000u,0xc5380000u};
 U32 c[]={0x44c80000u,0xc3800000u,0xc5380000u};
 assert(coop_is_r1m2b_anchor(a));
 assert(coop_claim_slot(&r,0x1111)==0);
 assert(coop_claim_slot(&r,0x2222)==1);
 assert(coop_claim_slot(&r,0x1111)==0);
 assert(coop_claim_slot(&r,0x2222)==1);
 assert(coop_claim_slot(&r,0x3333)==-1);
 assert(coop_claim_slot(&r,0)==-1);
 coop_assign_trial_position(a,0);
 coop_assign_trial_position(b,1);
 assert(a[2]==0xc5380000u);
 assert(b[2]==0xc5320000u);
 assert(!coop_is_r1m2b_anchor(b));
 c[0]=0;
 assert(!coop_is_r1m2b_anchor(c));
 return 0;
}
