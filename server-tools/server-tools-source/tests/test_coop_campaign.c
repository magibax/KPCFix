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
 cmd(&s,B,"!campaign");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"full campaign (R1M0..R5M1B, 48 worlds)")&&strstr(r.msg,"!campaign full|water"));
 assert(strlen(r.msg)<COOP_MSG_MAX);
 cmd(&s,B,"!campaign fire");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host"));
 /* Malformed or unknown words: the help reply, never silence or a change. */
 cmd(&s,A,"!campaign earth realm fire");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Use: !campaign full")&&s.phase==COOP_PHASE_LOBBY);
 cmd(&s,A,"!campaign narnia");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Use:"));
 cmd(&s,A,"!campaign realm fire");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Use:"));    /* realm-only form removed */
 cmd(&s,A,"!campaign from narnia");assert(strstr(r.msg,"Use:"));
 cmd(&s,A,"!campaign earth, fire");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Use:"));
 assert(strlen(r.msg)<COOP_MSG_MAX&&s.sel_mode==COOP_SEL_FULL);
 cmd(&s,A,"!ready now");assert(r.action==COOP_ACT_NONE&&coop_ready_count(&s)==0); /* arguments only for !campaign */
 cmd(&s,A,"!help");assert(strstr(r.msg,"!campaign <realm|full>")&&strstr(r.msg,"!goto !back")&&strlen(r.msg)<COOP_MSG_MAX);
 /* Minimum players applies; readiness does not. */
 {CoopSession one;coop_session_init(&one,2,3);coop_session_enable_campaign(&one);spawn(&one,A,"Alice",0);
  cmd(&one,A,"!campaign earth");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Need 2 players")&&one.phase==COOP_PHASE_LOBBY&&!one.started);}
 /* !campaign <realm> starts now from that realm's first world and carries on to the end. */
 cmd(&s,A,"!ready");assert(coop_ready_count(&s)==1);
 cmd(&s,A,"!campaign  Earth");
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==0&&strstr(r.msg,"Host started from Earth (R2M0..R5M1B, 36 worlds) -> R2M0"));
 assert(strlen(r.msg)<COOP_MSG_MAX&&s.started&&s.start_pending&&s.phase==COOP_PHASE_TRANSITION&&coop_ready_count(&s)==0);
 assert(!strcmp(coop_target_world(&s,0),"realm2/r2m0"));
 cmd(&s,A,"!campaign fire");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Already playing"));
 /* Aliases and the full playthrough. */
 {CoopSession t;lobby(&t);cmd(&t,A,"!campaign from 4");assert(r.action==COOP_ACT_CHANGE_WORLD&&strstr(r.msg,"from Fire (R4M0..R5M1B")&&strstr(r.msg,"-> R4M0"));}
 {CoopSession t;lobby(&t);cmd(&t,A,"!campaign demon");assert(strstr(r.msg,"-> R4M0"));}
 {CoopSession t;lobby(&t);cmd(&t,A,"!campaign nightmare");assert(strstr(r.msg,"from Nightmare (R5M1A..R5M1B, 2 worlds) -> R5M1A"));}
 {CoopSession t;lobby(&t);cmd(&t,A,"!campaign water");assert(t.sel_mode==COOP_SEL_FULL&&strstr(r.msg,"full campaign (R1M0..R5M1B, 48 worlds) -> R1M0"));}
 {CoopSession t;lobby(&t);cmd(&t,A,"!campaign full");assert(r.action==COOP_ACT_CHANGE_WORLD&&t.route_len==48&&strstr(r.msg,"-> R1M0"));
  /* A joining member does not change the selection. */
  spawn(&t,0x300u,"Carol",10);assert(t.sel_mode==COOP_SEL_FULL);}
 /* !start still runs the current selection with everyone ready. */
 {CoopSession t;lobby(&t);cmd(&t,A,"!ready");cmd(&t,B,"!ready");cmd(&t,A,"!start");assert(r.action==COOP_ACT_CHANGE_WORLD&&strstr(r.msg,"-> R1M0"));}
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
 /* A configured settle (campaign default 6 s, longer than the clients' 5 s
  * loading picture) holds the intro start accordingly. */
 {CoopSession i;lobby(&i);assert(i.intro_settle_ms==COOP_INTRO_SETTLE_MS);i.intro_settle_ms=6000;
  cmd(&i,A,"!ready");cmd(&i,B,"!ready");cmd(&i,A,"!start");
  load(&i,"worlds/realm1/R1M0",2000);coop_session_intro_arm(&i,1);
  spawn(&i,A,"Alice",2100);spawn(&i,B,"Bob",2200);
  assert(coop_session_intro_tick(&i,2200+5999,&r)==0);
  assert(coop_session_intro_tick(&i,2200+6000,&r)==COOP_ACT_INTRO_START);}
 /* The realm boundary inside a full campaign is an ordinary transition. */
 s.route_pos=11;s.phase=COOP_PHASE_GAMEPLAY;s.transition_epoch=0;
 coop_session_exit(&s,"Worlds/Realm2/R2M0",9000,&r);assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==12);
}
static void test_realm_completion_and_abort(void){
 CoopSession s;
 lobby(&s);
 /* A one-realm slice (no longer a chat command) completes at the realm boundary. */
 assert(coop_session_select(&s,COOP_SEL_REALM,1));
 cmd(&s,A,"!ready");cmd(&s,B,"!ready");cmd(&s,A,"!start");
 load(&s,"worlds\\realm1\\R1M0",2000);spawn(&s,A,"Alice",2100);spawn(&s,B,"Bob",2100);
 /* Jump to the realm's last world (R1M4C) and leave it toward R2M0. */
 s.route_pos=11;assert(!strcmp(coop_route_label(&s,11),"R1M4C"));
 coop_session_exit(&s,"worlds/realm2/r1m4c",9000,&r);assert(r.action!=COOP_ACT_CHANGE_WORLD);   /* not the successor */
 s.reject_reply_epoch=0;
 coop_session_exit(&s,"worlds/realm2/r2m0",9000,&r);
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"route complete -> lobby PM01"));
 assert(s.completed==1&&s.lobby_pending&&!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"deathmatch/pm01"));
 load(&s,"worlds\\deathmatch\\PM01",10000);
 assert(s.phase==COOP_PHASE_LOBBY&&!s.started&&s.generation==2&&coop_present_count(&s)==2&&coop_ready_count(&s)==0);
 assert(s.sel_mode==COOP_SEL_REALM&&s.sel_realm==1);                          /* selection persists */
 /* Abort: host only; (a campaign never uses a campaign world as its lobby: random PM01-PM10, seed = time); from the lobby world it resets the lobby in place. */
 cmd(&s,A,"!ready");coop_session_select(&s,COOP_SEL_FROM,3);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"Lobby reset (PM01)"));
 assert(coop_ready_count(&s)==0&&s.sel_mode==COOP_SEL_FULL&&s.phase==COOP_PHASE_LOBBY);
 cmd(&s,A,"!campaign earth");assert(r.action==COOP_ACT_CHANGE_WORLD);
 load(&s,"worlds\\realm2\\R2M0",20000);spawn(&s,A,"Alice",20100);
 cmd(&s,B,"!abort");assert(strstr(r.msg,"Only the host"));
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"Host aborted -> lobby PM01"));
 load(&s,"worlds\\deathmatch\\PM01",20500);assert(s.phase==COOP_PHASE_LOBBY);
 cmd(&s,A,"!status");assert(strstr(r.msg,"LOBBY PM01"));
 /* CoopLobbyWorld still overrides the default. */
 cmd(&s,A,"!campaign full");load(&s,"worlds\\realm1\\R1M0",20600);spawn(&s,A,"Alice",20700);
 memcpy(s.lobby_world,"deathmatch/pm01",16);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"lobby PM01"));
 assert(!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"deathmatch/pm01"));
 load(&s,"worlds\\deathmatch\\PM01",21000);assert(s.phase==COOP_PHASE_LOBBY&&s.completed==1);
 cmd(&s,A,"!status");assert(strstr(r.msg,"LOBBY PM01"));
 /* The final campaign world completes on any valid exit. */
 cmd(&s,A,"!campaign nightmare");assert(strstr(r.msg,"-> R5M1A"));
 load(&s,"worlds\\realm5\\R5M1A",30000);spawn(&s,A,"Alice",30100);
 coop_session_exit(&s,"worlds/realm5/r5m1b",31000,&r);assert(r.target_pos==1);
 load(&s,"worlds\\realm5\\R5M1B",32000);spawn(&s,A,"Alice",32100);
 coop_session_exit(&s,"worlds/realm1/r1m0",33000,&r);assert(r.target_pos==COOP_TARGET_LOBBY&&s.completed==2);
}
static void test_test_start_and_legacy(void){
 CoopSession s;
 lobby(&s);s.test_label=coop_campaign_worlds[6].label;coop_resolve_test_start(&s);
 assert(s.start_pos==6);                                                      /* R1M3A in the full campaign */
 cmd(&s,A,"!campaign earth");assert(s.start_pos==0&&strstr(r.msg,"-> R2M0"));  /* not in the selection: start at R2M0 */
 lobby(&s);s.test_label=coop_campaign_worlds[6].label;
 cmd(&s,A,"!campaign water");assert(s.start_pos==6&&strstr(r.msg,"-> R1M3A"));
 /* Legacy MVP route: !campaign is off, !abort now works and returns to R1M2B. */
 coop_session_init(&s,1,3);spawn(&s,A,"Alice",0);
 assert(s.route_len==3&&!s.campaign);
 cmd(&s,A,"!campaign");assert(strstr(r.msg,"needs CoopCampaign=1"));
 cmd(&s,A,"!campaign fire");assert(strstr(r.msg,"needs CoopCampaign=1")&&s.route_len==3&&s.phase==COOP_PHASE_LOBBY);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"Lobby reset (R1M2B)")&&s.phase==COOP_PHASE_LOBBY); /* launcher-started lobby */
 cmd(&s,A,"!ready");cmd(&s,A,"!start");assert(strstr(r.msg,"-> R1M2B"));
 load(&s,"worlds\\realm1\\R1M2B",1000);spawn(&s,A,"Alice",1100);
 cmd(&s,A,"!next");load(&s,"worlds\\realm1\\R1M2C",2000);spawn(&s,A,"Alice",2100);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"lobby R1M2B"));
 load(&s,"worlds\\realm1\\R1M2B",3000);assert(s.phase==COOP_PHASE_LOBBY&&!s.started);
 cmd(&s,A,"!help");assert(!strstr(r.msg,"!campaign")&&strstr(r.msg,"!abort"));
}
/* !abort resets everything back to the lobby from any state. */
static void test_abort_any_state(void){
 CoopSession s;
 /* In play: back to a deathmatch lobby, readiness cleared, selection back to full. */
 lobby(&s);cmd(&s,A,"!campaign fire");load(&s,"worlds\\realm4\\R4M0",1000);spawn(&s,A,"Alice",1100);
 assert(s.sel_mode==COOP_SEL_FROM&&s.sel_realm==4);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"Host aborted -> lobby PM01"));
 assert(s.sel_mode==COOP_SEL_FULL&&s.lobby_pending);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Already returning"));
 load(&s,"worlds\\deathmatch\\PM01",2000);assert(s.phase==COOP_PHASE_LOBBY&&!s.started);
 {CoopResult a;coop_session_after_load(&s,2100,&a);assert(a.action==COOP_ACT_NONE);}
 /* During a world change (here the !start load): deferred until it has loaded. */
 lobby(&s);cmd(&s,A,"!campaign earth");assert(s.phase==COOP_PHASE_TRANSITION);
 cmd(&s,B,"!abort");assert(strstr(r.msg,"Only the host"));
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"once this world has loaded")&&s.abort_after_load);
 load(&s,"worlds\\realm2\\R2M0",3000);
 {CoopResult a;coop_session_after_load(&s,3100,&a);
  assert(a.action==COOP_ACT_CHANGE_WORLD&&a.target_pos==COOP_TARGET_LOBBY&&strstr(a.msg,"lobby PM01")&&!s.abort_after_load);}
 load(&s,"worlds\\deathmatch\\PM01",4000);assert(s.phase==COOP_PHASE_LOBBY&&!s.started&&s.sel_mode==COOP_SEL_FULL);
 /* Nothing pending: after_load does nothing. */
 {CoopResult a;coop_session_after_load(&s,4100,&a);assert(a.action==COOP_ACT_NONE);}
 /* The campaign can start again straight away. */
 cmd(&s,A,"!campaign water");assert(r.action==COOP_ACT_CHANGE_WORLD&&strstr(r.msg,"-> R1M0"));
}
/* !difficulty (host, lobby) and host-only exits during play. */
static void test_difficulty_and_host_exits(void){
 CoopSession s;CoopResult x;
 lobby(&s);assert(s.skill==-1);
 cmd(&s,B,"!difficulty");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"server default"));
 cmd(&s,B,"!difficulty nightmare");assert(strstr(r.msg,"Only the host")&&s.skill==-1);
 cmd(&s,A,"!difficulty  Cold Sweat");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"Cold Sweat")&&s.skill==3);
 cmd(&s,A,"!difficulty 0");assert(s.skill==0&&strstr(r.msg,"Lullaby"));
 cmd(&s,A,"!skill hard");assert(s.skill==3);
 cmd(&s,A,"!difficulty impossible");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Use:")&&s.skill==3);
 cmd(&s,A,"!difficulty");assert(strstr(r.msg,"Cold Sweat")&&strlen(r.msg)<COOP_MSG_MAX);
 cmd(&s,A,"!campaign water");load(&s,"worlds\\realm1\\R1M0",1000);spawn(&s,A,"Alice",1100);
 cmd(&s,A,"!difficulty lullaby");assert(strstr(r.msg,"only be changed in the lobby")&&s.skill==3);
 /* Exits: outside GAMEPLAY (intro running, R1M0's scripted exit) any activator passes on. */
 s.phase=COOP_PHASE_INTRO_RUNNING;assert(coop_session_exit_by(&s,1,1,1000,&x)&&x.action==COOP_ACT_NONE);
 s.phase=COOP_PHASE_GAMEPLAY;
 assert(coop_session_exit_by(&s,0,1,1000,&x));                                  /* host (Alice) */
 assert(coop_session_exit_by(&s,-1,0,1000,&x));                                 /* a script/object */
 assert(!coop_session_exit_by(&s,1,1,1000,&x)&&x.action==COOP_ACT_ANNOUNCE&&strstr(x.msg,"Only the host"));
 assert(!coop_session_exit_by(&s,1,1,1000+COOP_HOST_EXIT_REPLY_MS-1,&x)&&x.action==COOP_ACT_NONE);   /* rate limited */
 assert(!coop_session_exit_by(&s,1,1,1000+COOP_HOST_EXIT_REPLY_MS,&x)&&x.action==COOP_ACT_ANNOUNCE); /* said again */
 {unsigned int e=s.epoch;s.epoch++;                                         /* a new world replies at once */
  assert(!coop_session_exit_by(&s,1,1,1001+COOP_HOST_EXIT_REPLY_MS,&x)&&x.action==COOP_ACT_ANNOUNCE);s.epoch=e;}
 assert(!coop_session_exit_by(&s,-1,1,1002+COOP_HOST_EXIT_REPLY_MS,&x));                                /* a player outside the session */
 /* Campaign default: the intro starts with the last spawn (no settle). */
 assert(COOP_CAMPAIGN_SETTLE_MS==0u);
}
/* Host-only level-ending sequences and the per-world exit names. */
static void test_host_sequences_and_exit_names(void){
 CoopSession s;CoopResult x;unsigned int i,k;
 assert(coop_is_exit_name("R1M1A","CExitTrigger0")&&coop_is_exit_name("r1m1a"," cexittrigger0 "));
 assert(coop_is_exit_name("R1M1B","Gubba")&&!coop_is_exit_name("R1M1B","CExitTrigger0"));
 assert(coop_is_exit_name("R3M1A","sloopy")&&!coop_is_exit_name("R3M1A","sloopy2"));
 assert(!coop_is_exit_name("R1M1A","")&&!coop_is_exit_name("","exit")&&!coop_is_exit_name("PM01","exit"));
 assert(!coop_is_exit_name(0,"exit")&&!coop_is_exit_name("R1M2C",0));
 /* Every campaign world has exactly one exit name. */
 for(i=0;i<COOP_CAMPAIGN_LEN;i++){
  unsigned int found=0;
  for(k=0;k<COOP_EXIT_NAMES;k++)if(coop_ieq(coop_exit_names[k][0],coop_campaign_worlds[i].label))found++;
  assert(found==1u);
 }
 lobby(&s);cmd(&s,A,"!campaign water");load(&s,"worlds\\realm1\\R1M1A",1000);spawn(&s,A,"Alice",1100);
 s.phase=COOP_PHASE_INTRO_RUNNING;assert(coop_session_sequence_by(&s,1,2000,&x)&&x.action==COOP_ACT_NONE);
 s.phase=COOP_PHASE_GAMEPLAY;
 assert(coop_session_sequence_by(&s,0,2000,&x)&&x.action==COOP_ACT_NONE);           /* host */
 assert(!coop_session_sequence_by(&s,1,2000,&x)&&x.action==COOP_ACT_ANNOUNCE&&strstr(x.msg,"Only the host"));
 assert(!coop_session_sequence_by(&s,1,2000+COOP_HOST_SEQ_REPLY_MS-1,&x)&&x.action==COOP_ACT_NONE);
 assert(!coop_session_sequence_by(&s,-1,2000+COOP_HOST_SEQ_REPLY_MS,&x)&&x.action==COOP_ACT_ANNOUNCE);
 assert(s.rejected_sequences==3u&&strlen(x.msg)<COOP_MSG_MAX);
}
/* !changelevel <world>: host only, campaign worlds by label or path, from the
 * lobby or during play; the campaign carries on from that world. */
static void test_changelevel(void){
 CoopSession s;CoopResult x;
 assert(coop_campaign_find("R1M2A")==3&&coop_campaign_find("r1m2a")==3&&coop_campaign_find(" R1M2A ")==3);
 assert(coop_campaign_find("worlds\\realm1\\R1M2A")==3&&coop_campaign_find("worlds/realm1/r1m2a.dat")==3);
 assert(coop_campaign_find("realm1\\R1M2A")==3&&coop_campaign_find(".\\worlds\\realm1\\R1M2A")==3);
 assert(coop_campaign_find("R1M0")==0&&coop_campaign_find("R5M1B")==47&&coop_campaign_find("realm4/r4m1a")==36);
 assert(coop_campaign_find("R1M2D")<0&&coop_campaign_find("PM01")<0&&coop_campaign_find("worlds\\deathmatch\\PM01")<0);
 assert(coop_campaign_find("realm2/r1m2a")<0&&coop_campaign_find("R1M2A x")<0&&coop_campaign_find("")<0&&coop_campaign_find(0)<0);
 /* Lobby: the host jumps straight in (no !ready); the route continues from there. */
 lobby(&s);
 cmd(&s,B,"!changelevel R1M2A");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host")&&s.phase==COOP_PHASE_LOBBY);
 cmd(&s,A,"!changelevel");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Use: !changelevel"));
 cmd(&s,A,"!changelevel R9M9Z");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Unknown world")&&s.phase==COOP_PHASE_LOBBY);
 cmd(&s,A,"!ChangeLevel worlds\\realm1\\R1M2A");
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==0&&strstr(r.msg,"-> R1M2A")&&strlen(r.msg)<COOP_MSG_MAX);
 assert(s.started&&s.start_pending&&s.sel_mode==COOP_SEL_WORLD&&!strcmp(coop_route_label(&s,0),"R1M2A")&&s.route_len==45);
 assert(!strcmp(coop_target_world(&s,0),"realm1/r1m2a"));
 cmd(&s,A,"!changelevel R1M3A");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"already in progress"));
 load(&s,"worlds\\realm1\\R1M2A",1000);assert(s.phase==COOP_PHASE_INTRO_PENDING&&s.route_pos==0);
 spawn(&s,A,"Alice",1100);assert(s.phase==COOP_PHASE_GAMEPLAY);
 /* A natural exit leads to the WORLDORDER successor. */
 coop_session_exit(&s,"worlds/realm1/r1m2b",1200,&x);assert(x.action==COOP_ACT_CHANGE_WORLD&&x.target_pos==1&&strstr(x.msg,"R1M2B"));
 load(&s,"worlds\\realm1\\R1M2B",1300);spawn(&s,A,"Alice",1400);
 /* During play: to another realm, the campaign then continues from there. */
 cmd(&s,A,"!changelevel r4m1a");assert(r.action==COOP_ACT_CHANGE_WORLD&&strstr(r.msg,"-> R4M1A")&&!s.start_pending);
 {char sel[COOP_MSG_MAX];unsigned int n=0;sel[0]=0;coop_selection_text(&s,sel,&n);assert(strstr(sel,"from R4M1A (R4M1A..R5M1B, 12 worlds)"));}
 load(&s,"worlds\\realm4\\R4M1A",1500);spawn(&s,A,"Alice",1600);assert(s.phase==COOP_PHASE_GAMEPLAY&&s.route_pos==0);
 coop_session_exit(&s,"worlds/realm4/r4m1b",1700,&x);assert(x.action==COOP_ACT_CHANGE_WORLD&&x.target_pos==1);
 load(&s,"worlds\\realm4\\R4M1B",1800);spawn(&s,A,"Alice",1900);
 /* !abort: back to the lobby and the default selection. */
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&s.sel_mode==COOP_SEL_FULL);
 load(&s,"worlds\\realm1\\R1M2B",2000);assert(s.phase==COOP_PHASE_LOBBY&&!s.started);
 /* Below the minimum player count in the lobby: refused. */
 coop_session_init(&s,2,3);coop_session_enable_campaign(&s);spawn(&s,A,"Alice",0);
 cmd(&s,A,"!changelevel R1M2A");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Need 2 players")&&s.phase==COOP_PHASE_LOBBY);
 /* MVP route (CoopCampaign=0): not available. */
 coop_session_init(&s,1,3);spawn(&s,A,"Alice",0);
 cmd(&s,A,"!changelevel R1M2A");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"needs CoopCampaign=1"));
}
/* Lobby pick (campaign without a deathmatch CoopLobbyWorld) and the
 * dedicated console's "changelevel" (server authority). */
static void test_lobby_pick_and_console(void){
 CoopSession s;CoopResult x;
 lobby(&s);
 strcpy(s.world_label,"R2M0");coop_session_pick_lobby(&s,7);assert(!strcmp(s.lobby_pick,"deathmatch/pm08"));
 coop_session_pick_lobby(&s,19);assert(!strcmp(s.lobby_pick,"deathmatch/pm10"));
 strcpy(s.world_label,"PM08");coop_session_pick_lobby(&s,7);assert(!strcmp(s.lobby_pick,"deathmatch/pm09"));   /* never the loaded one */
 /* A campaign world as CoopLobbyWorld is not used; a deathmatch one is fixed. */
 strcpy(s.lobby_world,"realm1/r1m2b");assert(!coop_lobby_fixed(&s));
 assert(!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"deathmatch/pm09"));
 strcpy(s.lobby_world,"deathmatch/pm03");assert(coop_lobby_fixed(&s));
 coop_session_pick_lobby(&s,1);assert(!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"deathmatch/pm03"));
 /* MVP route keeps its lobby. */
 coop_session_init(&s,1,3);coop_session_pick_lobby(&s,5);assert(!s.lobby_pick[0]&&!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"realm1/r1m2b"));
 /* Console: no host and no player minimum needed. */
 coop_session_init(&s,2,3);coop_session_enable_campaign(&s);
 coop_session_console_changelevel(&s,"",1000,&x);assert(x.action==COOP_ACT_REPLY&&strstr(x.msg,"Use: changelevel"));
 coop_session_console_changelevel(&s,"R9M9Z",1000,&x);assert(x.action==COOP_ACT_REPLY&&strstr(x.msg,"Unknown world"));
 coop_session_console_changelevel(&s,"worlds\\realm1\\R1M2A",1000,&x);
 assert(x.action==COOP_ACT_CHANGE_WORLD&&strstr(x.msg,"Server changed level")&&strstr(x.msg,"-> R1M2A")&&s.start_pending);
 coop_session_console_changelevel(&s,"R1M3A",1000,&x);assert(x.action==COOP_ACT_REPLY&&strstr(x.msg,"already in progress"));
 load(&s,"worlds\\realm1\\R1M2A",2000);spawn(&s,A,"Alice",2100);
 coop_session_console_changelevel(&s,"r4m1a",3000,&x);assert(x.action==COOP_ACT_CHANGE_WORLD&&strstr(x.msg,"-> R4M1A")&&!s.start_pending);
 /* The help lists !goto and !back. */
 cmd(&s,A,"!help");assert(strstr(r.msg,"!goto !back"));
 /* Chat !changelevel still needs the host. */
 lobby(&s);cmd(&s,B,"!changelevel R1M2A");assert(strstr(r.msg,"Only the host"));
}
/* !changelevel to any shipped map: the tutorial (one-world route) and the
 * deathmatch maps (they become the lobby); !reset and !coop off (host
 * actions for the proxy); everyone dead reloads the world. */
static void test_any_map_reset_and_dead(void){
 CoopSession s;CoopResult x;
 assert(coop_extra_find("TUTORIAL")==0&&coop_extra_find("worlds\\realm0\\tutorial.dat")==0&&coop_extra_find("R1M0")<0);
 assert(coop_deathmatch_find("PM05")==5&&coop_deathmatch_find("pm5")==5&&coop_deathmatch_find("worlds/deathmatch/PM10")==10);
 assert(coop_deathmatch_find("deathmatch/pm01")==1&&!coop_deathmatch_find("PM11")&&!coop_deathmatch_find("PM00")&&!coop_deathmatch_find("PM")&&!coop_deathmatch_find("PM05x"));
 assert(!coop_deathmatch_find("R1M2A")&&!coop_deathmatch_find("")&&!coop_deathmatch_find(0));
 /* Tutorial: started from the lobby as a one-world route; its exit completes it. */
 lobby(&s);
 cmd(&s,A,"!changelevel tutorial");
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==0&&strstr(r.msg,"(tutorial) -> TUTORIAL")&&s.extra_route&&s.route_len==1);
 assert(!strcmp(coop_target_world(&s,0),"realm0/tutorial")&&coop_campaign_index(&s,0)<0&&s.started);
 load(&s,"worlds/realm0/TUTORIAL",1000);assert(s.phase==COOP_PHASE_INTRO_PENDING);
 spawn(&s,A,"Alice",1100);assert(s.phase==COOP_PHASE_GAMEPLAY);
 coop_session_exit(&s,"worlds/realm1/r1m0",1200,&x);assert(x.action==COOP_ACT_CHANGE_WORLD&&x.target_pos==COOP_TARGET_LOBBY);
 load(&s,"worlds/realm1/R1M2B",1300);assert(s.phase==COOP_PHASE_LOBBY&&!s.extra_route&&s.sel_mode==COOP_SEL_FULL);
 /* A campaign world after the tutorial is a normal campaign slice again. */
 cmd(&s,A,"!changelevel R1M2A");assert(r.action==COOP_ACT_CHANGE_WORLD&&!s.extra_route&&s.route_len==45);
 load(&s,"worlds/realm1/R1M2A",1400);spawn(&s,A,"Alice",1500);assert(s.phase==COOP_PHASE_GAMEPLAY);
 /* Deathmatch map during play: back to the lobby state there; it stays the lobby. */
 cmd(&s,B,"!changelevel PM05");assert(strstr(r.msg,"Only the host"));
 cmd(&s,A,"!changelevel worlds/deathmatch/PM05");
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==COOP_TARGET_LOBBY&&strstr(r.msg,"lobby PM05")&&!strcmp(coop_target_world(&s,COOP_TARGET_LOBBY),"deathmatch/pm05"));
 load(&s,"worlds/deathmatch/PM05",1600);assert(s.phase==COOP_PHASE_LOBBY&&!s.started&&coop_in_lobby_world(&s));
 /* No player minimum for a deathmatch map (one player, minimum 2). */
 coop_session_init(&s,2,3);coop_session_enable_campaign(&s);spawn(&s,A,"Alice",0);
 cmd(&s,A,"!changelevel PM02");assert(r.action==COOP_ACT_CHANGE_WORLD&&strstr(r.msg,"lobby PM02"));
 cmd(&s,A,"!changelevel PM03");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"already in progress"));
 load(&s,"worlds/deathmatch/PM02",100);
 /* In that lobby, !abort resets the lobby in place (it is the lobby world). */
 strcpy(s.lobby_world,"deathmatch/pm08");
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"Lobby reset (PM02"));
 /* During play, !abort clears the forced lobby and returns to the normal one. */
 cmd(&s,A,"!changelevel R1M2A");load(&s,"worlds/realm1/R1M2A",200);spawn(&s,A,"Alice",300);
 cmd(&s,A,"!abort");assert(r.action==COOP_ACT_CHANGE_WORLD&&!s.lobby_force[0]&&strstr(r.msg,"lobby PM08"));
 /* Console: the tutorial and deathmatch maps too. */
 coop_session_init(&s,2,3);coop_session_enable_campaign(&s);
 coop_session_console_changelevel(&s,"PM07",1000,&x);assert(x.action==COOP_ACT_CHANGE_WORLD&&strstr(x.msg,"Server changed level -> lobby PM07"));
 load(&s,"worlds/deathmatch/PM07",2000);
 coop_session_console_changelevel(&s,"TUTORIAL",3000,&x);assert(x.action==COOP_ACT_CHANGE_WORLD&&strstr(x.msg,"-> TUTORIAL"));
 /* !reset and !coop off: host only; the proxy carries them out. */
 lobby(&s);
 cmd(&s,B,"!reset");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host"));
 cmd(&s,A,"!reset");assert(r.action==COOP_ACT_RESET&&strstr(r.msg,"start state"));
 cmd(&s,A,"!coop on");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"already on"));
 cmd(&s,B,"!coop off");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host"));
 cmd(&s,A,"!coop off");assert(r.action==COOP_ACT_SESSION_OFF);
 cmd(&s,A,"!coop maybe");assert(r.action==COOP_ACT_NONE);
 cmd(&s,A,"!coop");assert(r.action==COOP_ACT_REPLY);
 cmd(&s,A,"!campaign water");assert(r.action==COOP_ACT_CHANGE_WORLD);            /* arguments still work */
 /* Everyone dead for 2 s during play: the world restarts (same route position). */
 lobby(&s);cmd(&s,A,"!changelevel R1M3A");load(&s,"worlds/realm1/R1M3A",1000);
 spawn(&s,A,"Alice",1100);spawn(&s,B,"Bob",1100);assert(s.phase==COOP_PHASE_GAMEPLAY);
 assert(!coop_session_dead_check(&s,2,1,5000,&x)&&!s.dead_since);              /* one alive */
 assert(!coop_session_dead_check(&s,2,2,6000,&x)&&s.dead_since);               /* both dead: wait */
 assert(!coop_session_dead_check(&s,2,1,7000,&x)&&!s.dead_since);              /* one respawned: reset */
 assert(!coop_session_dead_check(&s,2,2,8000,&x));
 assert(!coop_session_dead_check(&s,2,2,9999,&x));
 assert(coop_session_dead_check(&s,2,2,10002,&x)&&x.action==COOP_ACT_CHANGE_WORLD&&x.target_pos==s.route_pos);
 assert(strstr(x.msg,"Everyone died: restarting R1M3A")&&s.restarts==1&&s.phase==COOP_PHASE_TRANSITION);
 load(&s,"worlds/realm1/R1M3A",11000);assert(s.phase==COOP_PHASE_INTRO_PENDING&&s.started);   /* intro again */
 /* Not in the lobby, not with nobody present. */
 lobby(&s);assert(!coop_session_dead_check(&s,2,2,1000,&x)&&!coop_session_dead_check(&s,2,2,9000,&x));
 assert(!coop_session_dead_check(&s,0,0,9000,&x));
}
int main(void){
 test_any_map_reset_and_dead();
 test_lobby_pick_and_console();
 test_changelevel();
 test_difficulty_and_host_exits();
 test_host_sequences_and_exit_names();
 test_abort_any_state();
 test_table();test_selection_commands();test_full_route_and_exits();test_realm_completion_and_abort();test_test_start_and_legacy();
 printf("PASS coop campaign graph: WORLDORDER table, selection commands, exits, realm completion, abort, lobby return, deathmatch lobby pick, console changelevel, test start, legacy route\n");
 return 0;
}
