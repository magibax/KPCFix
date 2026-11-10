#ifndef KPC_COOP_SPAWN_H
#define KPC_COOP_SPAWN_H

/* Experimental THREE-PLAYER R1M2B spawn trial. No proprietary world data.
 * Input/output coordinates are IEEE-754 single-precision bit patterns.
 * These map-specific positions are NOT collision/floor-validated by code. */
#define COOP_TRIAL_SLOTS 3u
typedef struct { U32 player[COOP_TRIAL_SLOTS]; } CoopSpawnReservations;

/* Only the exact R1M2B single-player entry anchor is eligible.
 * Player identity is an engine object pointer for this test; reservations
 * remain until object module reinitialization, NOT merely disconnect. */
static int coop_claim_slot(CoopSpawnReservations *state,U32 player){
 U32 i;
 if(!player)return -1;
 for(i=0;i<COOP_TRIAL_SLOTS;i++)
  if(state->player[i]==player)return (int)i;
 for(i=0;i<COOP_TRIAL_SLOTS;i++)
  if(!state->player[i]){state->player[i]=player;return (int)i;}
 return -1;
}
static int coop_is_r1m2b_anchor(const U32 *xyz){
 return xyz&&xyz[0]==0x44c80000u&&xyz[1]==0xc3800000u&&xyz[2]==0xc5380000u;
}
/* Slot 0: entry anchor       (1600,-256,-2944) — user verified.
 * Slot 1: 96 units +Z       (1600,-256,-2848) — user verified.
 * Slot 2: 96 units -Z       (1600,-256,-3040) — UNTESTED.
 * All slots use stock engine placement/telefrag handling afterward.
 * Set maxclients=3: fourth players are NOT accommodated by this trial. */
static void coop_assign_trial_position(U32 *xyz,int slot){
 if(slot==1)xyz[2]=0xc5320000u;
 if(slot==2)xyz[2]=0xc53e0000u;
}
#endif
