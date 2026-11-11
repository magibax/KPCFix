typedef unsigned int U32;
#include "coop_script_clock.h"
#include <assert.h>
#include <stdio.h>

/* Simulate frames every frame_ms of wall time with the engine offering
 * engine_us per frame; return the total script time given (us). */
static U32 run(CoopScriptClock *c,U32 *now,U32 frames,U32 frame_ms,U32 engine_us){
 U32 i,total=0;
 for(i=0;i<frames;i++){*now+=frame_ms;total+=coop_script_clock_step(c,*now,engine_us);}
 return total;
}

int main(void){
 CoopScriptClock c;U32 now=5000,total,g;
 coop_script_clock_reset(&c);
 /* First call only establishes the reference point. */
 assert(coop_script_clock_step(&c,now,10000)==0);

 /* Measured host behaviour: 8 ms frames credited 10 ms by the engine (1.25x).
  * The governor delivers wall time: 1000 frames = 8 s, not 10 s. */
 total=run(&c,&now,1000,8,10000);
 assert(total==8000000u);

 /* Engine slower than (or equal to) the wall clock: never accelerated
  * beyond the engine's own step; output equals the original. */
 total=run(&c,&now,100,16,10000);
 assert(total==1000000u);

 /* That left 6 ms of debt per frame, capped at 250 ms. Engine back at
  * 10 ms per 8 ms frame: catch up at the original rate only (+2 ms per
  * frame) and never run ahead of the wall clock. */
 assert(c.debt_us==250000u);
 g=coop_script_clock_step(&c,now+8,10000);now+=8;
 assert(g==10000u&&c.debt_us==248000u);
 total=run(&c,&now,124,8,10000);
 assert(total==1240000u&&c.debt_us==0u);
 total=run(&c,&now,10,8,10000);
 assert(total==80000u);

 /* Coarse time source (15/16 ms ticks) with 8 ms frames: steps stay <= the
  * engine step (no jumps), and the long-run total equals the wall clock. */
 coop_script_clock_reset(&c);now=1000;coop_script_clock_step(&c,now,10000);
 {U32 i,t=0,wall=0,step,max_step=0;
  for(i=0;i<4000;i++){
   U32 tick=(i%2)?16u:0u;   /* GetTickCount-like: advances every other frame */
   now+=tick;wall+=tick;step=coop_script_clock_step(&c,now,10000);t+=step;
   if(step>max_step)max_step=step;
  }
  assert(max_step<=10000u);
  assert(wall*1000u-t<=16000u);   /* within one tick of the wall clock */
 }

 /* A long stall (e.g. a 5 s hitch): at most 250 ms of debt is kept, and
  * each following frame still advances by at most the engine step. */
 coop_script_clock_reset(&c);now=0;coop_script_clock_step(&c,now,10000);
 g=coop_script_clock_step(&c,now+=5000,200000);
 assert(g==200000u&&c.debt_us==50000u);

 /* 32-bit millisecond counter wrap. */
 coop_script_clock_reset(&c);now=0xfffffff0u;coop_script_clock_step(&c,now,10000);
 g=coop_script_clock_step(&c,now+=0x20u,100000);   /* wraps to 0x10 */
 assert(g==32000u);

 /* Zero engine step gives zero and keeps the debt. */
 coop_script_clock_reset(&c);now=0;coop_script_clock_step(&c,now,10000);
 assert(coop_script_clock_step(&c,now+=8,0)==0&&c.debt_us==8000u);

 printf("PASS coop script clock governor\n");
 return 0;
}
