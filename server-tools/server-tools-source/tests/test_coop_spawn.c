typedef unsigned int U32;
#include "coop_spawn.h"
#include <assert.h>

#define X 0x44c80000u
#define Y 0xc3800000u
#define Z0 0xc5380000u
#define Z1 0xc5320000u
#define Z2 0xc53e0000u

int main(void){
 CoopSpawnReservations r={{0}};
 U32 a[]={X,Y,Z0}, b[]={X,Y,Z0}, c[]={X,Y,Z0};
 U32 off_map[]={X+1,Y,Z0};
 assert(COOP_TRIAL_SLOTS==3);
 assert(coop_is_r1m2b_anchor(a));
 assert(coop_is_r1m2b_anchor(b));
 assert(coop_is_r1m2b_anchor(c));
 assert(!coop_is_r1m2b_anchor(off_map));
 assert(!coop_is_r1m2b_anchor(0));
 assert(coop_claim_slot(&r,0)==-1);
 assert(coop_claim_slot(&r,0x1111)==0);
 assert(coop_claim_slot(&r,0x2222)==1);
 assert(coop_claim_slot(&r,0x3333)==2);
 assert(coop_claim_slot(&r,0x1111)==0); /* same player respawn */
 assert(coop_claim_slot(&r,0x2222)==1);
 assert(coop_claim_slot(&r,0x3333)==2);
 assert(coop_claim_slot(&r,0x4444)==-1); /* limit must not alias */
 coop_assign_trial_position(a,0);
 coop_assign_trial_position(b,1);
 coop_assign_trial_position(c,2);
 assert(a[0]==X&&a[1]==Y&&a[2]==Z0);
 assert(b[0]==X&&b[1]==Y&&b[2]==Z1);
 assert(c[0]==X&&c[1]==Y&&c[2]==Z2);
 assert(a[2]!=b[2]&&a[2]!=c[2]&&b[2]!=c[2]);
 return 0;
}
