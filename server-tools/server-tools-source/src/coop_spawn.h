#ifndef KPC_COOP_SPAWN_H
#define KPC_COOP_SPAWN_H

/* Experimental TWO-PLAYER R1M2B spawn trial. No proprietary world data.
 * Input/output coordinates are IEEE-754 single-precision bit patterns.
 * The helper intentionally does not claim to validate world collision. */
typedef struct { U32 player[2]; } CoopSpawnReservations;

/* The caller must first establish an exact R1M2B single-player anchor match.
 * This function reserves two distinct slots, stable for a player's respawn.
 * It does not recycle disconnected players (research build only). */
static int coop_claim_slot(CoopSpawnReservations *state,U32 player){
 if(!player)return -1;
 if(state->player[0]==player)return 0;
 if(state->player[1]==player)return 1;
 if(!state->player[0]){state->player[0]=player;return 0;}
 if(!state->player[1]){state->player[1]=player;return 1;}
 return -1;
}
static int coop_is_r1m2b_anchor(const U32 *xyz){
 return xyz&&xyz[0]==0x44c80000u&&xyz[1]==0xc3800000u&&xyz[2]==0xc5380000u;
}
/* Trial: from (1600,-256,-2944) to (1600,-256,-2848).
 * Object locations suggest the map progresses along Z here.
 * This is **not** a collision or floor safety certification. */
static void coop_assign_trial_position(U32 *xyz,int slot){
 if(slot==1)xyz[2]=0xc5320000u;
}
#endif
