/* Asset-free tests for the !start scoreboard presentation (CoopStartScoreboard). */
/* Only part of the shared session header is exercised here. */
#pragma GCC diagnostic ignored "-Wunused-function"
#include "coop_session.h"
#include "coop_scoreboard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define A 0x100u
#define B 0x200u
#define C 0x300u

static CoopResult r;
static CoopSbMessage out[COOP_SESSION_MAX_MEMBERS];

static int spawn(CoopSession *s,CoopScoreboard *b,unsigned int client,const char *name,unsigned int now){
 int i=coop_session_seen(s,client,client+1,client+2,name,now,&r);
 coop_sb_spawn(b,s,i,now);
 return i;
}
static int has(unsigned int n,unsigned int client,unsigned int id){
 unsigned int i;for(i=0;i<n;i++)if(out[i].client==client&&out[i].id==id)return 1;return 0;
}
/* Lobby with Alice and Bob ready, host Alice types !start. */
static void start(CoopSession *s,CoopScoreboard *b,unsigned int pos,unsigned int now){
 unsigned int n;
 coop_session_init(s,2,3);s->start_pos=pos;coop_sb_reset(b);
 spawn(s,b,A,"Alice",0);spawn(s,b,B,"Bob",0);
 coop_session_command(s,A,"!ready",100,&r);coop_session_command(s,B,"!ready",100,&r);
 coop_session_command(s,A,"!start",now,&r);
 assert(r.action==COOP_ACT_CHANGE_WORLD&&s->start_pending);
 n=coop_sb_start(b,s,now,out);
 assert(n==2&&has(n,A,COOP_SB_SHOW)&&has(n,B,COOP_SB_SHOW));
}
static void load(CoopSession *s,CoopScoreboard *b,const char *world,int intro,unsigned int now){
 coop_sb_world_loading(b);
 coop_session_world_loading(s,now);coop_session_world_loaded(s,world);
 coop_session_intro_arm(s,intro);
}

static void test_intro_flow(void){
 CoopSession s;CoopScoreboard b;unsigned int n,t=1000;
 start(&s,&b,2,t);
 /* The world change waits for the hold; nothing else is sent meanwhile. */
 assert(!coop_sb_take_change(&b,&s,t+COOP_SB_HOLD_MS-1));
 assert(coop_sb_frame(&b,&s,t+500,out)==0);
 assert(coop_sb_take_change(&b,&s,t+COOP_SB_HOLD_MS));
 assert(!coop_sb_take_change(&b,&s,t+COOP_SB_HOLD_MS+10));          /* once */
 /* Native NextLevel; the stock re-entry closes each client's scoreboard. */
 load(&s,&b,"worlds\\realm1\\R1M3A",1,t=3000);
 assert(coop_sb_frame(&b,&s,t,out)==0);                             /* nothing during load */
 spawn(&s,&b,A,"Alice",t=4000);
 assert(coop_sb_frame(&b,&s,t+COOP_SB_RESHOW_MS-1,out)==0);
 n=coop_sb_frame(&b,&s,t+COOP_SB_RESHOW_MS,out);assert(n==1&&has(n,A,COOP_SB_SHOW));
 assert(coop_sb_frame(&b,&s,t+1000,out)==0);                        /* never shown twice */
 spawn(&s,&b,A,"Alice",t+1100);assert(coop_sb_frame(&b,&s,t+2000,out)==0); /* respawn: still shown, no resend */
 spawn(&s,&b,B,"Bob",t=6000);
 n=coop_sb_frame(&b,&s,t+COOP_SB_RESHOW_MS,out);assert(n==1&&has(n,B,COOP_SB_SHOW));
 /* Barrier + settle: the intro starts; both close before the script plays. */
 assert(coop_session_intro_tick(&s,t+COOP_INTRO_SETTLE_MS,&r)==COOP_ACT_INTRO_START);
 n=coop_sb_frame(&b,&s,t+COOP_INTRO_SETTLE_MS,out);
 assert(n==2&&has(n,A,COOP_SB_CLOSE)&&has(n,B,COOP_SB_CLOSE)&&!b.active);
 assert(coop_sb_frame(&b,&s,t+COOP_INTRO_SETTLE_MS+1,out)==0);      /* closed once */
 /* A respawn during the intro shows nothing. */
 spawn(&s,&b,A,"Alice",t+5000);assert(coop_sb_frame(&b,&s,t+6000,out)==0);
}
static void test_no_intro_world(void){
 CoopSession s;CoopScoreboard b;
 start(&s,&b,0,1000);
 assert(coop_sb_take_change(&b,&s,2000));
 load(&s,&b,"worlds\\realm1\\R1M2B",0,3000);
 spawn(&s,&b,A,"Alice",4000);                                       /* gameplay begins */
 assert(s.phase==COOP_PHASE_GAMEPLAY);
 assert(coop_sb_frame(&b,&s,5000,out)==0&&!b.active);              /* stock re-entry closed it */
}
static void test_failures(void){
 CoopSession s;CoopScoreboard b;unsigned int n;
 /* Change failed before NextLevel: both are still in the old world. */
 start(&s,&b,2,1000);
 coop_session_change_failed(&s,&r);assert(s.phase==COOP_PHASE_LOBBY);
 assert(!coop_sb_take_change(&b,&s,3000));
 n=coop_sb_frame(&b,&s,3000,out);assert(n==2&&has(n,A,COOP_SB_CLOSE)&&has(n,B,COOP_SB_CLOSE)&&!b.active);
 /* Transition timeout (no NextLevel within 30 s) also closes. */
 start(&s,&b,2,1000);
 assert(coop_session_tick(&s,1000+COOP_TRANSITION_TIMEOUT_MS,&r));
 n=coop_sb_frame(&b,&s,1000+COOP_TRANSITION_TIMEOUT_MS,out);assert(n==2);
 /* Barrier timeout with one player in: the intro still starts and closes it. */
 start(&s,&b,2,1000);coop_sb_take_change(&b,&s,2000);
 load(&s,&b,"worlds\\realm1\\R1M3A",1,3000);
 spawn(&s,&b,A,"Alice",4000);n=coop_sb_frame(&b,&s,4300,out);assert(n==1);
 assert(coop_session_intro_tick(&s,3000+COOP_INTRO_BARRIER_MS,&r)==COOP_ACT_INTRO_START);
 n=coop_sb_frame(&b,&s,3000+COOP_INTRO_BARRIER_MS,out);assert(n==1&&has(n,A,COOP_SB_CLOSE));
 /* Failsafe: an intro that never starts (no tick) is closed after COOP_SB_MAX_MS. */
 start(&s,&b,2,1000);coop_sb_take_change(&b,&s,2000);
 load(&s,&b,"worlds\\realm1\\R1M3A",1,3000);
 spawn(&s,&b,A,"Alice",4000);assert(coop_sb_frame(&b,&s,4300,out)==1);
 assert(coop_sb_frame(&b,&s,1000+COOP_SB_MAX_MS-1,out)==0);
 n=coop_sb_frame(&b,&s,1000+COOP_SB_MAX_MS,out);assert(n==1&&has(n,A,COOP_SB_CLOSE)&&!b.active);
}
static void test_disconnect_and_late_join(void){
 CoopSession s;CoopScoreboard b;unsigned int n,live[2];int i;
 start(&s,&b,2,1000);coop_sb_take_change(&b,&s,2000);
 load(&s,&b,"worlds\\realm1\\R1M3A",1,3000);
 spawn(&s,&b,A,"Alice",4000);spawn(&s,&b,B,"Bob",4000);
 assert(coop_sb_frame(&b,&s,4300,out)==2);
 /* Bob disconnects during the barrier: forgotten, no message to a stale handle. */
 live[0]=s.m[0].object;coop_session_presence(&s,live,1,5000,&r);
 assert(!s.m[1].used);
 /* Carol joins late into the same slot: her own show, never Bob's state. */
 i=spawn(&s,&b,C,"Carol",5500);assert(i==1);
 n=coop_sb_frame(&b,&s,5500+COOP_SB_RESHOW_MS,out);assert(n==1&&has(n,C,COOP_SB_SHOW));
 assert(coop_session_intro_tick(&s,5500+COOP_INTRO_SETTLE_MS,&r)==COOP_ACT_INTRO_START);
 n=coop_sb_frame(&b,&s,5500+COOP_INTRO_SETTLE_MS,out);
 assert(n==2&&has(n,A,COOP_SB_CLOSE)&&has(n,C,COOP_SB_CLOSE)&&!has(n,B,COOP_SB_CLOSE));
}
static void test_inactive(void){
 CoopSession s;CoopScoreboard b;
 coop_session_init(&s,1,3);coop_sb_reset(&b);
 spawn(&s,&b,A,"Alice",0);
 assert(coop_sb_frame(&b,&s,100,out)==0&&!coop_sb_take_change(&b,&s,100));
 /* Natural exits never use it: no show without coop_sb_start. */
 load(&s,&b,"worlds\\realm1\\R1M3A",1,200);spawn(&s,&b,A,"Alice",300);
 assert(coop_sb_frame(&b,&s,1000,out)==0);
}
int main(void){
 test_intro_flow();test_no_intro_world();test_failures();test_disconnect_and_late_join();test_inactive();
 printf("PASS coop start scoreboard policy\n");
 return 0;
}
