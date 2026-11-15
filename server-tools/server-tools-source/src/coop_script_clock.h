#ifndef KPC_COOP_SCRIPT_CLOCK_H
#define KPC_COOP_SCRIPT_CLOCK_H

/* Wall-clock governor for the original object.lto script clock ([Coop]
 * CoopScriptClock, see proxy.c coop_script_update). Pure integer arithmetic.
 *
 * Each server frame the engine offers engine_us of script time. The governor
 * accumulates real elapsed time (debt) and gives the scripts
 * min(engine_us, debt). Consequences:
 *  - script time never runs ahead of the wall clock (by more than one
 *    millisecond tick of the time source);
 *  - a frame never advances scripts by more than the original engine step;
 *  - after a hitch the scripts catch up at most at the original engine rate,
 *    with the unpaid real time capped at COOP_SCRIPT_CLOCK_MAX_DEBT_US. */
#define COOP_SCRIPT_CLOCK_MAX_DEBT_US 250000u

typedef struct {U32 started,last_ms,debt_us;} CoopScriptClock;

static void coop_script_clock_reset(CoopScriptClock *c){c->started=0;c->last_ms=0;c->debt_us=0;}

static U32 coop_script_clock_step(CoopScriptClock *c,U32 now_ms,U32 engine_us){
 U32 elapsed_ms,give;
 if(!c->started){c->started=1;c->last_ms=now_ms;c->debt_us=0;}
 elapsed_ms=now_ms-c->last_ms;   /* wrap-safe for a monotonic 32-bit ms counter */
 c->last_ms=now_ms;
 if(elapsed_ms>COOP_SCRIPT_CLOCK_MAX_DEBT_US/1000u)elapsed_ms=COOP_SCRIPT_CLOCK_MAX_DEBT_US/1000u;
 c->debt_us+=elapsed_ms*1000u;
 give=engine_us<c->debt_us?engine_us:c->debt_us;
 c->debt_us-=give;
 if(c->debt_us>COOP_SCRIPT_CLOCK_MAX_DEBT_US)c->debt_us=COOP_SCRIPT_CLOCK_MAX_DEBT_US;
 return give;
}

#endif
