#ifndef KPC_COOP_INVENTORY_H
#define KPC_COOP_INVENTORY_H
/* Per-member inventory persistence, player classes and pickup respawn policy
 * for the co-op session. Pure and freestanding: the proxy reads the player
 * through verified object.lto routines, fills a CoopInvState and executes the
 * returned plan with the engine's own give call.
 *
 * Evidence (original 1.13 object.lto 3a03f879, base 0x10000000):
 *   item ids (table 0x141e40, stride 0x20): weapons 2-14 (Fists..Stargaze),
 *     ammo 0x11-0x14 (Magma/Water/Air/Earth), BlackDiamond 0x1e (the armour
 *     shard: powerShardItem_t sets definition 0x1421f8, id 0x1e, cd23b),
 *     armour pieces 0x27-0x2c (Gauntlets..Mask; bits of player+0x260, f25f0).
 *   player+0x54 inventory; dd190(inv, id) count, f25f0(player, id) has-item,
 *   f3440(player, id, amount, notify) give (thiscall, ret 0xc). A weapon
 *   other than Fists needs the gauntlets (f3471) and the player's class
 *   (player+0x264) unless its definition allows any class (f3489-f349b).
 *   Ammo is capped at 100 + 100 per armour piece, so armour is restored
 *   before ammo.
 *   player+0x264 class: 0 Starbearer, 1 Beast King, 2 Celestial, 3 Demon
 *   (f2090 model table); f2090(player, 1) applies it (the stock
 *   CyclePlayerClass cheat fab30 does +0x264 = (+0x264 + 1) & 3, f2090(1)).
 *   The class change does not touch the inventory. */

#include "coop_aabb_overlap.h"
#define COOP_INV_WEAPON_FIRST 2u
#define COOP_INV_WEAPON_LAST 14u
#define COOP_INV_AMMO_FIRST 0x11u
#define COOP_INV_AMMO_LAST 0x14u
#define COOP_INV_SHARD 0x1eu
#define COOP_INV_ARMOUR_FIRST 0x27u
#define COOP_INV_ARMOUR_LAST 0x2cu
#define COOP_INV_CLASSES 4u
#define COOP_INV_PLAN_MAX 32u

typedef struct {
 unsigned int cls;                 /* 0-3, or >3 unknown */
 unsigned short weapons;           /* bit (id - 2) for ids 2-14 */
 unsigned char armour;             /* bit (id - 0x27) for ids 0x27-0x2c */
 unsigned int ammo[4];             /* ids 0x11-0x14 */
 unsigned int shards;              /* id 0x1e count */
} CoopInvState;

typedef struct {
 unsigned int valid,client;        /* member identity the snapshot belongs to */
 CoopInvState s;
} CoopInvSnap;

typedef struct {unsigned int id,amount;} CoopInvGive;
typedef struct {
 int set_class;                    /* -1 unchanged, else the class to apply first */
 unsigned int count;
 CoopInvGive give[COOP_INV_PLAN_MAX];  /* in order: armour, weapons, shards, ammo */
} CoopInvPlan;

static int coop_inv_weapon(unsigned int id){return id>=COOP_INV_WEAPON_FIRST&&id<=COOP_INV_WEAPON_LAST;}
static int coop_inv_armour(unsigned int id){return id>=COOP_INV_ARMOUR_FIRST&&id<=COOP_INV_ARMOUR_LAST;}
static int coop_inv_ammo(unsigned int id){return id>=COOP_INV_AMMO_FIRST&&id<=COOP_INV_AMMO_LAST;}

static unsigned int coop_inv_bits(unsigned int v){unsigned int n=0;while(v){n+=v&1u;v>>=1;}return n;}

/* What to give so that `now` has at least everything in `saved`. Nothing is
 * ever taken away; counts only grow (a fresh spawn's start items stay). */
static void coop_inv_plan(const CoopInvState *saved,const CoopInvState *now,CoopInvPlan *p){
 unsigned int id,k;
 p->set_class=-1;p->count=0;
 if(!saved||!now)return;
 if(saved->cls<COOP_INV_CLASSES&&saved->cls!=now->cls)p->set_class=(int)saved->cls;
#define COOP_INV_ADD(i,a) do{if(p->count<COOP_INV_PLAN_MAX){p->give[p->count].id=(i);p->give[p->count].amount=(a);p->count++;}}while(0)
 for(id=COOP_INV_ARMOUR_FIRST;id<=COOP_INV_ARMOUR_LAST;id++){
  unsigned int bit=1u<<(id-COOP_INV_ARMOUR_FIRST);
  if((saved->armour&bit)&&!(now->armour&bit))COOP_INV_ADD(id,1u);
 }
 for(id=COOP_INV_WEAPON_FIRST;id<=COOP_INV_WEAPON_LAST;id++){
  unsigned int bit=1u<<(id-COOP_INV_WEAPON_FIRST);
  if((saved->weapons&bit)&&!(now->weapons&bit))COOP_INV_ADD(id,1u);
 }
 if(saved->shards>now->shards)COOP_INV_ADD(COOP_INV_SHARD,saved->shards-now->shards);
 for(k=0;k<4u;k++)if(saved->ammo[k]>now->ammo[k])COOP_INV_ADD(COOP_INV_AMMO_FIRST+k,saved->ammo[k]-now->ammo[k]);
#undef COOP_INV_ADD
}
/* Short summary for the log: "class=Demon weapons=5 armour=3 shards=10 ammo=100/0/50/0". */
static const char *coop_class_name(unsigned int cls){
 static const char *const names[COOP_INV_CLASSES]={"Starbearer","Beast King","Celestial","Demon"};
 return cls<COOP_INV_CLASSES?names[cls]:"?";
}
/* !class argument: name, realm or 1-4. Returns 0-3, or -1. */
static int coop_class_parse(const char *w){
 static const char *const keys[]={"starbearer","star","water","beastking","beast","earth","celestial","air","demon","fire"};
 static const unsigned char cls[]={0,0,0,1,1,1,2,2,3,3};
 char a[24];unsigned int i,n=0,k;
 if(!w)return -1;
 for(i=0;w[i]&&n+1<sizeof a;i++){
  char c=w[i];
  if(c==' '||c=='\t'||c=='_'||c=='-')continue;
  if(c>='A'&&c<='Z')c=(char)(c+32);
  a[n++]=c;
 }
 a[n]=0;
 if(n==1&&a[0]>='1'&&a[0]<='4')return a[0]-'1';
 for(k=0;k<sizeof(keys)/sizeof(keys[0]);k++){
  for(i=0;keys[k][i]&&a[i]==keys[k][i];i++){}
  if(!keys[k][i]&&!a[i])return cls[k];
 }
 return -1;
}

/* ---- pickup respawn (CoopItemRespawnSec) ----
 * object.lto: the pickup path cc180 hides a taken item with cc570(item, 0)
 * (cc29e); cc570 schedules a respawn instead of removing the object when the
 * item's Respawn property +0xb4 is set (+0x79 = 1, countdown +0x7c = Respawn
 * Time +0xb8, float seconds). The item update counts +0x7c down (cbfe6, while
 * +0x81 is set) and calls cc080 (cbfff), which makes it visible and touchable
 * again. Campaign worlds author Respawn=false, so taken items were removed.
 * Respawning in co-op: weapons, ammo, armour shards, health and the other
 * charms/power-ups. Progression items never respawn: pamphlets 0x19, the
 * pickup trigger 0x1c, the ultimate weapon 0x23, keys 0x24, tickets 0x25 and
 * the armour pieces 0x27-0x2c (each has its own cutscene). */
static int coop_item_respawnable(unsigned int id){
 if(id<COOP_INV_WEAPON_FIRST||id>0x2cu)return 0;
 if(id==0xfu||id==0x10u||id==0x15u||id==0x16u)return 0;   /* table markers */
 if(id==0x19u||id==0x1cu||id==0x23u||id==0x24u||id==0x25u||id>=0x26u)return 0;
 return 1;
}
/* Half extents of the boxes for the "player on the spot" test: the player's
 * server bounds (-20,-46,-20)..(20,46,20) (GENERAL-ALLOCATOR.md) and a pickup
 * box a little larger than the stock pickup models (BoundingBox 20x55x20). */
#define COOP_RESPAWN_PLAYER_HX 20.0f
#define COOP_RESPAWN_PLAYER_HY 46.0f
#define COOP_RESPAWN_ITEM_HX 24.0f
#define COOP_RESPAWN_ITEM_HY 32.0f
#define COOP_RESPAWN_RETRY_SEC 0.5f
/* 1 when no player box overlaps the item's spot (positions are box centres);
 * players: count positions, 3 floats each. Unknown/invalid boxes count as
 * occupied (fail closed: the item waits). */
static int coop_respawn_spot_clear(const float item[3],const float *players,unsigned int count){
 unsigned int i;
 CoopAabb a,b;
 a.min=(CoopVec3){item[0]-COOP_RESPAWN_ITEM_HX,item[1]-COOP_RESPAWN_ITEM_HY,item[2]-COOP_RESPAWN_ITEM_HX};
 a.max=(CoopVec3){item[0]+COOP_RESPAWN_ITEM_HX,item[1]+COOP_RESPAWN_ITEM_HY,item[2]+COOP_RESPAWN_ITEM_HX};
 for(i=0;i<count;i++){
  const float *p=players+3u*i;
  b.min=(CoopVec3){p[0]-COOP_RESPAWN_PLAYER_HX,p[1]-COOP_RESPAWN_PLAYER_HY,p[2]-COOP_RESPAWN_PLAYER_HX};
  b.max=(CoopVec3){p[0]+COOP_RESPAWN_PLAYER_HX,p[1]+COOP_RESPAWN_PLAYER_HY,p[2]+COOP_RESPAWN_PLAYER_HX};
  if(coop_aabb_overlap(&a,&b,0.f)!=COOP_AABB_DISJOINT)return 0;
 }
 return 1;
}
#endif
