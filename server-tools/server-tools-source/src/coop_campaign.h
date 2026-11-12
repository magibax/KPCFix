#ifndef KPC_COOP_CAMPAIGN_H
#define KPC_COOP_CAMPAIGN_H
/* Campaign graph for the co-op session ([Coop] CoopCampaign=1).
 *
 * Source of truth: the original WORLDS\WORLDORDER.TXT in PSYCHO.REZ (stock
 * 1.13), which "specifies the order that maps play in in the single player
 * game": 48 worlds, realm folders 1-5 (WORLDORDER realm numbers 0 water,
 * 1 earth, 2 air, 3 fire, 4 nightmare), stage numbers, and the INTRO /
 * WORLDMAP flags. The names below are copied from that file, in its order.
 * No world is invented from numbering. See coop/CAMPAIGN-MAP-INVENTORY.md.
 *
 * Selection (host commands, see coop_session_command):
 *   full            every world from realm1/r1m0 to realm5/r5m1b (default)
 *   from <realm>    first world of <realm> to the end of the campaign
 *   realm <realm>   only the worlds of <realm>
 * A route is a contiguous slice [first, last] of this table, so the session's
 * route position and "next world" logic are the same as for the MVP route. */
#define COOP_W_INTRO    0x01u   /* WORLDORDER "INTRO": realm-opening cinematic map */
#define COOP_W_WORLDMAP 0x02u   /* WORLDORDER "WORLDMAP": world map after the stage */
#define COOP_W_STAGE1ST 0x04u   /* WORLDORDER "First Map In Stage" = 1 */
#define COOP_W_INTRO_OK 0x08u   /* starting_cinematic may play in co-op (R1M3A: user-tested;
                                  * R1M0-R4M0: required for progression, each ends with
                                  * "trigger_generic CExitTrigger0"; not yet client-tested) */
#define COOP_REALMS 5u

typedef struct {
 const char *world;      /* normalised WORLDORDER path without "worlds/" */
 const char *label;      /* upper-case short name used in chat and logs */
 unsigned char realm;    /* folder realm 1-5 */
 unsigned char stage;    /* WORLDORDER stage number */
 unsigned char flags;
} CoopWorld;

#define CW(r,m,realm,stage,flags) {"realm" #realm "/" r,m,realm,stage,flags}
static const CoopWorld coop_campaign_worlds[]={
 CW("r1m0","R1M0",1,0,COOP_W_INTRO|COOP_W_STAGE1ST|COOP_W_INTRO_OK),
 CW("r1m1a","R1M1A",1,0,0),CW("r1m1b","R1M1B",1,0,COOP_W_WORLDMAP),
 CW("r1m2a","R1M2A",1,1,COOP_W_STAGE1ST),CW("r1m2b","R1M2B",1,1,0),CW("r1m2c","R1M2C",1,1,COOP_W_WORLDMAP),
 CW("r1m3a","R1M3A",1,2,COOP_W_STAGE1ST|COOP_W_INTRO_OK),CW("r1m3b","R1M3B",1,2,0),CW("r1m3c","R1M3C",1,2,COOP_W_WORLDMAP),
 CW("r1m4a","R1M4A",1,3,COOP_W_STAGE1ST),CW("r1m4b","R1M4B",1,3,0),CW("r1m4c","R1M4C",1,3,COOP_W_WORLDMAP),
 CW("r2m0","R2M0",2,0,COOP_W_INTRO|COOP_W_STAGE1ST|COOP_W_INTRO_OK),
 CW("r2m1a","R2M1A",2,0,0),CW("r2m1b","R2M1B",2,0,0),CW("r2m1c","R2M1C",2,0,COOP_W_WORLDMAP),
 CW("r2m2a","R2M2A",2,1,COOP_W_STAGE1ST),CW("r2m2b","R2M2B",2,1,0),CW("r2m2c","R2M2C",2,1,COOP_W_WORLDMAP),
 CW("r2m3a","R2M3A",2,2,COOP_W_STAGE1ST),CW("r2m3b","R2M3B",2,2,0),CW("r2m3c","R2M3C",2,2,COOP_W_WORLDMAP),
 CW("r2m4a","R2M4A",2,3,COOP_W_STAGE1ST),CW("r2m4b","R2M4B",2,3,0),CW("r2m4c","R2M4C",2,3,COOP_W_WORLDMAP),
 CW("r3m0","R3M0",3,0,COOP_W_INTRO|COOP_W_STAGE1ST|COOP_W_INTRO_OK),
 CW("r3m1a","R3M1A",3,0,0),CW("r3m1b","R3M1B",3,0,COOP_W_WORLDMAP),
 CW("r3m2a","R3M2A",3,1,COOP_W_STAGE1ST),CW("r3m2b","R3M2B",3,1,COOP_W_WORLDMAP),
 CW("r3m3a","R3M3A",3,2,COOP_W_STAGE1ST),CW("r3m3b","R3M3B",3,2,0),CW("r3m3c","R3M3C",3,2,COOP_W_WORLDMAP),
 CW("r3m4a","R3M4A",3,3,COOP_W_STAGE1ST),CW("r3m4b","R3M4B",3,3,COOP_W_WORLDMAP),
 CW("r4m0","R4M0",4,0,COOP_W_INTRO|COOP_W_STAGE1ST|COOP_W_INTRO_OK),
 CW("r4m1a","R4M1A",4,0,0),CW("r4m1b","R4M1B",4,0,0),CW("r4m1c","R4M1C",4,0,COOP_W_WORLDMAP),
 CW("r4m2a","R4M2A",4,1,COOP_W_STAGE1ST),CW("r4m2b","R4M2B",4,1,0),CW("r4m2c","R4M2C",4,1,COOP_W_WORLDMAP),
 CW("r4m3a","R4M3A",4,2,COOP_W_STAGE1ST),CW("r4m3b","R4M3B",4,2,COOP_W_WORLDMAP),
 CW("r4m4a","R4M4A",4,3,COOP_W_STAGE1ST),CW("r4m4b","R4M4B",4,3,COOP_W_WORLDMAP),
 CW("r5m1a","R5M1A",5,0,COOP_W_STAGE1ST),CW("r5m1b","R5M1B",5,1,0),
};
#undef CW
#define COOP_CAMPAIGN_LEN (sizeof(coop_campaign_worlds)/sizeof(coop_campaign_worlds[0]))

/* The fixed kpc-coop/1 MVP route (CoopCampaign=0), unchanged behaviour. */
static const CoopWorld coop_mvp_worlds[]={
 {"realm1/r1m2b","R1M2B",1,1,0},{"realm1/r1m2c","R1M2C",1,1,COOP_W_WORLDMAP},
 {"realm1/r1m3a","R1M3A",1,2,COOP_W_STAGE1ST|COOP_W_INTRO_OK},
};
#define COOP_MVP_LEN 3u

/* COOP_SEL_WORLD: host !changelevel, from one world (sel_realm = its table
 * index) to the end of the campaign along WORLDORDER. */
typedef enum {COOP_SEL_MVP=0,COOP_SEL_FULL=1,COOP_SEL_FROM=2,COOP_SEL_REALM=3,COOP_SEL_WORLD=4} CoopSelMode;

/* Realm names as the shipped data uses them (WORLDORDER.TXT, QUESTNAMES.TXT
 * comments), the KISS character of each realm (confirmed from the in-game
 * character descriptions and narration, CAMPAIGN-MAP-INVENTORY.md section e:
 * water Starbearer, earth Beast King, air Celestial, fire Demon; Nightmare
 * has none), plus digits. Returns realm 1-5 or 0. */
static unsigned int coop_realm_parse(const char *w){
 static const char *const names[COOP_REALMS*2]={"water","earth","air","fire","nightmare",
  "starbearer","beastking","celestial","demon",""};
 unsigned int r,i;
 if(w[0]>='1'&&w[0]<='5'&&!w[1])return (unsigned int)(w[0]-'0');
 if(!w[0])return 0;
 for(r=0;r<COOP_REALMS*2;r++){
  if(!names[r][0])continue;
  for(i=0;names[r][i]&&w[i]==names[r][i];i++);
  if(!names[r][i]&&!w[i])return r%COOP_REALMS+1u;
 }
 return 0;
}
static const char *coop_realm_name(unsigned int realm){
 static const char *const names[COOP_REALMS]={"Water","Earth","Air","Fire","Nightmare"};
 return realm>=1u&&realm<=COOP_REALMS?names[realm-1u]:"?";
}
/* Slice of the campaign table for a selection; returns 0 for an invalid one. */
static int coop_campaign_slice(unsigned int mode,unsigned int realm,unsigned int *first,unsigned int *len){
 unsigned int i,a=COOP_CAMPAIGN_LEN,b=0;
 if(mode==COOP_SEL_FULL){*first=0;*len=COOP_CAMPAIGN_LEN;return 1;}
 if(mode==COOP_SEL_WORLD){if(realm>=COOP_CAMPAIGN_LEN)return 0;*first=realm;*len=COOP_CAMPAIGN_LEN-realm;return 1;}
 if((mode!=COOP_SEL_FROM&&mode!=COOP_SEL_REALM)||realm<1u||realm>COOP_REALMS)return 0;
 for(i=0;i<COOP_CAMPAIGN_LEN;i++)if(coop_campaign_worlds[i].realm==realm){if(a==COOP_CAMPAIGN_LEN)a=i;b=i;}
 if(a==COOP_CAMPAIGN_LEN)return 0;
 *first=a;*len=(mode==COOP_SEL_FROM?COOP_CAMPAIGN_LEN:b+1u)-a;
 return 1;
}
/* Name of each world's exit object (CExitTrigger), from the stock world
 * object tables (one exit per world; names only, no world data is shipped).
 * A script line "trigger_generic <name>" naming it ends the level, so a
 * trigger whose command list reaches such a line starts a level-ending
 * sequence (e.g. R1M1A r1m1a_victory.scr, R1M1B's mirror "Gubba"). */
static const char *const coop_exit_names[][2]={
 {"TUTORIAL","exit"},{"R1M0","CExitTrigger0"},{"R1M1A","CExitTrigger0"},{"R1M1B","Gubba"},
 {"R1M2A","CExitTrigger0"},{"R1M2B","CExitTrigger0"},{"R1M2C","exit"},{"R1M3A","CExitTrigger0"},
 {"R1M3B","CExitTrigger0"},{"R1M3C","exit"},{"R1M4A","exit"},{"R1M4B","exit"},{"R1M4C","exit"},
 {"R2M0","CExitTrigger0"},{"R2M1A","CExitTrigger0"},{"R2M1B","CExitTrigger0"},{"R2M1C","exit"},
 {"R2M2A","CExitTrigger0"},{"R2M2B","CExitTrigger0"},{"R2M2C","levexit01"},{"R2M3A","CExitTrigger0"},
 {"R2M3B","exit"},{"R2M3C","exit"},{"R2M4A","CExitTrigger0"},{"R2M4B","CExitTrigger0"},{"R2M4C","exit"},
 {"R3M0","CExitTrigger0"},{"R3M1A","sloopy"},{"R3M1B","kooklook"},{"R3M2A","CExitTrigger0"},
 {"R3M2B","exittrigger"},{"R3M3A","CExitTrigger0"},{"R3M3B","CExitTrigger0"},{"R3M3C","exit"},
 {"R3M4A","CExitTrigger0"},{"R3M4B","exit"},{"R4M0","CExitTrigger0"},{"R4M1A","CExitTrigger0"},
 {"R4M1B","CExitTrigger0"},{"R4M1C","exit"},{"R4M2A","exitout01"},{"R4M2B","CExitTrigger0"},
 {"R4M2C","exit"},{"R4M3A","CExitTrigger0"},{"R4M3B","exit"},{"R4M4A","finalfire"},{"R4M4B","exit"},
 {"R5M1A","CExitTrigger0"},{"R5M1B","exit"},
};
#define COOP_EXIT_NAMES (sizeof(coop_exit_names)/sizeof(coop_exit_names[0]))
static int coop_ieq(const char *a,const char *b){
 unsigned int i;
 for(i=0;i<128;i++){
  char x=(a[i]>='A'&&a[i]<='Z')?(char)(a[i]+32):a[i],y=(b[i]>='A'&&b[i]<='Z')?(char)(b[i]+32):b[i];
  if(x!=y)return 0;
  if(!x)return 1;
 }
 return 0;
}
/* 1 if name is the exit object of the world with this label (e.g. R1M1A).
 * Leading/trailing blanks of the script argument are ignored. */
static int coop_is_exit_name(const char *label,const char *name){
 char n[64];unsigned int i,k=0;
 if(!label||!name)return 0;
 for(i=0;name[i]==' '||name[i]=='\t';i++){}
 for(;name[i]&&name[i]!=' '&&name[i]!='\t'&&k+1<sizeof n;i++)n[k++]=name[i];
 n[k]=0;
 if(!k)return 0;
 for(i=0;i<COOP_EXIT_NAMES;i++)if(coop_ieq(coop_exit_names[i][0],label))return coop_ieq(coop_exit_names[i][1],n);
 return 0;
}
/* !changelevel argument -> campaign table index, or -1. Accepted, case-
 * insensitively: the label ("R1M2A"), the WORLDORDER path with or without
 * "worlds\" and ".dat" ("realm1/r1m2a", "worlds\realm1\R1M2A.dat"). Blanks
 * around the argument are ignored; anything else is unknown. */
static int coop_campaign_find(const char *arg){
 char w[72];unsigned int i,n=0,start=0,k,tail;
 if(!arg)return -1;
 for(i=0;arg[i]==' '||arg[i]=='\t';i++){}
 for(;arg[i]&&arg[i]!=' '&&arg[i]!='\t';i++){
  char c=arg[i];
  if(n+1>=sizeof w||(unsigned char)c<33||(unsigned char)c>126)return -1;
  if(c>='A'&&c<='Z')c=(char)(c+32);
  w[n++]=c=='\\'?'/':c;
 }
 for(;arg[i]==' '||arg[i]=='\t';i++){}
 if(arg[i]||!n)return -1;
 w[n]=0;
 if(n>=4&&w[n-4]=='.'&&w[n-3]=='d'&&w[n-2]=='a'&&w[n-1]=='t'){n-=4;w[n]=0;}
 if(n>=2&&w[0]=='.'&&w[1]=='/')start=2;
 if(n-start>7&&w[start]=='w'&&w[start+1]=='o'&&w[start+2]=='r'&&w[start+3]=='l'&&w[start+4]=='d'&&w[start+5]=='s'&&w[start+6]=='/')start+=7;
 for(k=0;k<COOP_CAMPAIGN_LEN;k++){
  const char *path=coop_campaign_worlds[k].world,*label=coop_campaign_worlds[k].label;
  for(i=0;path[i]&&path[i]==w[start+i];i++){}
  if(!path[i]&&!w[start+i])return (int)k;
  for(i=0,tail=1;label[i]||w[start+i];i++){
   char c=label[i]>='A'&&label[i]<='Z'?(char)(label[i]+32):label[i];
   if(c!=w[start+i]){tail=0;break;}
  }
  if(tail)return (int)k;
 }
 return -1;
}
#endif
