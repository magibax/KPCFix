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
#define COOP_ROUTE_LEN 3u
#define COOP_TRANSITION_TIMEOUT_MS 30000u
#define COOP_LOAD_GRACE_MS 90000u

typedef enum {
 COOP_PHASE_LOBBY=1,          /* WAITING_FOR_READY; session not started */
 COOP_PHASE_WORLD_LOADING=2,  /* native NextLevel in progress */
 COOP_PHASE_INTRO_PENDING=3,  /* world loaded, waiting for first participant */
 COOP_PHASE_GAMEPLAY=4,
 COOP_PHASE_TRANSITION=5      /* server-requested world change pending */
} CoopPhase;

typedef enum {
 COOP_ACT_NONE=0,
 COOP_ACT_ANNOUNCE=1,     /* broadcast msg */
 COOP_ACT_CHANGE_WORLD=2, /* broadcast msg, then load route[target] natively */
 COOP_ACT_REPLY=3         /* informational reply (broadcast; chat has no unicast here) */
} CoopAction;

typedef struct {
 unsigned int client,player,object,join_seq,seen_epoch;
 unsigned char used,ready,away,spawned;
 char name[COOP_NAME_MAX];
 /* Per-player world music delivery (see coop_session_music_*). */
 unsigned int music_epoch;    /* epoch+1 of the last delivered music start (0 = none) */
 unsigned int music_due_ms;   /* delivery time when music_pending */
 unsigned char music_pending;
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
 unsigned int reject_reply_epoch;    /* epoch+1 of the last off-route reply (0 = none) */
 int host;                          /* member index or -1 */
 CoopMember m[COOP_SESSION_MAX_MEMBERS];
} CoopSession;

typedef struct {
 int action;
 unsigned int target_pos;
 char msg[COOP_MSG_MAX];
} CoopResult;

static const char *const coop_route[COOP_ROUTE_LEN]={"realm1/r1m2b","realm1/r1m2c","realm1/r1m3a"};

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
/* Returns the route index of a world or -1. */
static int coop_route_index(const char *world){
 unsigned int i;
 for(i=0;i<COOP_ROUTE_LEN;i++)if(coop_world_equal(world,coop_route[i]))return (int)i;
 return -1;
}
static const char *coop_route_label(unsigned int pos){
 static const char *const label[COOP_ROUTE_LEN]={"R1M2B","R1M2C","R1M3A"};
 return pos<COOP_ROUTE_LEN?label[pos]:"?";
}
static const char *coop_phase_name(int phase){
 switch(phase){
  case COOP_PHASE_LOBBY:return "LOBBY";
  case COOP_PHASE_WORLD_LOADING:return "LOADING";
  case COOP_PHASE_INTRO_PENDING:return "INTRO_PENDING";
  case COOP_PHASE_GAMEPLAY:return "PLAYING";
  case COOP_PHASE_TRANSITION:return "TRANSITION";
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
 coop_append(out,&n," ");coop_append(out,&n,coop_route_label(s->route_pos));
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
  s->m[i].player=player;s->m[i].object=object;s->m[i].seen_epoch=s->epoch;s->m[i].spawned=1;
  coop_elect_host(s);
  if(r){
   unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,s->m[i].name);
   coop_append(r->msg,&n,s->phase==COOP_PHASE_LOBBY?" joined the lobby. Type !ready (help: !coop)":
                         " joined in progress");
  }
 }else{
  if(name)coop_sanitize(s->m[i].name,name,COOP_NAME_MAX);
  s->m[i].player=player;s->m[i].object=object;s->m[i].seen_epoch=s->epoch;s->m[i].spawned=1;
 }
 /* First participant in a freshly loaded started world releases the barrier.
  * Intros are NOT synchronised in this MVP: the native multiplayer path does
  * not start the authored starting_cinematic, and we do not force it. */
 if(s->phase==COOP_PHASE_INTRO_PENDING&&s->started){
  s->phase=COOP_PHASE_GAMEPLAY;s->seq++;
  if(r){
   unsigned int n=0;r->action=COOP_ACT_ANNOUNCE;r->msg[0]=0;
   coop_append(r->msg,&n,"[Co-op] ");coop_append(r->msg,&n,coop_route_label(s->route_pos));
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
 coop_append(r->msg,&n," -> ");coop_append(r->msg,&n,coop_route_label(target));
}

/* Chat command from a known member. Text is untrusted. */
static void coop_session_command(CoopSession *s,unsigned int client,const char *text,
  unsigned int now,CoopResult *r){
 char cmd[16];unsigned int i=0,n=0,len;int me=coop_find_client(s,client);
 coop_result(r,COOP_ACT_NONE,"");
 len=coop_strlen(text,COOP_COMMAND_MAX+1);
 if(!text||len>COOP_COMMAND_MAX)return;
 while(i<len&&(text[i]==' '||text[i]=='\t'))i++;
 if(i>=len||text[i]!='!')return;
 i++;
 while(i<len&&n<sizeof(cmd)-1){
  char c=coop_lower(text[i]);
  if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))break;
  cmd[n++]=c;i++;
 }
 cmd[n]=0;
 while(i<len&&(text[i]==' '||text[i]=='\t'))i++;
 if(!n||i!=len)return;                 /* trailing arguments are not accepted */
#define COOP_IS(word) coop_word(cmd,word)
 if(COOP_IS("coop")||COOP_IS("status")){
  r->action=COOP_ACT_REPLY;coop_status(s,r->msg);return;
 }
 if(COOP_IS("help")){
  coop_result(r,COOP_ACT_REPLY,"[Co-op] kpc-coop/1: !ready !unready !leave !join !start(host) !next(host) !coop");
  return;
 }
 if(me<0){
  if(COOP_IS("ready")||COOP_IS("unready")||COOP_IS("leave")||COOP_IS("join")||COOP_IS("start")||COOP_IS("next"))
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
  s->started=1;s->start_pending=1;
  coop_begin_change(s,0,now,r,"All ready. Starting");
  return;
 }
 if(COOP_IS("next")){
  if(me!=s->host){coop_result(r,COOP_ACT_REPLY,"[Co-op] Only the host (*) can force the exit");return;}
  if(s->phase!=COOP_PHASE_GAMEPLAY){coop_result(r,COOP_ACT_REPLY,"[Co-op] Not in play; exit ignored");return;}
  if(s->route_pos+1>=COOP_ROUTE_LEN){coop_result(r,COOP_ACT_REPLY,"[Co-op] End of the MVP route");return;}
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
 if(s->phase!=COOP_PHASE_GAMEPLAY){s->rejected_exits++;
  if(s->rejected_exits==1)coop_result(r,COOP_ACT_REPLY,"[Co-op] Exit ignored: session not started (!coop)");
  return;}
 idx=coop_route_index(next_world);
 if(idx<0||(unsigned int)idx!=s->route_pos+1){
  s->rejected_exits++;
  /* An exit volume can fire repeatedly while touched: reply once per epoch. */
  if(s->reject_reply_epoch!=s->epoch+1u){
   s->reject_reply_epoch=s->epoch+1u;
   coop_result(r,COOP_ACT_REPLY,"[Co-op] Exit destination is outside the MVP route; not changing world");
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
 int idx=coop_route_index(world);
 s->start_pending=0;
 if(idx>=0)s->route_pos=(unsigned int)idx;
 if(s->started&&idx>=0)s->phase=COOP_PHASE_INTRO_PENDING;
 else{
  /* A native/console level change outside the route ends the session. */
  if(s->started&&idx<0)s->started=0;
  s->phase=COOP_PHASE_LOBBY;
  for(idx=0;idx<(int)COOP_SESSION_MAX_MEMBERS;idx++)s->m[idx].ready=0;
 }
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
