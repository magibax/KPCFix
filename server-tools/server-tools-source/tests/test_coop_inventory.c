/* Asset-free tests for co-op inventory persistence, player classes and the
 * pickup respawn policy (coop_inventory.h). */
#pragma GCC diagnostic ignored "-Wunused-function"
typedef unsigned int U32;   /* coop_allocator.h (via coop_aabb_overlap.h) uses the proxy's U32 */
#include "coop_inventory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static CoopInvState state(unsigned int cls,unsigned int weapons,unsigned int armour,unsigned int shards,
                          unsigned int a0,unsigned int a1,unsigned int a2,unsigned int a3){
 CoopInvState s;memset(&s,0,sizeof s);
 s.cls=cls;s.weapons=(unsigned short)weapons;s.armour=(unsigned char)armour;s.shards=shards;
 s.ammo[0]=a0;s.ammo[1]=a1;s.ammo[2]=a2;s.ammo[3]=a3;return s;
}
#define W(id) (1u<<((id)-COOP_INV_WEAPON_FIRST))
#define ARM(id) (1u<<((id)-COOP_INV_ARMOUR_FIRST))

static void test_plan(void){
 CoopInvPlan p;
 /* A fresh spawn (Fists only, start gauntlets) after a world change. */
 CoopInvState saved=state(3,W(2)|W(3)|W(8)|W(14),ARM(0x27)|ARM(0x29),40,120,0,55,300);
 CoopInvState fresh=state(0,W(2),ARM(0x27),0,0,0,0,0);
 coop_inv_plan(&saved,&fresh,&p);
 assert(p.set_class==3);
 /* armour first (ammo cap), then weapons, then shards, then ammo. */
 assert(p.count==8);
 assert(p.give[0].id==0x29&&p.give[0].amount==1);
 assert(p.give[1].id==3&&p.give[2].id==8&&p.give[3].id==14);
 assert(p.give[4].id==COOP_INV_SHARD&&p.give[4].amount==40);
 assert(p.give[5].id==0x11&&p.give[5].amount==120);
 assert(p.give[6].id==0x13&&p.give[6].amount==55);
 assert(p.give[7].id==0x14&&p.give[7].amount==300);
 /* Nothing is taken away: more than saved stays, equal gives nothing. */
 {CoopInvState more=state(3,W(2)|W(3)|W(8)|W(14)|W(9),0x3f,90,200,10,60,300);
  coop_inv_plan(&saved,&more,&p);assert(p.set_class==-1&&p.count==0);}
 coop_inv_plan(&saved,&saved,&p);assert(p.set_class==-1&&p.count==0);
 /* Unknown saved class: unchanged. */
 {CoopInvState odd=saved;odd.cls=7;coop_inv_plan(&odd,&fresh,&p);assert(p.set_class==-1);}
 /* A partially restored player (weapon ammo came with the weapons). */
 {CoopInvState part=state(3,W(2)|W(3)|W(8)|W(14),ARM(0x27)|ARM(0x29),10,150,0,20,0);
  coop_inv_plan(&saved,&part,&p);
  assert(p.count==3&&p.give[0].id==COOP_INV_SHARD&&p.give[0].amount==30&&p.give[1].id==0x13&&p.give[1].amount==35&&p.give[2].id==0x14&&p.give[2].amount==300);}
 /* Every armour piece and weapon at once stays within the plan size. */
 {CoopInvState all=state(1,0x1fff,0x3f,90,700,700,700,700),none=state(1,0,0,0,0,0,0,0);
  coop_inv_plan(&all,&none,&p);assert(p.count==6+13+1+4&&p.count<=COOP_INV_PLAN_MAX);}
 coop_inv_plan(0,&fresh,&p);assert(p.count==0&&p.set_class==-1);
 assert(coop_inv_bits(0x1fff)==13&&coop_inv_bits(0)==0&&coop_inv_bits(ARM(0x27)|ARM(0x2c))==2);
 assert(coop_inv_weapon(2)&&coop_inv_weapon(14)&&!coop_inv_weapon(15)&&!coop_inv_weapon(1));
 assert(coop_inv_armour(0x27)&&coop_inv_armour(0x2c)&&!coop_inv_armour(0x2d)&&coop_inv_ammo(0x11)&&!coop_inv_ammo(0x15));
}
static void test_classes(void){
 assert(!strcmp(coop_class_name(0),"Starbearer")&&!strcmp(coop_class_name(1),"Beast King"));
 assert(!strcmp(coop_class_name(2),"Celestial")&&!strcmp(coop_class_name(3),"Demon")&&!strcmp(coop_class_name(4),"?"));
 assert(coop_class_parse("starbearer")==0&&coop_class_parse("Water")==0&&coop_class_parse("1")==0);
 assert(coop_class_parse("beastking")==1&&coop_class_parse("Beast King")==1&&coop_class_parse("beast_king")==1&&coop_class_parse("earth")==1);
 assert(coop_class_parse("celestial")==2&&coop_class_parse("AIR")==2&&coop_class_parse("3")==2);
 assert(coop_class_parse("demon")==3&&coop_class_parse("fire")==3&&coop_class_parse("4")==3);
 assert(coop_class_parse("")==-1&&coop_class_parse("5")==-1&&coop_class_parse("0")==-1&&coop_class_parse("demons")==-1);
 assert(coop_class_parse("nightmare")==-1&&coop_class_parse(0)==-1&&coop_class_parse("dem")==-1);
}
static void test_respawn_policy(void){
 unsigned int id;
 /* Weapons, ammo, health, charms, shards respawn. */
 for(id=2;id<=14;id++)assert(coop_item_respawnable(id));
 for(id=0x11;id<=0x14;id++)assert(coop_item_respawnable(id));
 assert(coop_item_respawnable(0x17)&&coop_item_respawnable(0x18)&&coop_item_respawnable(0x1a)&&coop_item_respawnable(0x1b));
 assert(coop_item_respawnable(0x1d)&&coop_item_respawnable(0x1e)&&coop_item_respawnable(0x1f)&&coop_item_respawnable(0x22));
 /* Progression items and markers never do. */
 assert(!coop_item_respawnable(0x19)&&!coop_item_respawnable(0x1c)&&!coop_item_respawnable(0x23));
 assert(!coop_item_respawnable(0x24)&&!coop_item_respawnable(0x25));
 assert(!coop_item_respawnable(0x26)&&!coop_item_respawnable(0x2d)&&!coop_item_respawnable(0x2e));
 /* Armour pieces ("armor shards" in the game) respawn; a respawned one is picked up quietly. */
 for(id=0x27;id<=0x2c;id++)assert(coop_item_respawnable(id));
 assert(coop_armour_quiet(0x29,0,0,0)==0);                     /* first pickup: stock cutscene */
 assert(coop_armour_quiet(0x29,1,0,0)==1);                     /* respawned, player lacks it */
 assert(coop_armour_quiet(0x29,1,1,0)==2&&coop_armour_quiet(0x29,1,0,1)==2);   /* has it / dead */
 assert(coop_armour_quiet(0x1e,1,0,0)==0);                     /* not an armour piece */
 assert(!coop_item_respawnable(0)&&!coop_item_respawnable(1)&&!coop_item_respawnable(0xf)&&!coop_item_respawnable(0x10));
 assert(!coop_item_respawnable(0x15)&&!coop_item_respawnable(0x16)&&!coop_item_respawnable(0x100));
}
static void test_spot(void){
 const float item[3]={100.f,20.f,-50.f};
 float players[9]={1000.f,46.f,0.f, 100.f,46.f,-50.f, -400.f,0.f,0.f};
 assert(coop_respawn_spot_clear(item,players,0));
 assert(coop_respawn_spot_clear(item,players,1));                  /* far away */
 assert(!coop_respawn_spot_clear(item,players,2));                 /* standing on it */
 players[3]=100.f+COOP_RESPAWN_ITEM_HX+COOP_RESPAWN_PLAYER_HX;      /* touching faces: clear */
 assert(coop_respawn_spot_clear(item,players,3));
 players[3]-=1.f;assert(!coop_respawn_spot_clear(item,players,3));
 players[3]=100.f;players[4]=20.f+COOP_RESPAWN_ITEM_HY+COOP_RESPAWN_PLAYER_HY+1.f;   /* above it */
 assert(coop_respawn_spot_clear(item,players,3));
 /* Invalid (NaN) positions fail closed: the item waits. */
 {union{unsigned int u;float f;}nan;nan.u=0x7fc00000u;players[0]=nan.f;assert(!coop_respawn_spot_clear(item,players,1));}
}
static void test_class_swap(void){
 /* ids 2-14; group 3-6 melee (BeastClaws 1, Punisher 3, Thornblade 0, Twister 2),
  * 7-10 any class, 11-14 one per class; Fists any class. */
 unsigned int c[13];unsigned int id;
#define B(i) ((unsigned short)(1u<<((i)-2u)))
 c[0]=COOP_INV_ANY_CLASS;c[1]=1;c[2]=3;c[3]=0;c[4]=2;
 for(id=7;id<=10;id++)c[id-2]=COOP_INV_ANY_CLASS;
 c[9]=3;c[10]=2;c[11]=1;c[12]=0;
 /* Starbearer with Thornblade, a gun and the class weapon of 11-14 -> Demon gets Punisher and id 11. */
 assert(coop_inv_class_swap(B(5)|B(8)|B(14),c,3)==(B(4)|B(11)));
 /* Already owns the Demon melee: only the other group. */
 assert(coop_inv_class_swap(B(4)|B(5)|B(14),c,3)==B(11));
 /* Only any-class weapons (Fists, guns): nothing to add. */
 assert(coop_inv_class_swap(B(2)|B(7)|B(9),c,1)==0);
 /* Nothing owned: nothing. Back to the own class: nothing new. */
 assert(coop_inv_class_swap(0,c,2)==0);
 assert(coop_inv_class_swap(B(5)|B(14),c,0)==0);
 /* Unknown classes (unreadable definitions) never match; bad input. */
 for(id=0;id<13;id++)c[id]=COOP_INV_CLASSES;
 assert(coop_inv_class_swap(B(5),c,0)==0);
 assert(coop_inv_class_swap(B(5),0,0)==0&&coop_inv_class_swap(B(5),c,4)==0);
#undef B
}
static void test_ammo_excess(void){
 /* RT3: 197 saved; the restore's weapon gives pushed it to 200 -> take 3 back. */
 CoopInvState saved;unsigned int base[4]={0,0,0,0},now[4]={200,200,50,0},out[4];
 memset(&saved,0,sizeof saved);saved.ammo[0]=197;saved.ammo[1]=200;saved.ammo[2]=120;saved.ammo[3]=0;
 coop_inv_ammo_excess(&saved,base,now,out);
 assert(out[0]==3&&out[1]==0&&out[2]==0&&out[3]==0);   /* below saved: given later, never taken */
 /* The spawn's own start ammo stays: base above saved. */
 base[3]=40;now[3]=90;coop_inv_ammo_excess(&saved,base,now,out);assert(out[3]==50);
 now[3]=30;coop_inv_ammo_excess(&saved,base,now,out);assert(out[3]==0);
 /* No snapshot: only the base counts. */
 coop_inv_ammo_excess(0,base,now,out);assert(out[0]==200&&out[3]==0);
}
static void test_fresh_start(void){
 /* A start from the lobby forgets the saved inventory; every other change keeps it. */
 assert(!coop_inv_keep_through_change(1));
 assert(coop_inv_keep_through_change(0));
}
int main(void){
 test_plan();test_classes();test_respawn_policy();test_spot();test_class_swap();test_fresh_start();test_ammo_excess();
 printf("PASS coop inventory: restore plan order and growth only, classes, respawnable items, spot test, class-change weapons, fresh start, ammo trim after weapon gives\n");
 return 0;
}
