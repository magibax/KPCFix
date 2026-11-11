/* Asset-free tests for the co-op lobby/session state machine (kpc-coop/1). */
#include "coop_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define A 0x100u
#define B 0x200u
#define C 0x300u
#define D 0x400u

static CoopResult r;

static int spawn(CoopSession *s,unsigned int client,const char *name,unsigned int now){
 return coop_session_seen(s,client,client+1,client+2,name,now,&r);
}
static void cmd(CoopSession *s,unsigned int client,const char *text){
 coop_session_command(s,client,text,1000,&r);
}
static void live_all(CoopSession *s,unsigned int now){
 unsigned int live[COOP_SESSION_MAX_MEMBERS],n=0,i;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(s->m[i].used&&s->m[i].spawned)live[n++]=s->m[i].object;
 coop_session_presence(s,live,n,now,&r);
}
/* Simulate the native world change that the proxy performs for CHANGE_WORLD. */
static void native_change(CoopSession *s,const char *world,unsigned int now){
 coop_session_world_loading(s,now);
 coop_session_world_loaded(s,world);
}

static void test_normalize(void){
 char out[COOP_WORLD_MAX];
 assert(coop_world_normalize("worlds\\realm1\\R1M2B",out)&&!strcmp(out,"realm1/r1m2b"));
 assert(coop_world_normalize("./Worlds/Realm1/R1M2C.dat",out)&&!strcmp(out,"realm1/r1m2c"));
 assert(coop_world_equal("realm1\\R1M2C","worlds/realm1/r1m2c"));
 assert(!coop_world_equal("worlds\\realm1\\R1M2D","realm1/r1m2c"));
 assert(!coop_world_normalize("",out));
 assert(!coop_world_normalize("worlds\\realm1\\R1 M2B",out)); /* space rejected */
 {char big[200];memset(big,'a',199);big[199]=0;assert(!coop_world_normalize(big,out));}
 assert(coop_route_index("worlds\\realm1\\R1M2B")==0);
 assert(coop_route_index("worlds\\realm1\\R1M2C")==1);
 /* NextWorld exactly as the R1M2C mirror exit object reported it on Windows. */
 assert(coop_route_index("worlds/realm1/r1m3a")==2);
 assert(coop_route_index("worlds\\realm1\\R1M3B")==-1);
 assert(coop_route_index("worlds\\deathmatch\\PM01")==-1);
 assert(coop_route_index(0)==-1);
}

static void test_lobby_ready_start(void){
 CoopSession s;
 coop_session_init(&s,2,3);
 assert(s.phase==COOP_PHASE_LOBBY&&s.host==-1&&s.generation==1);
 assert(spawn(&s,A,"Alice",0)==0);
 assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"Alice joined the lobby"));
 assert(s.host==0);
 /* Host cannot start alone below the minimum. */
 cmd(&s,A,"!ready");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"*Alice(R)"));
 cmd(&s,A,"!start");assert(r.action==COOP_ACT_REPLY&&s.phase==COOP_PHASE_LOBBY);
 assert(spawn(&s,B,"Bob",0)==1);
 cmd(&s,B,"!start");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host"));
 cmd(&s,A,"!start");assert(r.action==COOP_ACT_REPLY&&s.phase==COOP_PHASE_LOBBY); /* Bob not ready */
 cmd(&s,B,"  !READY  ");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"2/2 ready"));
 cmd(&s,B,"!unready");assert(strstr(r.msg,"Bob(-)"));
 cmd(&s,B,"!ready");
 {unsigned int seq=s.seq;
  cmd(&s,A,"!start");
  assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==0&&s.phase==COOP_PHASE_TRANSITION);
  assert(s.started&&s.start_pending&&s.seq==seq+1);}
 /* Duplicate start is rejected while the change is pending. */
 cmd(&s,A,"!start");assert(r.action==COOP_ACT_REPLY&&s.phase==COOP_PHASE_TRANSITION);
 native_change(&s,"worlds\\realm1\\R1M2B",2000);
 assert(s.epoch==1&&s.phase==COOP_PHASE_INTRO_PENDING&&s.route_pos==0);
 /* Zero participants: barrier holds until somebody actually spawns. */
 live_all(&s,3000);assert(s.phase==COOP_PHASE_INTRO_PENDING&&s.m[0].used&&s.m[1].used);
 assert(spawn(&s,B,"Bob",4000)==1);  /* slot is stable across worlds */
 assert(s.phase==COOP_PHASE_GAMEPLAY&&strstr(r.msg,"R1M2B started (epoch 1)"));
 assert(spawn(&s,A,"Alice",4100)==0);
 cmd(&s,A,"!ready");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"already started"));
}

static void started_session(CoopSession *s){
 coop_session_init(s,2,3);
 spawn(s,A,"Alice",0);spawn(s,B,"Bob",0);
 cmd(s,A,"!ready");cmd(s,B,"!ready");cmd(s,A,"!start");
 assert(r.action==COOP_ACT_CHANGE_WORLD);
 native_change(s,"worlds\\realm1\\R1M2B",100);
 spawn(s,A,"Alice",200);spawn(s,B,"Bob",210);
 assert(s->phase==COOP_PHASE_GAMEPLAY&&s->epoch==1);
}

static void test_transition_debounce_and_route(void){
 CoopSession s;
 started_session(&s);
 /* Wrong destination is refused, phase unchanged. */
 coop_session_exit(&s,"worlds\\realm1\\R1M2D",300,&r);
 assert(r.action==COOP_ACT_REPLY&&s.phase==COOP_PHASE_GAMEPLAY&&s.rejected_exits==1);
 coop_session_exit(&s,"worlds\\realm1\\R1M2B",300,&r);assert(r.action!=COOP_ACT_CHANGE_WORLD);
 /* Genuine exit: exactly once per epoch, even if touched every frame by both players. */
 coop_session_exit(&s,"Worlds/Realm1/R1M2C",400,&r);
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==1&&s.phase==COOP_PHASE_TRANSITION);
 {unsigned int i,before=s.duplicate_exits;
  for(i=0;i<50;i++){coop_session_exit(&s,"worlds\\realm1\\R1M2C",401+i,&r);assert(r.action==COOP_ACT_NONE);}
  assert(s.duplicate_exits==before+50);}
 cmd(&s,A,"!next");assert(r.action==COOP_ACT_REPLY); /* not in play while transitioning */
 native_change(&s,"worlds\\realm1\\R1M2C",1000);
 assert(s.epoch==2&&s.route_pos==1&&s.phase==COOP_PHASE_INTRO_PENDING);
 /* Stale exit from the previous world during loading is a duplicate, not a new change. */
 coop_session_exit(&s,"worlds\\realm1\\R1M2C",1100,&r);assert(r.action==COOP_ACT_NONE);
 spawn(&s,A,"Alice",1200);assert(s.phase==COOP_PHASE_GAMEPLAY);
 /* Off-route destination: refused; the chat reply is sent once per epoch only. */
 coop_session_exit(&s,"worlds\\realm1\\R1M2D",1300,&r);assert(r.action==COOP_ACT_REPLY);
 {unsigned int i,before=s.rejected_exits;
  for(i=0;i<20;i++){coop_session_exit(&s,"worlds\\realm1\\R1M2D",1301+i,&r);assert(r.action==COOP_ACT_NONE);}
  assert(s.rejected_exits==before+20&&s.phase==COOP_PHASE_GAMEPLAY);}
 /* Natural exit R1M2C -> R1M3A (golden-mirror exit object's NextWorld). */
 coop_session_exit(&s,"worlds/realm1/r1m3a",1400,&r);
 assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==2&&strstr(r.msg,"Exit reached -> R1M3A"));
 coop_session_exit(&s,"worlds/realm1/r1m3a",1401,&r);assert(r.action==COOP_ACT_NONE);
 native_change(&s,"worlds\\realm1\\R1M3A",2000);
 assert(s.epoch==3&&s.route_pos==2&&s.phase==COOP_PHASE_INTRO_PENDING);
 spawn(&s,A,"Alice",2100);assert(s.phase==COOP_PHASE_GAMEPLAY);
 /* Off-route reply is available again in the new epoch. */
 coop_session_exit(&s,"worlds\\realm1\\R1M3B",2200,&r);assert(r.action==COOP_ACT_REPLY);
 /* End of route: no further world change, by exit or by host override. */
 cmd(&s,A,"!next");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"End of the MVP route"));
}

static void test_host_override_and_timeout(void){
 CoopSession s;
 started_session(&s);
 cmd(&s,B,"!next");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"Only the host"));
 cmd(&s,A,"!next");assert(r.action==COOP_ACT_CHANGE_WORLD&&r.target_pos==1);
 cmd(&s,A,"!next");assert(r.action==COOP_ACT_REPLY);
 assert(!coop_session_tick(&s,1000,&r));
 assert(coop_session_tick(&s,1000+COOP_TRANSITION_TIMEOUT_MS,&r)&&s.phase==COOP_PHASE_GAMEPLAY);
 assert(r.action==COOP_ACT_ANNOUNCE);
 cmd(&s,A,"!next");assert(r.action==COOP_ACT_CHANGE_WORLD); /* retry allowed after timeout */
 coop_session_change_failed(&s,&r);
 assert(s.phase==COOP_PHASE_GAMEPLAY&&s.started&&r.action==COOP_ACT_ANNOUNCE);
 /* A failed Start returns to the lobby rather than pretending to play. */
 coop_session_init(&s,1,3);spawn(&s,A,"Solo",0);cmd(&s,A,"!ready");cmd(&s,A,"!start");
 assert(r.action==COOP_ACT_CHANGE_WORLD);
 coop_session_change_failed(&s,&r);assert(s.phase==COOP_PHASE_LOBBY&&!s.started);
 cmd(&s,A,"!start");assert(r.action==COOP_ACT_CHANGE_WORLD);
 assert(coop_session_tick(&s,1000+COOP_TRANSITION_TIMEOUT_MS,&r)&&s.phase==COOP_PHASE_LOBBY&&!s.started);
}

static void test_disconnect_reconnect_and_capacity(void){
 CoopSession s;unsigned int live[4];
 coop_session_init(&s,2,3);
 assert(spawn(&s,A,"Alice",0)==0);assert(spawn(&s,B,"Bob",0)==1);assert(spawn(&s,C,"Cid",0)==2);
 assert(spawn(&s,D,"Dee",0)==-1&&r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"full")&&s.full_rejects==1);
 cmd(&s,D,"!ready");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"not in the session"));
 cmd(&s,A,"!ready");cmd(&s,B,"!ready");
 /* Host Alice disconnects: her object leaves the native player list. */
 live[0]=B+2;live[1]=C+2;
 assert(coop_session_presence(&s,live,2,10,&r)==1&&strstr(r.msg,"Alice left"));
 assert(!s.m[0].used&&s.host==1);
 /* Slot 0 is reclaimed for the next joiner; reconnecting Alice gets a new identity. */
 assert(spawn(&s,D,"Dee",20)==0);
 assert(spawn(&s,A,"Alice",30)==-1);           /* still full: 3 present */
 cmd(&s,C,"!leave");assert(r.action==COOP_ACT_ANNOUNCE&&strstr(r.msg,"Cid(away)"));
 assert(spawn(&s,A,"Alice",40)==3);              /* away member keeps its slot */
 cmd(&s,C,"!join");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"full"));
 /* Engine reuses an object for a different client: old member is evicted. */
 assert(coop_session_seen(&s,0x999u,0x5000u,D+2,"New",50,&r)==0&&coop_find_client(&s,D)<0);
}

static void test_slow_joiner_and_all_left(void){
 CoopSession s;
 started_session(&s);
 coop_session_exit(&s,"worlds\\realm1\\R1M2C",500,&r);
 native_change(&s,"worlds\\realm1\\R1M2C",1000);
 /* Bob loads slowly: kept during the grace period although not in the list. */
 spawn(&s,A,"Alice",1500);
 {unsigned int live[1];live[0]=A+2;
  coop_session_presence(&s,live,1,1000+COOP_LOAD_GRACE_MS-1,&r);assert(s.m[1].used);
  coop_session_presence(&s,live,1,1000+COOP_LOAD_GRACE_MS,&r);assert(!s.m[1].used&&strstr(r.msg,"Bob left"));}
 /* Late arrival is re-admitted in progress, not as a lobby join. */
 assert(spawn(&s,B,"Bob",1000+COOP_LOAD_GRACE_MS+5)>=0&&strstr(r.msg,"joined in progress"));
 /* Everybody leaves: generation increments and the lobby is restored. */
 {unsigned int gen=s.generation;
  coop_session_presence(&s,0,0,200000,&r);
  assert(s.phase==COOP_PHASE_LOBBY&&!s.started&&s.generation==gen+1&&s.host==-1&&strstr(r.msg,"reset"));}
}

static void test_untrusted_input(void){
 CoopSession s;char lng[200];unsigned int i,seq;
 coop_session_init(&s,2,3);
 spawn(&s,A,"Ev\x01il\xff\x1bName-that-is-much-longer-than-thirty-two-bytes",0);
 for(i=0;s.m[0].name[i];i++)assert(s.m[0].name[i]>=32&&s.m[0].name[i]<127);
 assert(i==COOP_NAME_MAX-1);
 assert(spawn(&s,B,"",0)==1&&!strcmp(s.m[1].name,"?"));
 seq=s.seq;
 cmd(&s,A,"ready");assert(r.action==COOP_ACT_NONE);           /* not a command */
 cmd(&s,A,"!ready now");assert(r.action==COOP_ACT_NONE);      /* trailing args */
 cmd(&s,A,"!");assert(r.action==COOP_ACT_NONE);
 cmd(&s,A,"!unknowncommand");assert(r.action==COOP_ACT_NONE);
 cmd(&s,A,"!aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");assert(r.action==COOP_ACT_NONE);
 memset(lng,' ',sizeof lng);lng[0]='!';lng[1]='r';lng[199]=0;
 cmd(&s,A,lng);assert(r.action==COOP_ACT_NONE);               /* over-long */
 coop_session_command(&s,A,0,0,&r);assert(r.action==COOP_ACT_NONE);
 assert(s.seq==seq&&!s.m[0].ready);
 cmd(&s,A,"!help");assert(r.action==COOP_ACT_REPLY&&strstr(r.msg,"kpc-coop/1"));
 assert(strlen(r.msg)<COOP_MSG_MAX);
 /* Status always fits even with four maximal names. */
 coop_session_init(&s,1,4);
 for(i=0;i<4;i++)coop_session_seen(&s,0x10u+i,0x20u+i,0x30u+i,"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",0,&r);
 cmd(&s,0x10u,"!coop");assert(r.action==COOP_ACT_REPLY&&strlen(r.msg)==COOP_MSG_MAX-1);
 assert(coop_session_seen(&s,0,1,2,"x",0,&r)==-1);            /* invalid identity */
 /* Lobby exits never change the world. */
 coop_session_exit(&s,"worlds\\realm1\\R1M2C",0,&r);assert(r.action==COOP_ACT_REPLY&&s.phase==COOP_PHASE_LOBBY);
 coop_session_exit(&s,"worlds\\realm1\\R1M2C",0,&r);assert(r.action==COOP_ACT_NONE); /* reply once */
}

static void test_offroute_native_level(void){
 CoopSession s;
 started_session(&s);
 /* Operator typed `nextlevel` into an unrelated map: session stops, lobby returns. */
 native_change(&s,"worlds\\deathmatch\\PM01",900);
 assert(s.phase==COOP_PHASE_LOBBY&&!s.started&&!s.m[0].ready&&s.epoch==2);
}

int main(void){
 test_normalize();
 test_lobby_ready_start();
 test_transition_debounce_and_route();
 test_host_override_and_timeout();
 test_disconnect_reconnect_and_capacity();
 test_slow_joiner_and_all_left();
 test_untrusted_input();
 test_offroute_native_level();
 puts("PASS coop session: lobby FSM, readiness, host, capacity, disconnect/reconnect, slow join, epochs, debounce, route, timeouts, input bounds");
 return 0;
}
