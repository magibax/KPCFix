#ifndef KPC_COOP_SCOREBOARD_H
#define KPC_COOP_SCOREBOARD_H
/* !start loading presentation with the stock multiplayer scoreboard
 * ([Coop] CoopStartScoreboard). Pure policy over CoopSession; the proxy sends
 * the returned messages with the original engine message API.
 *
 * Client evidence (stock cshell 703813a4):
 *  - 0x48 MID_MULTI_INTERMISSION (cb4f7 -> cdd50) reads no payload; in
 *    multiplayer it sets state 7 and shows screen 2 (scoreboard). The client
 *    has no timer and no click gate of its own: the 3 s + fire/use gate is the
 *    server's native end-level (object.lto 100760/1007c0), never used here.
 *  - 0x46 MID_CHANGING_LEVELS sets state 5; the render path (cdb57-cdb6f)
 *    does not draw screen 2 in state 5, so the scoreboard cannot be seen while
 *    the client changes world. That gap is client-side.
 *  - On re-entry the server's client-enter code sends 0x1a and 0x69
 *    MID_CAN_RENDER; either closes screen 2 (cb2d8 / cbba6).
 *  - 0x69 (cbb86) reads no payload, closes screen 2 and returns the client to
 *    state 1 (c6f30). Showing 0x48 twice without a close in between loses the
 *    saved HUD flag, so every show is paired with exactly one close.
 * Sequence: show to every present member at !start, hold the world change
 * COOP_SB_HOLD_MS, let the stock re-entry close it, show it again to each
 * member COOP_SB_RESHOW_MS after it spawned in a world whose intro is armed,
 * and close it for everyone just before the intro starts. Any other outcome
 * (no intro, change failed or timed out, intro failed, phase left) closes it;
 * COOP_SB_MAX_MS is a final failsafe. */
#define COOP_SB_HOLD_MS 1000u
#define COOP_SB_RESHOW_MS 250u
#define COOP_SB_MAX_MS 45000u
#define COOP_SB_SHOW 0x48u
#define COOP_SB_CLOSE 0x69u

typedef struct {
 unsigned int active,started_ms;
 unsigned int change_pending,change_due_ms;
 unsigned char shown[COOP_SESSION_MAX_MEMBERS],reshow[COOP_SESSION_MAX_MEMBERS];
 unsigned int client[COOP_SESSION_MAX_MEMBERS],reshow_due[COOP_SESSION_MAX_MEMBERS];
} CoopScoreboard;

typedef struct {unsigned int client,id;} CoopSbMessage;

static void coop_sb_reset(CoopScoreboard *b){
 unsigned int i;
 b->active=0;b->started_ms=0;b->change_pending=0;b->change_due_ms=0;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){b->shown[i]=0;b->reshow[i]=0;b->client[i]=0;b->reshow_due[i]=0;}
}
static int coop_sb_member_live(const CoopSession *s,unsigned int i,unsigned int client){
 return s->m[i].used&&!s->m[i].away&&s->m[i].client==client&&client!=0;
}
/* Host !start accepted (phase TRANSITION, start_pending). Fills out[] with a
 * show for every present member and defers the world change. Returns count. */
static unsigned int coop_sb_start(CoopScoreboard *b,const CoopSession *s,unsigned int now,CoopSbMessage *out){
 unsigned int i,n=0;
 coop_sb_reset(b);
 b->active=1;b->started_ms=now;b->change_pending=1;b->change_due_ms=now+COOP_SB_HOLD_MS;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  if(!s->m[i].used||s->m[i].away||!s->m[i].client)continue;
  b->shown[i]=1;b->client[i]=s->m[i].client;
  out[n].client=s->m[i].client;out[n].id=COOP_SB_SHOW;n++;
 }
 return n;
}
/* 1 once the hold has elapsed and the deferred world change should be made. */
static int coop_sb_take_change(CoopScoreboard *b,const CoopSession *s,unsigned int now){
 if(!b->change_pending)return 0;
 if(s->phase!=COOP_PHASE_TRANSITION){b->change_pending=0;return 0;}
 if((int)(now-b->change_due_ms)<0)return 0;
 b->change_pending=0;return 1;
}
/* Native NextLevel: every client changes world and its own re-entry (0x1a,
 * 0x69) closes the scoreboard, so nothing is shown any more. */
static void coop_sb_world_loading(CoopScoreboard *b){
 unsigned int i;
 b->change_pending=0;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){b->shown[i]=0;b->reshow[i]=0;}
}
/* Member i spawned (after world_loaded/intro_arm). In a world whose intro is
 * armed and still pending, schedule one show shortly after the stock close. */
static void coop_sb_spawn(CoopScoreboard *b,const CoopSession *s,int i,unsigned int now){
 if(!b->active||i<0||(unsigned int)i>=COOP_SESSION_MAX_MEMBERS)return;
 if(b->client[i]!=s->m[i].client){b->shown[i]=0;b->reshow[i]=0;b->client[i]=s->m[i].client;}
 if(s->phase!=COOP_PHASE_INTRO_PENDING||!s->intro_armed||b->shown[i]||!s->m[i].client)return;
 b->reshow[i]=1;b->reshow_due[i]=now+COOP_SB_RESHOW_MS;
}
static int coop_sb_keep(const CoopScoreboard *b,const CoopSession *s,unsigned int now){
 if((unsigned int)(now-b->started_ms)>=COOP_SB_MAX_MS)return 0;
 if(s->phase==COOP_PHASE_TRANSITION||s->phase==COOP_PHASE_WORLD_LOADING)return 1;
 return s->phase==COOP_PHASE_INTRO_PENDING&&s->intro_armed;
}
/* Per server frame (and immediately before the intro starts): fills out[]
 * with due shows and required closes. A member that left is forgotten without
 * a message (its client handle is no longer valid). Returns count. */
static unsigned int coop_sb_frame(CoopScoreboard *b,const CoopSession *s,unsigned int now,CoopSbMessage *out){
 unsigned int i,n=0;
 int keep;
 if(!b->active)return 0;
 keep=coop_sb_keep(b,s,now);
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  int live=coop_sb_member_live(s,i,b->client[i]);
  if(!live){b->shown[i]=0;b->reshow[i]=0;continue;}
  if(!keep){
   b->reshow[i]=0;
   if(b->shown[i]){b->shown[i]=0;out[n].client=b->client[i];out[n].id=COOP_SB_CLOSE;n++;}
   continue;
  }
  if(b->reshow[i]&&!b->shown[i]&&(int)(now-b->reshow_due[i])>=0){
   b->reshow[i]=0;b->shown[i]=1;out[n].client=b->client[i];out[n].id=COOP_SB_SHOW;n++;
  }
 }
 if(!keep){b->active=0;b->change_pending=0;}
 return n;
}
#endif
