/* Asset-free tests for the WORLDORDER campaign graph and host selection
 * (CoopCampaign=1). The world table itself is checked against the shipped
 * WORLDORDER.TXT by coop/tools/check_campaign_table.py when assets exist. */
#pragma GCC diagnostic ignored "-Wunused-function"
#include "coop_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define A 0x100u
#define B 0x200u
static CoopResult r;

static int spawn(CoopSession *s,unsigned int client,const char *name,unsigned int now){
 return coop_session_seen(s,client,client+1,client+2,name,now,&r);
}
static void cmd(CoopSession *s,unsigned int client,const char *text){coop_session_command(s,client,text,1000,&r);}
static void load(CoopSession *s,const char *world,unsigned int now){
 coop_session_world_loading(s,now);coop_session_world_loaded(s,world);
}
static void lobby(CoopSession *s){
 coop_session_init(s,2,3);coop_session_enable_campaign(s);
 spawn(s,A,"Alice",0);spawn(s,B,"Bob",0);
}

static void test_table(void){
 unsigned int first,len,i,realm_first[6]={0},seen[6]={0};
 assert(COOP_CAMPAIGN_LEN==48);
 assert(!strcmp(coop_campaign_worlds[0].world,"realm1/r1m0")&&(coop_campaign_worlds[0].flags&COOP_W_INTRO));
 assert(!strcmp(coop_campaign_worlds[47].world,"realm5/r5m1b"));
 for(i=0;i<COOP_CAMPAIGN_LEN;i++){
  const CoopWorld *w=&coop_campaign_worlds[i];char label[16];
  assert(w->realm>=1&&w->realm<=5);
  if(i)assert(w->realm>=coop_campaign_worlds[i-1].realm);           /* realms are contiguous */
  if(!seen[w->realm]){seen[w->realm]=1;realm_first[w->realm]=i;}
  coop_world_label(w->world,label);assert(!strcmp(label,w->label));  /* label = path tail */
 }
 /* The four realm openings are INTRO maps; Nightmare has none. */
 for(i=1;i<=4;i++)assert(coop_campaign_worlds[realm_first[i]].flags&COOP_W_INTRO);
 assert(!(coop_campaign_worlds[realm_first[5]].flags&COOP_W_INTRO)&&!strcmp(coop_campaign_worlds[realm_first[5]].label,"R5M1A"));
 assert(realm_first[2]==12&&realm_first[3]==25&&realm_first[4]==35&&realm_first[5]==46); /* WORLDORDER "Index =" comments */
 assert(coop_campaign_slice(COOP_SEL_FULL,0,&first,&len)&&first==0&&len==48);
 assert(coop_campaign_slice(COOP_SEL_FROM,2,&first,&len)&&first==12&&len==36);
 assert(coop_campaign_slice(COOP_SEL_REALM,2,&first,&len)&&first==12&&len==13);
 assert(coop_campaign_slice(COOP_SEL_REALM,3,&first,&len)&&first==25&&len==10);
 assert(coop_campaign_slice(COOP_SEL_FROM,5,&first,&len)&&first==46&&len==2);
 assert(coop_campaign_slice(COOP_SEL_REALM,5,&first,&len)&&first==46&&len==2);
 assert(!coop_campaign_slice(COOP_SEL_REALM,0,&first,&len)&&!coop_campaign_slice(COOP_SEL_FROM,6,&first,&len));
 assert(!coop_campaign_slice(COOP_SEL_MVP,1,&first,&len));
 assert(coop_realm_parse("water")==1&&coop_realm_parse("nightmare")==5&&coop_realm_parse("3")==3);
 assert(coop_realm_parse("starbearer")==1&&coop_realm_parse("beastking")==2&&coop_realm_parse("celestial")==3&&coop_realm_parse("demon")==4);
 assert(!coop_realm_parse("6")&&!coop_realm_parse("0")&&!coop_realm_parse("wate")&&!coop_realm_parse("waters")&&!coop_realm_parse(""));
}
static void test_selection_commands(void){
 CoopSession s;
 lobby(&s);
 assert(s.sel_mode==COOP_SEL_FULL&&s.route_len==48&&!strcmp(coop_route_label(&s,0),"R1M0"));
 cmd(&s,B,"!campaign");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"full campaign (R1M0..R5M1B, 48 worlds)"));
 assert(strlen(r.msg)<COOP_MSG_MAX);
 cmd(&s,B,"!campaign realm fire");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host"));
 cmd(&s,A,"!ready");cmd(&s,B,"!ready");assert(coop_ready_count(&s)==2);
 cmd(&s,A,"!campaign  from   Earth");
 assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"from Earth (R2M0..R5M1B, 36 worlds)")&&coop_ready_count(&s)==0);
 cmd(&s,A,"!campaign from 2");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Already selected"));
 cmd(&s,A,"!campaign realm demon");assert(strstr(r.msg,"realm Fire (R4M0..R4M4B, 11 worlds)"));
 cmd(&s,A,"!campaign realm nightmare");assert(strstr(r.msg,"realm Nightmare (R5M1A..R5M1B, 2 worlds)"));
 cmd(&s,A,"!campaign from narnia");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Usage"));
 cmd(&s,A,"!campaign sideways");assert(strstr(r.msg,"Usage"));
 cmd(&s,A,"!campaign from earth now");assert(r.action==COOP_ACT_NONE);        /* three words: not a command */
 cmd(&s,A,"!ready now");assert(r.action==COOP_ACT_NONE&&coop_ready_count(&s)==0); /* arguments only for !campaign */
 cmd(&s,A,"!campaign full");assert(r.action==COOP_ACT_ANNOUNCE&&s.route_len==48);
 /* A joining member does not change the selection. */
 spawn(&s,0x300u,"Carol",10);assert(s.sel_mode==COOP_SEL_FULL);
 cmd(&s,A,"!help");assert(strstr(r.msg,"!campaign")&&strlen(r.msg)<COOP_MSG_MAX);
}
static void test_full_route_and_exits(void){
 CoopSession s;
 lobby(&s);
 cmd(&s,A,"!ready");cmd(&s,B,"!ready");cmd(&s,A,"!start");
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==0&&strstr(r.msg,"Starting -> R1M0"));
 assert(!strcmp(coop_target_world(&s,0),"realm1/r1m0"));
 load(&s,"worlds\\realm1\\R1M0",2000);
 assert(s.phase==COOP_PHASE_INTRO_PENDING&&s.route_pos==0&&!strcmp(s.world_label,"R1M0"));
 spawn(&s,A,"Alice",2500);assert(s.phase==COOP_PHASE_GAMEPLAY);
 /* Only the WORLDORDER successor is accepted. */
 coop_session_exit(&s,"worlds/realm1/r1m2a",3000,&r);assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"not the next world"));
 coop_session_exit(&s,"worlds/realm1/r1m1a",3000,&r);assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==1);
 load(&s,"worlds\\realm1\\R1M1A",4000);spawn(&s,A,"Alice",4100);
 cmd(&s,B,"!next");assert(strstr(r.msg,"Only the host"));
 cmd(&s,A,"!next");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==2&&strstr(r.msg,"-> R1M1B"));
 /* An INTRO world (R1M0) leaves through its intro script while it runs;
  * a gameplay world refuses exits during its intro. */
 {CoopSession i;lobby(&i);cmd(&i,A,"!ready");cmd(&i,B,"!ready");cmd(&i,A,"!start");
  load(&i,"worlds/realm1/R1M0",2000);coop_session_intro_arm(&i,1);
  assert(i.intro_armed&&(i.route[0].flags&COOP_W_INTRO_OK));
  spawn(&i,A,"Alice",2100);spawn(&i,B,"Bob",2100);
  assert(coop_session_intro_tick(&i,2100+COOP_INTRO_SETTLE_MS,&r)==COOP_ACT_INTRO_START);
  coop_session_exit(&i,"worlds/realm1/RIM1a",80000,&r);assert(r.action!=COOP_ACT_CHANGE_WORLD);
  i.reject_reply_epoch=0;
  coop_session_exit(&i,"Worlds/Realm1/R1M1a",81000,&r);assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==1);
  i.route_pos=6;i.phase=COOP_PHASE_INTRO_RUNNING;i.transition_epoch=0;     /* R1M3A: not an INTRO world */
  coop_session_exit(&i,"worlds/realm1/r1m3b",82000,&r);assert(r.action!=COOP_ACT_CHANGE_WORLD);}
 /* The realm boundary inside a full campaign is an ordinary transition. */
 s.route_pos=11;s.phase=COOP_PHASE_GAMEPLAY;s.transition_epoch=0;
 coop_session_exit(&s,"Worlds/Realm2/R2M0",9000,&r);assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==12);
}
static void test_realm_completion_and_abort(void){
 CoopSession s;
 lobby(&s);
 cmd(&s,A,"!campaign realm water");
 cmd(&s,A,"!ready");cmd(&s,B,"!ready");cmd(&s,A,"!start");
 load(&s,"worlds\\realm1\\R1M0",2000);spawn(&s,A,"Alice",2100);spawn(&s,B,"Bob",2100);
 /* Jump to the realm's last world (R1M4C) and leave it toward R2M0. */
 s.route_pos=11;assert(!strcmp(coop_route_label(&s,11),"R1M4C"));
 coop_session_exit(&s,"worlds/realm2/r1m4c",9000,&r);assert(r.action!=COOP_ACT_CHANGE_WORLD);   /* not the successor */
 s.reject_reply_epoch=0;
 coop_session_exit(&s,"worlds/realm2/r2m0",9000,&r);
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"route complete -> lobby R1M0"));
 assert(s.completed==1&&s.lobby_pending);
 load(&s,"worlds\\realm1\\R1M0",10000);
 assert(s.phase==COOP_PHASE_LOBBY&&!s.started&&s.generation==2&&coop_present_count(&s)==2&&coop_ready_count(&s)==0);
 assert(s.sel_mode==COOP_SEL_REALM&&s.sel_realm==1);                          /* selection persists */
 /* Abort: host only, only in play. */
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Nothing to end"));
 cmd(&s,A,"!ready");cmd(&s,B,"!ready");cmd(&s,A,"!start");
 load(&s,"worlds\\realm1\\R1M0",20000);spawn(&s,A,"Alice",20100);
 cmd(&s,B,"!abort");assert(strstr(r.msg,"Only the host"));
 memcpy(s.lobby_world,"deathmatch/pm01",16);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"lobby PM01"));
 assert(!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"deathmatch/pm01"));
 load(&s,"worlds\\deathmatch\\PM01",21000);assert(s.phase==COOP_PHASE_LOBBY&&s.generation==3&&s.completed==1);
 cmd(&s,A,"!status");assert(strstr(r.msg,"LOBBY PM01"));
 /* The final campaign world completes on any valid exit. */
 cmd(&s,A,"!campaign realm nightmare");cmd(&s,A,"!ready");cmd(&s,B,"!ready");cmd(&s,A,"!start");
 assert(strstr(r.msg,"-> R5M1A"));
 load(&s,"worlds\\realm5\\R5M1A",30000);spawn(&s,A,"Alice",30100);
 coop_session_exit(&s,"worlds/realm5/r5m1b",31000,&r);assert(r.target_pos==1);
 load(&s,"worlds\\realm5\\R5M1B",32000);spawn(&s,A,"Alice",32100);
 coop_session_exit(&s,"worlds/realm1/r1m0",33000,&r);assert(r.target_pos==COOP_TARGET_LOBBY&&s.completed==2);
}
static void test_test_start_and_legacy(void){
 CoopSession s;
 lobby(&s);s.test_label=coop_campaign_worlds[6].label;coop_resolve_test_start(&s);
 assert(s.start_pos==6);                                                      /* R1M3A in the full campaign */
 cmd(&s,A,"!campaign realm earth");assert(s.start_pos==0);                    /* not in the selection: start at R2M0 */
 cmd(&s,A,"!campaign from water");assert(s.start_pos==6);
 /* Legacy MVP route: unchanged commands and replies. */
 coop_session_init(&s,1,3);spawn(&s,A,"Alice",0);
 assert(s.route_len==3&&!s.campaign);
 cmd(&s,A,"!campaign");assert(strstr(r.msg,"Campaign selection is off"));
 cmd(&s,A,"!campaign realm fire");assert(strstr(r.msg,"Campaign selection is off")&&s.route_len==3);
 cmd(&s,A,"!abort");assert(strstr(r.msg,"CoopCampaign=1"));
 cmd(&s,A,"!help");assert(!strstr(r.msg,"!campaign"));
}
int main(void){
 test_table();test_selection_commands();test_full_route_and_exits();test_realm_completion_and_abort();test_test_start_and_legacy();
 printf("PASS coop campaign graph: WORLDORDER table, selection commands, exits, realm completion, abort, lobby return, test start, legacy route\n");
 return 0;
}
