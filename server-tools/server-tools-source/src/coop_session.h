#ifndef KPC_COOP_SESSION_H
#define KPC_COOP_SESSION_H
/* Experimental co-op session/lobby state machine (protocol "kpc-coop/1").
 *
 * Pure, freestanding policy: no engine pointers are dereferenced here. The
 * proxy feeds it bounded, already-copied inputs (client handle, player and
 * object identities, sanitised names, chat text, world names, a millisecond
 * clock) and executes the returned action through verified native paths.
 *
 * Transport: players type chat commands (!ready, !start ...). The original
 * game's reliable chat relay carries them; the server answers with ordinary
 * chat broadcasts. No new network message IDs are introduced.
 *
 * Authority: only the dedicated server decides. A client can only change its
 * own readiness/away flag; the elected host (earliest joined, present, not
 * away) may request Start and the host override Next. World changes are only
 * ever to the next entry of the fixed, server-side route.
 */
#define COOP_PROTOCOL_VERSION 1u
#define COOP_SESSION_MAX_MEMBERS 4u
#define COOP_NAME_MAX 32u
#define COOP_MSG_MAX 112u
#define COOP_WORLD_MAX 64u
#define COOP_COMMAND_MAX 64u
/* Windows evidence: R1M2B -> R1M2C changed via the host !next override; on
 * R1M2C a golden-mirror exit object reported NextWorld realm1/r1m3a (refused
 * by the earlier build). A natural R1M2C -> R1M3A change is not yet observed. */
#include "coop_campaign.h"
#define COOP_INTRO_SETTLE_MS 3000u    /* default intro settle after the last participant spawned */
#define COOP_CAMPAIGN_SETTLE_MS 0u    /* campaign default: KPCFix clients hold their spawn behind the picture, so the intro starts with the last spawn */
#define COOP_TARGET_LOBBY 0xffffu   /* CoopResult.target_pos: return to the lobby world */
#define COOP_DEFAULT_LOBBY "realm1/r1m2b" /* lobby world when CoopLobbyWorld is empty */
#define COOP_TRANSITION_TIMEOUT_MS 30000u
#define COOP_LOAD_GRACE_MS 90000u

typedef enum {
 COOP_PHASE_LOBBY=1,          /* WAITING_FOR_READY; session not started */
 COOP_PHASE_WORLD_LOADING=2,  /* native NextLevel in progress */
 COOP_PHASE_INTRO_PENDING=3,  /* world loaded, waiting for first participant */
 COOP_PHASE_GAMEPLAY=4,
 COOP_PHASE_TRANSITION=5,     /* server-requested world change pending */
 COOP_PHASE_INTRO_RUNNING=6   /* original starting_cinematic playing for the session */
} CoopPhase;

typedef enum {
 COOP_ACT_NONE=0,
 COOP_ACT_ANNOUNCE=1,     /* broadcast msg */
 COOP_ACT_CHANGE_WORLD=2, /* broadcast msg, then load route[target] natively */
 COOP_ACT_REPLY=3,        /* informational reply (broadcast; chat has no unicast here) */
 COOP_ACT_INTRO_START=4,  /* broadcast msg, then play the world's starting_cinematic once */
 COOP_ACT_INTRO_ABORT=5   /* broadcast msg, then release camera/input (intro overran) */
} CoopAction;

typedef struct {
 unsigned int client,player,object,join_seq,seen_epoch;
 unsigned char used,ready,away,spawned;
 char name[COOP_NAME_MAX];
 /* Per-player world music delivery (see coop_session_music_*). */
 unsigned int music_epoch;    /* epoch+1 of the last delivered music start (0 = none) */
 unsigned int music_due_ms;   /* delivery time when music_pending */
 unsigned char music_pending;
 unsigned int spawn_ms;       /* last spawn time (valid when spawned this epoch) */
} CoopMember;

typedef struct {
 int phase;
 unsigned int generation,epoch,seq,join_counter;
 unsigned int min_players,max_players;
 unsigned int started;              /* Start committed in this generation */
 unsigned int start_pending;        /* current transition was requested by Start */
 unsigned int route_pos,target_pos; /* index into the route */
 unsigned int transition_epoch;     /* debounce: one change per epoch */
 unsigned int transition_ms,epoch_ms;
 unsigned int duplicate_exits,rejected_exits,full_rejects;
 unsigned int start_pos;            /* route index !start loads (test option; default 0) */
 unsigned int intro_armed;          /* this epoch's world has a playable intro */
 unsigned int intro_ms;             /* INTRO_RUNNING start time */
 unsigned int reject_reply_epoch;    /* epoch+1 of the last off-route reply (0 = none) */
 int host;                          /* member index or -1 */
 CoopMember m[COOP_SESSION_MAX_MEMBERS];
 /* Route view: the MVP table (CoopCampaign=0) or a WORLDORDER slice. */
 const CoopWorld *route;
 unsigned int route_len;
 unsigned int campaign;             /* CoopCampaign=1: host selection enabled */
 unsigned int sel_mode,sel_realm;   /* host selection; kept until changed */
 const char *test_label;            /* CoopTestStartWorld label (test only) or 0 */
 unsigned int lobby_pending;        /* current transition returns to the lobby */
 unsigned int abort_after_load;     /* !abort during a world change: back to the lobby once it has loaded */
 int skill;                         /* difficulty written before each world load: 0-4, or -1 = unchanged */
 unsigned int host_exit_epoch;      /* epoch+1 of the last "only the host" exit reply */
 unsigned int host_exit_ms;         /* its time (repeated after COOP_HOST_EXIT_REPLY_MS) */
 unsigned int host_seq_ms,host_seq_said; /* last "only the host" sequence reply (rate limit) */
 unsigned int rejected_sequences;
 unsigned int completed;            /* routes completed in this server run */
 char world_label[16];              /* loaded world, e.g. R1M2B */
 char lobby_world[COOP_WORLD_MAX];  /* normalised lobby world or empty = route start */
 unsigned int intro_settle_ms;      /* barrier settle after the last spawn (default 3000) */
} CoopSession;

typedef struct {
 int action;
 unsigned int target_pos;
 char msg[COOP_MSG_MAX];
} CoopResult;


/* ---------- bounded string helpers (no libc) ---------- */
static unsigned int coop_strlen(const char *s,unsigned int max){
 unsigned int n=0;if(!s)return 0;while(n<max&&s[n])n++;return n;
}
static void coop_append(char *out,unsigned int *n,const char *s){
 while(s&&*s&&*n+1<COOP_MSG_MAX)out[(*n)++]=*s++;
 out[*n]=0;
}
static void coop_append_uint(char *out,unsigned int *n,unsigned int v){
 char tmp[11];unsigned int k=0;
 do{tmp[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10);
 while(k&&*n+1<COOP_MSG_MAX)out[(*n)++]=tmp[--k];
 out[*n]=0;
}
/* Printable ASCII only; control/high bytes become '?'. Always terminated. */
static void coop_sanitize(char *out,const char *in,unsigned int cap){
 unsigned int i=0;
 if(!cap)return;
 if(in)for(;i+1<cap&&in[i];i++){
  unsigned char c=(unsigned char)in[i];
  out[i]=(c>=32&&c<127)?(char)c:'?';
 }
 out[i]=0;
 if(!i&&cap>1){out[0]='?';out[1]=0;}
}
static char coop_lower(char c){return (c>='A'&&c<='Z')?(char)(c+32):c;}
/* Normalise a world path: lower-case, '\\'->'/', drop "./", "worlds/" and
 * ".dat". Output is bounded; returns 0 if the input is empty or too long. */
static int coop_world_normalize(const char *in,char out[COOP_WORLD_MAX]){
 unsigned int i=0,n=0,len=coop_strlen(in,128);
 if(!in||!len||len>=127)return 0;
 if(len>=2&&in[0]=='.'&&(in[1]=='/'||in[1]=='\\'))i=2;
 if(len-i>=7){
  const char *w="worlds";unsigned int k;int match=1;
  for(k=0;k<6;k++)if(coop_lower(in[i+k])!=w[k])match=0;
  if(match&&(in[i+6]=='/'||in[i+6]=='\\'))i+=7;
 }
 for(;i<len;i++){
  char c=in[i]=='\\'?'/':coop_lower(in[i]);
  if((unsigned char)c<33||(unsigned char)c>126)return 0;
  if(n+1>=COOP_WORLD_MAX)return 0;
  out[n++]=c;
 }
 if(n>=4&&out[n-4]=='.'&&out[n-3]=='d'&&out[n-2]=='a'&&out[n-1]=='t')n-=4;
 out[n]=0;
 return n>0;
}
static int coop_word(const char *a,const char *b){
 unsigned int i;
 for(i=0;i<16;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}
 return 0;
}
static int coop_world_equal(const char *a,const char *b){
 char x[COOP_WORLD_MAX],y[COOP_WORLD_MAX];unsigned int i;
 if(!coop_world_normalize(a,x)||!coop_world_normalize(b,y))return 0;
 for(i=0;x[i]||y[i];i++)if(x[i]!=y[i])return 0;
 return 1;
}
/* Returns the index of a world in the active route or -1. */
static int coop_route_index(const CoopSession *s,const char *world){
 unsigned int i;
 for(i=0;i<s->route_len;i++)if(coop_world_equal(world,s->route[i].world))return (int)i;
 return -1;
}
static const char *coop_route_label(const CoopSession *s,unsigned int pos){
 return pos<s->route_len?s->route[pos].label:"?";
}
/* World a CHANGE_WORLD result targets: a route world or the lobby world. */
static const char *coop_target_world(const CoopSession *s,unsigned int target){
 /* Lobby: CoopLobbyWorld, else always R1M2B (the co-op lobby world the
  * launcher starts; it is in every maplist the launcher writes). */
 if(target==COOP_TARGET_LOBBY)return s->lobby_world[0]?s->lobby_world:COOP_DEFAULT_LOBBY;
 return target<s->route_len?s->route[target].world:"";
}
/* Upper-case last path component without ".dat", bounded (e.g. R1M2B). */
static void coop_world_label(const char *world,char out[16]){
 char w[COOP_WORLD_MAX];unsigned int i,start=0,n=0;
 out[0]=0;
 if(!coop_world_normalize(world,w))return;
 for(i=0;w[i];i++)if(w[i]=='/')start=i+1;
 for(i=start;w[i]&&n<15;i++)out[n++]=(w[i]>='a'&&w[i]<='z')?(char)(w[i]-32):w[i];
 out[n]=0;
}
/* Global WORLDORDER index of route position pos, or -1 for the MVP table. */
static int coop_campaign_index(const CoopSession *s,unsigned int pos){
 if(!s->campaign||pos>=s->route_len)return -1;
 return (int)((unsigned int)(s->route-coop_campaign_worlds)+pos);
}
/* Resolve the test start label against the active route (0 if absent). */
static void coop_resolve_test_start(CoopSession *s){
 unsigned int i;
 s->start_pos=0;
 if(!s->test_label)return;
 for(i=0;i<s->route_len;i++)if(coop_word(s->route[i].label,s->test_label)){s->start_pos=i;return;}
}
/* Apply a selection (campaign mode). Returns 0 if invalid; unchanged then. */
static int coop_session_select(CoopSession *s,unsigned int mode,unsigned int realm){
 unsigned int first,len;
 if(!s->campaign||!coop_campaign_slice(mode,realm,&first,&len))return 0;
 s->sel_mode=mode;s->sel_realm=mode==COOP_SEL_FULL?0:realm;
 s->route=&coop_campaign_worlds[first];s->route_len=len;
 coop_resolve_test_start(s);
 return 1;
}
/* Proxy, once at arm: enable the campaign graph with the default selection
 * (full campaign from the first world, realm1/r1m0). */
static void coop_session_enable_campaign(CoopSession *s){
 s->campaign=1;coop_session_select(s,COOP_SEL_FULL,0);
}
/* "full (R1M0..R5M1B, 49 worlds)" / "from Earth (...)" / "realm Fire (...)". */
static void coop_selection_text(const CoopSession *s,char *out,unsigned int *n){
 if(!s->campaign){coop_append(out,n,"MVP route R1M2B>R1M2C>R1M3A");return;}
 if(s->sel_mode==COOP_SEL_FULL)coop_append(out,n,"full campaign");
 else if(s->sel_mode==COOP_SEL_WORLD){coop_append(out,n,"from ");coop_append(out,n,coop_route_label(s,0));}
 else{coop_append(out,n,s->sel_mode==COOP_SEL_FROM?"from ":"realm ");coop_append(out,n,coop_realm_name(s->sel_realm));}
 coop_append(out,n," (");coop_append(out,n,coop_route_label(s,0));coop_append(out,n,"..");
 coop_append(out,n,coop_route_label(s,s->route_len-1u));coop_append(out,n,", ");
 coop_append_uint(out,n,s->route_len);coop_append(out,n," worlds)");
}
static const char *coop_phase_name(int phase){
 switch(phase){
  case COOP_PHASE_LOBBY:return "LOBBY";
  case COOP_PHASE_WORLD_LOADING:return "LOADING";
  case COOP_PHASE_INTRO_PENDING:return "INTRO_PENDING";
  case COOP_PHASE_GAMEPLAY:return "PLAYING";
  case COOP_PHASE_TRANSITION:return "TRANSITION";
  case COOP_PHASE_INTRO_RUNNING:return "INTRO";
 }
 return "?";
}

/* ---------- membership ---------- */
static void coop_session_init(CoopSession *s,unsigned int min_players,unsigned int max_players){
 unsigned int i;unsigned char *p=(unsigned char *)s;
 for(i=0;i<sizeof(*s);i++)p[i]=0;
 if(max_players<1)max_players=1;
 if(max_players>COOP_SESSION_MAX_MEMBERS)max_players=COOP_SESSION_MAX_MEMBERS;
 if(min_players<1)min_players=1;
 if(min_players>max_players)min_players=max_players;
 s->min_players=min_players;s->max_players=max_players;
 s->phase=COOP_PHASE_LOBBY;s->generation=1;s->host=-1;
 s->route=coop_mvp_worlds;s->route_len=COOP_MVP_LEN;s->sel_mode=COOP_SEL_MVP;
 s->intro_settle_ms=COOP_INTRO_SETTLE_MS;
 s->skill=-1;
}
static int coop_find_client(const CoopSession *s,unsigned int client){
 unsigned int i;
 if(!client)return -1;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(s->m[i].used&&s->m[i].client==client)return (int)i;
 return -1;
}
static unsigned int coop_present_count(const CoopSession *s){
 unsigned int i,n=0;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(s->m[i].used&&!s->m[i].away)n++;
 return n;
}
static unsigned int coop_ready_count(const CoopSession *s){
 unsigned int i,n=0;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(s->m[i].used&&!s->m[i].away&&s->m[i].ready)n++;
 return n;
}
/* Deterministic host: earliest join sequence among present, non-away members. */
static int coop_elect_host(CoopSession *s){
 unsigned int i;int best=-1;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  const CoopMember *m=&s->m[i];
  if(!m->used||m->away)continue;
  if(best<0||m->join_seq<s->m[best].join_seq)best=(int)i;
 }
 s->host=best;return best;
}
static void coop_remove_member(CoopSession *s,unsigned int i){
 unsigned int k;unsigned char *p=(unsigned char *)&s->m[i];
 for(k=0;k<sizeof(s->m[i]);k++)p[k]=0;
 coop_elect_host(s);
}
/* All members gone: return to a fresh lobby generation. The loaded world is
 * left running natively; a later Start reloads route[0]. */
static void coop_reset_generation(CoopSession *s,unsigned int now){
 unsigned int i;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)if(s->m[i].used)coop_remove_member(s,i);
 s->generation++;s->started=0;s->phase=COOP_PHASE_LOBBY;s->host=-1;
 s->transition_epoch=0;s->epoch_ms=now;
}

/* Status line, e.g. "[Co-op] LOBBY R1M2B e0 host=Al: Al(R) Bo(-) 1/2 ready, min 2" */
static void coop_status(const CoopSession *s,char out[COOP_MSG_MAX]){
 unsigned int n=0,i;
 out[0]=0;
 coop_append(out,&n,"[Co-op] ");coop_append(out,&n,coop_phase_name(s->phase));
 coop_append(out,&n," ");coop_append(out,&n,s->world_label[0]?s->world_label:coop_route_label(s,s->route_pos));
 coop_append(out,&n," e");coop_append_uint(out,&n,s->epoch);
 coop_append(out,&n,":");
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  const CoopMember *m=&s->m[i];
  if(!m->used)continue;
  coop_append(out,&n," ");
  if((int)i==s->host)coop_append(out,&n,"*");
  coop_append(out,&n,m->name);
  coop_append(out,&n,m->away?"(away)":m->ready?"(R)":"(-)");
 }
 coop_append(out,&n," ");coop_append_uint(out,&n,coop_ready_count(s));
 coop_append(out,&n,"/");coop_append_uint(out,&n,coop_present_count(s));
 coop_append(out,&n," ready, need ");coop_append_uint(out,&n,s->min_players);
}

static void coop_result(CoopResult *r,int action,const char *text){
 unsigned int n=0;
 r->action=action;r->target_pos=0;r->msg[0]=0;
 coop_append(r->msg,&n,text);
}

/* A player object was placed by the engine (either spawn path) or chatted.
 * Registers/refreshes the member keyed by the stable client handle. Returns the
 * member index (its spawn slot) or -1 when the session is full/invalid. */
static int coop_session_seen(CoopSession *s,unsigned int client,unsigned int player,
  unsigned int object,const char *name,unsigned int now,CoopResult *r){
 int i=coop_find_client(s,client);unsigned int k;
 if(r)coop_result(r,COOP_ACT_NONE,"");
 if(!client||!player)return -1;
 /* Object/player reuse by a different client means the old member left. */
 for(k=0;k<COOP_SESSION_MAX_MEMBERS;k++){
  CoopMember *m=&s->m[k];
  if(m->used&&(int)k!=i&&m->client!=client&&
     ((object&&m->object==object)||m->player==player))coop_remove_member(s,k);
 }
 if(i<0){
  if(coop_present_count(s)>=s->max_players){
   s->full_rejects++;
   if(r){
    unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
    coop_append(r->msg,&n,"[Co-op] Session full (max ");
    coop_append_uint(r->msg,&n,s->max_players);coop_append(r->msg,&n,"); spectating only");
   }
   return -1;
  }
  for(k=0;k<COOP_SESSION_MAX_MEMBERS;k++)if(!s->m[k].used){i=(int)k;break;}
  if(i<0)return -1;
  s->m[i].used=1;s->m[i].client=client;s->m[i].join_seq=++s->join_counter;
  s->m[i].ready=0;s->m[i].away=0;
  coop_sanitize(s->m[i].name,name,COOP_NAME_MAX);
  s->m[i].player=player;s->m[i].object=object;s->m[i].seen_epoch=s->epoch;s->m[i].spawned=1;s->m[i].spawn_ms=now;
  coop_elect_host(s);
  if(r){
   unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,s->m[i].name);
   coop_append(r->msg,&n,s->phase==COOP_PHASE_LOBBY?" joined the lobby. Type !ready (help: !coop)":
                         " joined in progress");
  }
 }else{
  if(name)coop_sanitize(s->m[i].name,name,COOP_NAME_MAX);
  s->m[i].player=player;s->m[i].object=object;s->m[i].seen_epoch=s->epoch;s->m[i].spawned=1;s->m[i].spawn_ms=now;
 }
 /* First participant in a freshly loaded started world releases the barrier.
  * Intros are NOT synchronised in this MVP: the native multiplayer path does
  * not start the authored starting_cinematic, and we do not force it. */
 /* With an armed intro the barrier holds until coop_session_intro_tick
  * decides; announce once that the world is waiting for the players. */
 if(s->phase==COOP_PHASE_INTRO_PENDING&&s->started&&s->intro_armed){
  if(r&&r->action==COOP_ACT_NONE&&coop_present_count(s)){
   unsigned int n=0,k,ready=0;
   for(k=0;k<COOP_SESSION_MAX_MEMBERS;k++)if(s->m[k].used&&!s->m[k].away&&s->m[k].spawned&&s->m[k].seen_epoch==s->epoch)ready++;
   r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,coop_route_label(s,s->route_pos));
   coop_append(r->msg,&n," loaded. Intro starts when all players are in (");
   coop_append_uint(r->msg,&n,ready);coop_append(r->msg,&n,"/");coop_append_uint(r->msg,&n,coop_present_count(s));
   coop_append(r->msg,&n,")");
  }
  return i;
 }
 if(s->phase==COOP_PHASE_INTRO_PENDING&&s->started){
  s->phase=COOP_PHASE_GAMEPLAY;s->seq++;
  if(r){
   unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,coop_route_label(s,s->route_pos));
   coop_append(r->msg,&n," started (epoch ");coop_append_uint(r->msg,&n,s->epoch);
   coop_append(r->msg,&n,"). Intro skipped. Reach the exit together.");
  }
 }
 (void)now;
 return i;
}

/* Presence: objects currently in the engine's player list. A member whose
 * current-epoch object vanished has disconnected. Members not yet respawned
 * after a world load are kept until COOP_LOAD_GRACE_MS (slow joiners). */
static unsigned int coop_session_presence(CoopSession *s,const unsigned int *live,
  unsigned int count,unsigned int now,CoopResult *r){
 unsigned int i,k,removed=0;
 if(r)coop_result(r,COOP_ACT_NONE,"");
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  CoopMember *m=&s->m[i];int alive=0;
  if(!m->used)continue;
  if(m->seen_epoch==s->epoch&&m->spawned){
   for(k=0;k<count&&live;k++)if(live[k]==m->object){alive=1;break;}
  }else alive=(unsigned int)(now-s->epoch_ms)<COOP_LOAD_GRACE_MS;
  if(!alive){
   if(r&&!removed){
    unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
    coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,m->name);coop_append(r->msg,&n," left");
   }
   coop_remove_member(s,i);removed++;
  }
 }
 if(removed&&!coop_present_count(s)&&s->started){
  coop_reset_generation(s,now);
  if(r){
   unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   coop_append(r->msg,&n,"[Co-op] All players left; session reset to lobby (generation ");
   coop_append_uint(r->msg,&n,s->generation);coop_append(r->msg,&n,")");
  }
 }
 return removed;
}

/* Begin a server-side world change to route[target]. */
static void coop_begin_change(CoopSession *s,unsigned int target,unsigned int now,CoopResult *r,const char *why){
 unsigned int n=0;
 s->phase=COOP_PHASE_TRANSITION;s->target_pos=target;
 s->transition_epoch=s->epoch;s->transition_ms=now;s->seq++;
 r->action=COOP_ACT_CHANGE_WORLD;r->target_pos=target;r->msg[0]=0;
 coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,why);
 coop_append(r->msg,&n," -> ");
 if(target==COOP_TARGET_LOBBY){char l[16];coop_world_label(coop_target_world(s,target),l);coop_append(r->msg,&n,"lobby ");coop_append(r->msg,&n,l);}
 else coop_append(r->msg,&n,coop_route_label(s,target));
}

/* Difficulty names as the game's menu shows them (CoopSkill values 0-4). */
static const char *const coop_skill_names[5]={"Lullaby","Daydream","Sleepless","Cold Sweat","Nightmare"};
static int coop_skill_parse(const char *a,const char *b){
 static const char *const keys[5]={"lullaby","daydream","sleepless","coldsweat","nightmare"};
 char w[24];unsigned int i,n=0;
 if(a[0]>='0'&&a[0]<='4'&&!a[1]&&!b[0])return a[0]-'0';
 for(i=0;a[i]&&n<sizeof(w)-1;i++)w[n++]=a[i];
 for(i=0;b[i]&&n<sizeof(w)-1;i++)w[n++]=b[i];          /* "cold sweat" */
 w[n]=0;
 for(i=0;i<5;i++)if(coop_word(w,keys[i]))return (int)i;
 if(coop_word(w,"easy"))return 1;
 if(coop_word(w,"normal")||coop_word(w,"medium"))return 2;
 if(coop_word(w,"hard"))return 3;
 return -1;
}
/* Exits during play belong to the host: a native exit (mirror, exit volume)
 * set off by any other player is refused. The reply comes at the first
 * refusal in a world and again at most every COOP_HOST_EXIT_REPLY_MS, so a
 * later attempt does not look like a dead exit.
 * Exits fired by a script or by a non-player object (an intro's own
 * "trigger_generic CExitTrigger0") and every exit outside GAMEPLAY are left
 * to coop_session_exit. member: session index of the activator or -1. */
#define COOP_HOST_EXIT_REPLY_MS 5000u
static int coop_session_exit_by(CoopSession *s,int member,int is_player,unsigned int now,CoopResult *r){
 coop_result(r,COOP_ACT_NONE,"");
 if(s->phase!=COOP_PHASE_GAMEPLAY||!is_player)return 1;
 if(member>=0&&member==s->host)return 1;
 s->rejected_exits++;
 if(s->host_exit_epoch!=s->epoch+1u||now-s->host_exit_ms>=COOP_HOST_EXIT_REPLY_MS){
  s->host_exit_epoch=s->epoch+1u;s->host_exit_ms=now;
  coop_result(r,COOP_ACT_ANNOUNCE,"[Co-op] Only the host (*) can take the exit");
 }
 return 0;
}
/* Level-ending sequences belong to the host too: a trigger whose commands
 * reach the world's exit (a cutscene ending in "trigger_generic <exit>") is
 * refused for any other player before it runs, so it stays armed for the
 * host. The reply is repeated at most every COOP_HOST_SEQ_REPLY_MS while a
 * player stands in the trigger. Outside GAMEPLAY nothing is refused. */
#define COOP_HOST_SEQ_REPLY_MS 5000u
static int coop_session_sequence_by(CoopSession *s,int member,unsigned int now,CoopResult *r){
 coop_result(r,COOP_ACT_NONE,"");
 if(s->phase!=COOP_PHASE_GAMEPLAY)return 1;
 if(member>=0&&member==s->host)return 1;
 s->rejected_sequences++;
 if(!s->host_seq_said||now-s->host_seq_ms>=COOP_HOST_SEQ_REPLY_MS){
  s->host_seq_said=1;s->host_seq_ms=now;
  coop_result(r,COOP_ACT_ANNOUNCE,"[Co-op] Only the host (*) can start this sequence (it ends the level)");
 }
 return 0;
}
/* !abort: back to a fresh lobby. Readiness is cleared and the campaign
 * selection returns to its default (full campaign). */
static void coop_session_lobby_reset(CoopSession *s){
 unsigned int i;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)s->m[i].ready=0;
 if(s->campaign)coop_session_select(s,COOP_SEL_FULL,0);
 s->seq++;
}
static int coop_in_lobby_world(const CoopSession *s){
 char l[16];coop_world_label(coop_target_world(s,COOP_TARGET_LOBBY),l);
 /* No world loaded through the session yet: the launcher started the lobby. */
 return !s->world_label[0]||(l[0]&&coop_word(s->world_label,l));
}
static void coop_abort_change(CoopSession *s,unsigned int now,CoopResult *r){
 s->start_pending=0;s->lobby_pending=1;s->abort_after_load=0;
 coop_session_lobby_reset(s);
 coop_begin_change(s,COOP_TARGET_LOBBY,now,r,"Host aborted");
}
/* !changelevel <world> (host, campaign mode): every player goes to that
 * WORLDORDER world, from the lobby or during play. The campaign then carries
 * on from there (the selection becomes "from <world>" to the end), so natural
 * exits lead to its successor; !abort returns to the lobby and the default
 * selection. The change is the same server-side world change as an exit
 * (loading picture, spawn hold, !abort during the load). */
static void coop_session_changelevel(CoopSession *s,int me,const char *arg,unsigned int now,CoopResult *r){
 int g;unsigned int m=0,present=coop_present_count(s),k;
 char why[COOP_MSG_MAX];
 if(!s->campaign){coop_result(r,COOP_ACT_REPLY,"[Co-op] !changelevel needs CoopCampaign=1");return;}
 while(*arg==' '||*arg=='\t')arg++;
 if(!*arg){coop_result(r,COOP_ACT_REPLY,"[Co-op] Use: !changelevel <world>, e.g. !changelevel R1M2A or worlds\\realm1\\R1M2A (host)");return;}
 g=coop_campaign_find(arg);
 if(g<0){coop_result(r,COOP_ACT_REPLY,"[Co-op] Unknown world. Use a campaign world R1M0..R5M1B: !changelevel R1M2A or worlds\\realm1\\R1M2A");return;}
 if(me<0||me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can change the level");return;}
 if(s->phase==COOP_PHASE_TRANSITION||s->phase==COOP_PHASE_WORLD_LOADING){coop_result(r,COOP_ACT_REPLY,"[Co-op] A world change is already in progress");return;}
 if(s->phase==COOP_PHASE_LOBBY&&present<s->min_players){
  r->action=COOP_ACT_REPLY;r->msg[0]=0;
  coop_append(r->msg,&m,"[Co-op] Need ");coop_append_uint(r->msg,&m,s->min_players);
  coop_append(r->msg,&m," players to start, ");coop_append_uint(r->msg,&m,present);coop_append(r->msg,&m," here");
  return;
 }
 if(!coop_session_select(s,COOP_SEL_WORLD,(unsigned int)g)){coop_result(r,COOP_ACT_REPLY,"[Co-op] Campaign selection failed");return;}
 s->seq++;
 if(s->phase==COOP_PHASE_LOBBY){
  for(k=0;k<COOP_SESSION_MAX_MEMBERS;k++)s->m[k].ready=0;
  s->started=1;s->start_pending=1;
 }else s->start_pending=0;
 s->lobby_pending=0;s->abort_after_load=0;
 why[0]=0;coop_append(why,&m,"Host changed level (campaign continues from ");
 coop_append(why,&m,coop_route_label(s,0));coop_append(why,&m,")");
 coop_begin_change(s,0,now,r,why);
}
/* Chat command from a known member. Text is untrusted. */
static void coop_session_command(CoopSession *s,unsigned int client,const char *text,
  unsigned int now,CoopResult *r){
 char cmd[16],arg[2][12];unsigned int i=0,n=0,len,argc=0;int me=coop_find_client(s,client);
 coop_result(r,COOP_ACT_NONE,"");
 len=coop_strlen(text,COOP_COMMAND_MAX+1);
 if(!text||len>COOP_COMMAND_MAX)return;
 while(i<len&&(text[i]==' '||text[i]=='\t'))i++;
 if(i>=len||text[i]!='!')return;
 i++;
 /* !changelevel takes a world path (backslashes, dots): parsed on its own. */
 {
  static const char kw[]="changelevel";unsigned int k;
  for(k=0;kw[k]&&i+k<len&&coop_lower(text[i+k])==kw[k];k++){}
  if(!kw[k]&&(i+k==len||text[i+k]==' '||text[i+k]=='\t')){coop_session_changelevel(s,me,text+i+k,now,r);return;}
 }
 while(i<len&&n<sizeof(cmd)-1){
  char c=coop_lower(text[i]);
  if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))break;
  cmd[n++]=c;i++;
 }
 cmd[n]=0;
 while(i<len&&(text[i]==' '||text[i]=='\t'))i++;
 /* At most two short alphanumeric argument words (only !campaign uses them). */
 while(i<len&&argc<2){
  unsigned int k=0;
  while(i<len&&k<sizeof(arg[0])-1){
   char c=coop_lower(text[i]);
   if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))break;
   arg[argc][k++]=c;i++;
  }
  arg[argc][k]=0;
  if(!k)break;
  argc++;
  while(i<len&&(text[i]==' '||text[i]=='\t'))i++;
 }
 if(!n)return;
#define COOP_IS(word) coop_word(cmd,word)
 /* Anything else is not a command, except that a malformed !campaign gets
  * the help reply instead of silence. */
 if(i!=len&&!COOP_IS("campaign")&&!COOP_IS("difficulty")&&!COOP_IS("skill"))return;
 if(argc&&!COOP_IS("campaign")&&!COOP_IS("difficulty")&&!COOP_IS("skill"))return; /* only these take arguments */
 if(COOP_IS("coop")||COOP_IS("status")){
  r->action=COOP_ACT_REPLY;coop_status(s,r->msg);return;
 }
 if(COOP_IS("difficulty")||COOP_IS("skill")){
  unsigned int m=0;int v;
  if(!argc&&i==len){
   r->action=COOP_ACT_REPLY;r->msg[0]=0;
   coop_append(r->msg,&m,"[Co-op] Difficulty: ");
   coop_append(r->msg,&m,s->skill>=0?coop_skill_names[s->skill]:"server default");
   coop_append(r->msg,&m,". Host: !difficulty lullaby|daydream|sleepless|coldsweat|nightmare");
   return;
  }
  v=i==len?coop_skill_parse(arg[0],argc>1?arg[1]:""):-1;
  if(v<0){coop_result(r,COOP_ACT_REPLY,"[Co-op] Use: !difficulty lullaby|daydream|sleepless|coldsweat|nightmare (or 0-4)");return;}
  if(me<0||me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can set the difficulty");return;}
  if(s->phase!=COOP_PHASE_LOBBY){coop_result(r,COOP_ACT_REPLY,"[Co-op] Difficulty can only be changed in the lobby");return;}
  s->skill=v;s->seq++;
  r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
  coop_append(r->msg,&m,"[Co-op] Difficulty set to ");coop_append(r->msg,&m,coop_skill_names[v]);
  coop_append(r->msg,&m," (applies from the next world)");
  return;
 }
 if(COOP_IS("help")){
  coop_result(r,COOP_ACT_REPLY,s->campaign?
   "[Co-op] !coop !abort !next !difficulty !campaign full|water|earth|air|fire|nightmare !changelevel !kill !class":
   "[Co-op] kpc-coop/1: !ready !unready !leave !join !start !next !abort !difficulty (host) !coop");
  return;
 }
 if(COOP_IS("campaign")){
  /* !campaign full            full playthrough from R1M0, starts now
   * !campaign <realm>         that realm's first world, continues to the end,
   *                           starts now (realm: water earth air fire
   *                           nightmare or 1-5; "from <realm>" is an alias)
   * !campaign                 anyone: current selection and help
   * Starting skips !ready; the minimum player count still applies. */
  unsigned int mode=COOP_SEL_FULL,realm=0,m=0,present=coop_present_count(s);
  if(!s->campaign){coop_result(r,COOP_ACT_REPLY,"[Co-op] !campaign needs CoopCampaign=1 (now: MVP route R1M2B>R1M2C>R1M3A)");return;}
  if(!argc&&i==len){
   r->action=COOP_ACT_REPLY;r->msg[0]=0;
   coop_append(r->msg,&m,"[Co-op] Selected: ");coop_selection_text(s,r->msg,&m);
   coop_append(r->msg,&m,". Host: !campaign full|water|earth|air|fire|nightmare");
   return;
  }
  if(i==len&&argc==1&&coop_word(arg[0],"full"))mode=COOP_SEL_FULL;
  else if(i==len&&argc==1&&(realm=coop_realm_parse(arg[0]))!=0)mode=COOP_SEL_FROM;
  else if(i==len&&argc==2&&coop_word(arg[0],"from")&&(realm=coop_realm_parse(arg[1]))!=0)mode=COOP_SEL_FROM;
  else{coop_result(r,COOP_ACT_REPLY,"[Co-op] Use: !campaign full, or !campaign water|earth|air|fire|nightmare to start at a realm");return;}
  if(me<0||me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can start the campaign");return;}
  if(s->phase!=COOP_PHASE_LOBBY){coop_result(r,COOP_ACT_REPLY,"[Co-op] Already playing; the host can !abort back to the lobby first");return;}
  if(present<s->min_players){
   r->action=COOP_ACT_REPLY;r->msg[0]=0;
   coop_append(r->msg,&m,"[Co-op] Need ");coop_append_uint(r->msg,&m,s->min_players);
   coop_append(r->msg,&m," players to start, ");coop_append_uint(r->msg,&m,present);coop_append(r->msg,&m," here");
   return;
  }
  /* Realm 1 from its first world is the full playthrough. */
  if(mode==COOP_SEL_FROM&&realm==1u){mode=COOP_SEL_FULL;realm=0;}
  if(!coop_session_select(s,mode,realm)){coop_result(r,COOP_ACT_REPLY,"[Co-op] Campaign selection failed");return;}
  s->seq++;
  for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++)s->m[i].ready=0;
  s->started=1;s->start_pending=1;s->lobby_pending=0;
  {
   char why[COOP_MSG_MAX];unsigned int w=0;why[0]=0;
   coop_append(why,&w,"Host started ");coop_selection_text(s,why,&w);
   coop_begin_change(s,s->start_pos<s->route_len?s->start_pos:0u,now,r,why);
  }
  return;
 }
 if(me<0){
  if(COOP_IS("ready")||COOP_IS("unready")||COOP_IS("leave")||COOP_IS("join")||COOP_IS("start")||COOP_IS("next")||COOP_IS("abort"))
   coop_result(r,COOP_ACT_REPLY,"[Co-op] You are not in the session (full or not spawned)");
  return;
 }
 if(COOP_IS("ready")||COOP_IS("unready")){
  unsigned char want=COOP_IS("ready");
  if(s->phase!=COOP_PHASE_LOBBY){coop_result(r,COOP_ACT_REPLY,"[Co-op] Session already started");return;}
  if(s->m[me].away){coop_result(r,COOP_ACT_REPLY,"[Co-op] You are away; type !join first");return;}
  if(s->m[me].ready==want){r->action=COOP_ACT_REPLY;coop_status(s,r->msg);return;}
  s->m[me].ready=want;s->seq++;
  r->action=COOP_ACT_ANNOUNCE;coop_status(s,r->msg);return;
 }
 if(COOP_IS("leave")||COOP_IS("join")){
  unsigned char away=COOP_IS("leave");
  if(s->m[me].away==away){r->action=COOP_ACT_REPLY;coop_status(s,r->msg);return;}
  if(!away&&coop_present_count(s)>=s->max_players){coop_result(r,COOP_ACT_REPLY,"[Co-op] Session is full");return;}
  s->m[me].away=away;s->m[me].ready=0;s->seq++;coop_elect_host(s);
  r->action=COOP_ACT_ANNOUNCE;coop_status(s,r->msg);return;
 }
 if(COOP_IS("start")){
  unsigned int present=coop_present_count(s),ready=coop_ready_count(s);
  if(s->phase!=COOP_PHASE_LOBBY){coop_result(r,COOP_ACT_REPLY,"[Co-op] Already started");return;}
  if(me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can start");return;}
  if(present<s->min_players||ready!=present){
   r->action=COOP_ACT_REPLY;coop_status(s,r->msg);return;
  }
  s->started=1;s->start_pending=1;s->lobby_pending=0;
  coop_begin_change(s,s->start_pos<s->route_len?s->start_pos:0u,now,r,"All ready. Starting");
  return;
 }
 if(COOP_IS("abort")){
  /* Host: reset everything back to the lobby from any state. */
  if(me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can abort to the lobby");return;}
  if(s->phase==COOP_PHASE_TRANSITION||s->phase==COOP_PHASE_WORLD_LOADING){
   if(s->lobby_pending){coop_result(r,COOP_ACT_REPLY,"[Co-op] Already returning to the lobby");return;}
   s->abort_after_load=1;s->seq++;
   coop_result(r,COOP_ACT_ANNOUNCE,"[Co-op] Host aborted: back to the lobby once this world has loaded");
   return;
  }
  if(s->phase==COOP_PHASE_LOBBY&&coop_in_lobby_world(s)){
   unsigned int m=0;
   s->started=0;s->start_pending=0;s->abort_after_load=0;
   coop_session_lobby_reset(s);
   r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   {char l[16];coop_world_label(coop_target_world(s,COOP_TARGET_LOBBY),l);
    coop_append(r->msg,&m,"[Co-op] Lobby reset (");coop_append(r->msg,&m,l);}
   coop_append(r->msg,&m,s->campaign?"). Host: !campaign full|water|earth|air|fire|nightmare":"). Everyone !ready, host !start");
   return;
  }
  coop_abort_change(s,now,r);
  return;
 }
 if(COOP_IS("next")){
  if(me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can force the exit");return;}
  if(s->phase!=COOP_PHASE_GAMEPLAY){coop_result(r,COOP_ACT_REPLY,"[Co-op] Not in play; exit ignored");return;}
  if(s->route_pos+1>=s->route_len){coop_result(r,COOP_ACT_REPLY,s->campaign?"[Co-op] Last world of the selected route":"[Co-op] End of the MVP route");return;}
  if(s->transition_epoch==s->epoch){s->duplicate_exits++;return;}
  s->start_pending=0;
  coop_begin_change(s,s->route_pos+1,now,r,"Host override exit");
  return;
 }
#undef COOP_IS
}

/* A native exit trigger fired (any client). Server validates destination
 * against the fixed route and debounces to one transition per epoch. */
static void coop_session_exit(CoopSession *s,const char *next_world,unsigned int now,CoopResult *r){
 int idx;
 coop_result(r,COOP_ACT_NONE,"");
 if(s->phase==COOP_PHASE_TRANSITION||s->phase==COOP_PHASE_WORLD_LOADING||
    (s->epoch&&s->transition_epoch==s->epoch)){s->duplicate_exits++;return;}
 /* A realm-opening INTRO world (R1M0-R4M0) leaves through its own script:
  * the intro's last event is "trigger_generic CExitTrigger0". */
 if(s->phase==COOP_PHASE_INTRO_RUNNING&&s->route_pos<s->route_len&&(s->route[s->route_pos].flags&COOP_W_INTRO)){}
 else if(s->phase!=COOP_PHASE_GAMEPLAY){s->rejected_exits++;
  if(s->rejected_exits==1)coop_result(r,COOP_ACT_REPLY,"[Co-op] Exit ignored: session not started (!coop)");
  return;}
 idx=coop_route_index(s,next_world);
 /* Campaign: an exit from the last world of the selection to its WORLDORDER
  * successor (or any exit from the final campaign world) completes it. */
 if(s->campaign&&s->route_pos+1u>=s->route_len){
  int g=coop_campaign_index(s,s->route_pos);
  int finale=g<0||(unsigned int)g+1u>=COOP_CAMPAIGN_LEN;
  char w[COOP_WORLD_MAX];
  if(coop_world_normalize(next_world,w)&&(finale||coop_world_equal(next_world,coop_campaign_worlds[g+1].world))){
   s->start_pending=0;s->lobby_pending=1;s->completed++;
   coop_begin_change(s,COOP_TARGET_LOBBY,now,r,"Selected route complete");
   return;
  }
 }
 if(idx<0||(unsigned int)idx!=s->route_pos+1){
  s->rejected_exits++;
  /* An exit volume can fire repeatedly while touched: reply once per epoch. */
  if(s->reject_reply_epoch!=s->epoch+1u){
   s->reject_reply_epoch=s->epoch+1u;
   coop_result(r,COOP_ACT_REPLY,s->campaign?"[Co-op] Exit destination is not the next world of the selected route; not changing world":
                                          "[Co-op] Exit destination is outside the MVP route; not changing world");
  }
  return;
 }
 s->start_pending=0;
 coop_begin_change(s,(unsigned int)idx,now,r,"Exit reached");
}

/* Called before the native NextLevel. Increments the map epoch. */
static void coop_session_world_loading(CoopSession *s,unsigned int now){
 unsigned int i;
 s->epoch++;s->epoch_ms=now;s->phase=COOP_PHASE_WORLD_LOADING;s->seq++;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){s->m[i].spawned=0;s->m[i].object=0;s->m[i].player=0;s->m[i].music_pending=0;}
}

/* ---------- per-player world music delivery ----------
 * The original WorldProperties broadcasts the world's music start once, at
 * world initialisation, to the clients connected at that instant. Players who
 * join later never receive it, and players reloading the world lose it in their
 * own world switch. The session therefore delivers the same start to each
 * member once per epoch, a short delay after that member spawned. */
#define COOP_MUSIC_DELAY_MS 1500u
/* Member i spawned: schedule delivery unless already delivered this epoch. */
static void coop_session_music_schedule(CoopSession *s,int i,unsigned int now){
 CoopMember *m;
 if(i<0||(unsigned int)i>=COOP_SESSION_MAX_MEMBERS)return;
 m=&s->m[i];
 if(!m->used||m->music_pending||m->music_epoch==s->epoch+1u)return;
 m->music_pending=1;m->music_due_ms=now+COOP_MUSIC_DELAY_MS;
}
/* Returns a member whose delivery is due now (and marks it delivered for this
 * epoch), or -1. music_epoch_valid: a music snapshot exists for this epoch;
 * without one nothing is sent and the request is dropped (no stale track). */
static int coop_session_music_take(CoopSession *s,unsigned int now,int music_epoch_valid){
 unsigned int i;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  CoopMember *m=&s->m[i];
  if(!m->used||!m->music_pending||(int)(now-m->music_due_ms)<0)continue;
  m->music_pending=0;
  if(!music_epoch_valid)continue;
  m->music_epoch=s->epoch+1u;
  return (int)i;
 }
 return -1;
}
/* Called after the native NextLevel returned with the loaded world name. */
static void coop_session_world_loaded(CoopSession *s,const char *world){
 int idx=coop_route_index(s,world);
 coop_world_label(world,s->world_label);
 s->start_pending=0;s->intro_armed=0;
 if(s->lobby_pending){
  /* Completion or host abort: back to the lobby, members kept, unready. */
  s->lobby_pending=0;s->started=0;s->phase=COOP_PHASE_LOBBY;s->generation++;
  for(idx=0;idx<(int)COOP_SESSION_MAX_MEMBERS;idx++)s->m[idx].ready=0;
  return;
 }
 if(idx>=0)s->route_pos=(unsigned int)idx;
 if(s->started&&idx>=0)s->phase=COOP_PHASE_INTRO_PENDING;
 else{
  /* A native/console level change outside the route ends the session. */
  if(s->started&&idx<0)s->started=0;
  s->phase=COOP_PHASE_LOBBY;
  for(idx=0;idx<(int)COOP_SESSION_MAX_MEMBERS;idx++)s->m[idx].ready=0;
 }
}

/* Called right after coop_session_world_loaded: a host !abort made during the
 * change is carried out now (a CHANGE_WORLD back to the lobby). */
static void coop_session_after_load(CoopSession *s,unsigned int now,CoopResult *r){
 coop_result(r,COOP_ACT_NONE,"");
 if(!s->abort_after_load)return;
 s->abort_after_load=0;
 if(s->phase==COOP_PHASE_LOBBY&&coop_in_lobby_world(s)){coop_session_lobby_reset(s);return;}
 coop_abort_change(s,now,r);
}

/* ---------- synchronised original intro (CoopIntro) ----------
 * The original WorldProperties plays its starting_cinematic only in single
 * player. The session instead plays it once, server-side, when every present
 * member has entered the new world; the script's camera, letterbox, subtitle
 * and input events are broadcast to all clients by the original code. */
#define COOP_INTRO_BARRIER_MS 30000u  /* start anyway once one player is in */
#define COOP_INTRO_MAX_MS 120000u     /* release clients if the script overruns */
/* Proxy: this epoch's world has a playable intro (call after world_loaded). */
static void coop_session_intro_arm(CoopSession *s,int playable){
 s->intro_armed=s->phase==COOP_PHASE_INTRO_PENDING&&s->started&&playable;
}
/* Barrier/overrun decision, called every server frame. Returns
 * COOP_ACT_INTRO_START (play now) or COOP_ACT_INTRO_ABORT (release now). */
static int coop_session_intro_tick(CoopSession *s,unsigned int now,CoopResult *r){
 unsigned int i,present=0,ready=0,last=0;
 coop_result(r,COOP_ACT_NONE,"");
 if(s->phase==COOP_PHASE_INTRO_RUNNING){
  if((unsigned int)(now-s->intro_ms)<COOP_INTRO_MAX_MS)return 0;
  s->phase=COOP_PHASE_GAMEPLAY;s->intro_armed=0;s->seq++;
  coop_result(r,COOP_ACT_INTRO_ABORT,"[Co-op] Intro overran; controls released. Reach the exit together.");
  return COOP_ACT_INTRO_ABORT;
 }
 if(s->phase!=COOP_PHASE_INTRO_PENDING||!s->intro_armed)return 0;
 for(i=0;i<COOP_SESSION_MAX_MEMBERS;i++){
  const CoopMember *m=&s->m[i];
  if(!m->used||m->away)continue;
  present++;
  if(m->spawned&&m->seen_epoch==s->epoch){ready++;if(!last||(int)(m->spawn_ms-last)>0)last=m->spawn_ms;}
 }
 if(!ready)return 0;   /* a zero-player world never consumes the intro */
 if(!(ready==present&&(unsigned int)(now-last)>=s->intro_settle_ms)&&
    (unsigned int)(now-s->epoch_ms)<COOP_INTRO_BARRIER_MS)return 0;
 s->phase=COOP_PHASE_INTRO_RUNNING;s->intro_ms=now;s->seq++;
 {unsigned int n=0;r->action=COOP_ACT_INTRO_START;r->msg[0]=0;
  coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,coop_route_label(s,s->route_pos));
  coop_append(r->msg,&n," intro starting (epoch ");coop_append_uint(r->msg,&n,s->epoch);
  coop_append(r->msg,&n,", ");coop_append_uint(r->msg,&n,ready);coop_append(r->msg,&n,"/");
  coop_append_uint(r->msg,&n,present);coop_append(r->msg,&n," players)");}
 return COOP_ACT_INTRO_START;
}
/* The script finished (finished=1) or could not be started (0). */
static void coop_session_intro_done(CoopSession *s,int finished,CoopResult *r){
 coop_result(r,COOP_ACT_NONE,"");
 if(s->phase!=COOP_PHASE_INTRO_RUNNING)return;
 s->phase=COOP_PHASE_GAMEPLAY;s->intro_armed=0;s->seq++;
 coop_result(r,COOP_ACT_ANNOUNCE,finished?"[Co-op] Intro finished. Reach the exit together.":
                                         "[Co-op] Intro could not start; playing without it.");
}
/* Native world change did not happen in time: return to play, allow retry. */
static int coop_session_tick(CoopSession *s,unsigned int now,CoopResult *r){
 if(r)coop_result(r,COOP_ACT_NONE,"");
 if(s->phase==COOP_PHASE_TRANSITION&&(unsigned int)(now-s->transition_ms)>=COOP_TRANSITION_TIMEOUT_MS){
  s->phase=s->start_pending?COOP_PHASE_LOBBY:COOP_PHASE_GAMEPLAY;
  if(s->start_pending)s->started=0;
  s->start_pending=0;s->transition_epoch=0;s->seq++;
  if(r)coop_result(r,COOP_ACT_ANNOUNCE,"[Co-op] World change timed out; host may retry");
  return 1;
 }
 return 0;
}
/* The proxy could not prepare the native change (unexpected maplist etc.). */
static void coop_session_change_failed(CoopSession *s,CoopResult *r){
 s->phase=s->start_pending?COOP_PHASE_LOBBY:COOP_PHASE_GAMEPLAY;
 if(s->start_pending)s->started=0;
 s->start_pending=0;s->transition_epoch=0;s->seq++;
 coop_result(r,COOP_ACT_ANNOUNCE,"[Co-op] Server could not change world (maplist mismatch); see log");
}
#endif
