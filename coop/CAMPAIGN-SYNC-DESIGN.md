# Campaign startup and scripted-sequence synchronisation

Status: **design proposal and research plan, not implemented or live-verified**. A minimal subset (lobby barrier, epochs, debounced server-coordinated exit, intro skipped) is implemented as the [co-op MVP](COOP-MVP.md); the rest of this design remains open. Research branch: `feature/coop-campaign`. Recorded 2026-11-10.

## Problem

A dedicated co-op server may load a campaign world before any player joins. We do not yet know whether original 1.13 campaign startup scripts execute at world load, require a local player, or are suppressed by multiplayer game type. Once campaign execution is restored, an empty server must not silently consume player-facing cinematics or advance player-driven objectives before the first client is ready.

Joining mid-campaign presents a separate problem: a new client must inherit the current **authoritative world state**, not rerun scripts that already opened doors, spawned enemies, granted items or advanced objectives. The server must coordinate in-engine cutscene presentation without replaying gameplay-affecting effects per client.

This is a design investigation, separate from the three-position R1M2B spawn trial and the still-unverified general collision-safe spawn allocator.

## Confirmed project evidence (not assumptions about synchronisation)

- In the dedicated Windows trial, campaign world R1M2B loaded and three players could spawn at trial positions, but the normal introductory cinematic did not start and a known hallway transition did not advance the map.
- The developer browser's client-hosted map load is a working intro comparison; **it does not establish** that dedicated-server multiplayer invokes the same script path.
- R1M2B's `WorldProperties.starting_cinematic` references `CS2START.SCR`; existing world analysis indicates this is also associated with loading level scripts. `CExitTrigger0.NextWorld` and `WORLDORDER.TXT` identify R1M2C as the next world.
- The original 1.13 object module checks its game-type field at RVA `0x2d455` in the exit path. A nonzero value prevents the subsequent call at RVA `0x2d510`. The significance of the upstream trigger/activation path remains to be verified.
- The current opt-in `[Coop] TraceCampaign=1` instrumentation in `server-tools/server-tools-source/src/proxy.c` observes the original startup lookup callsite at RVA `0x1045b1`, logging the path and whether the lookup returns an object, and observes entry into the mode-gated exit handler. It leaves native decisions intact. **A successful lookup does not prove dispatch, execution or completion of the cinematic.** Absence of an exit-handler trace does not prove that the exit handler is the failing component; the event may never have reached it.
- The existing client-side `source/src/cinematic.c` handles fullscreen movie aspect ratio/blitting. It is not an implemented network synchronisation layer for the in-engine `starting_cinematic` script.

Related work: [co-op roadmap](README.md), [world/object survey](SPAWN-FINDINGS.md), [spawn ABI research](SPAWN-ENGINE-TRACE.md), and [allocator integration](GENERAL-ALLOCATOR.md).

### Unknowns requiring evidence

1. Does the dedicated server execute any portion of `CS2START.SCR` at map load with zero clients? At first join? Never in multiplayer mode?
2. Which instructions mutate authoritative world state and which send a per-player camera/audio/UI action?
3. Does original LithTech 1.13 provide suitable per-client cinematic RPCs, an ordered reliable channel, completion notifications and/or a script state query?
4. Are scripted timers driven by server world ticks, wall-clock time or per-player updates? Can a sequence legitimately run without a player?
5. What engine callback signals safe client readiness, world reset, player departure and exit-trigger activation, including right-click golden-mirror exits?
6. Can a cutscene be resumed at an arbitrary timeline offset, or only started at a safe cue boundary?

Do not globally impersonate single-player game type to force scripts on: this may affect multiplayer spawn selection, physics, deathmatch and other object paths. Trace, classify and gate the minimum required campaign paths.

## Proposed ownership model

| Responsibility | Owner | Rule |
| --- | --- | --- |
| World loading and required object/script initialisation | Dedicated server | May run without players; must not trigger player-facing progression by accident |
| Campaign phase, objectives, trigger consumption, gameplay script execution | Dedicated server | Execute gameplay effects once per map/sequence generation |
| Spawn/respawn positions, health, pickups, enemy and door state | Dedicated server | Preserve existing engine replication; add snapshots only for proven gaps |
| Decision to begin/end an in-engine cutscene | Dedicated server | One sequence instance and authoritative start time |
| Local camera, subtitles, voice, HUD lock and other purely presentational actions | Each client | Execute locally as instructed; avoid duplicating gameplay side effects |
| New-player catch-up | Dedicated server + joining client | Snapshot current state, then ordered events; never replay side-effectful scripts |

**Important:** Existing `starting_cinematic` scripts might mix server-side effects with client presentation. Merely broadcasting “run CS2START.SCR” to every client is unsafe until instructions are classified and replay semantics are verified.

## Proposed campaign state machine

| State | Entered when | Allowed behaviour |
| --- | --- | --- |
| `WORLD_LOADING` | Dedicated server loads or changes world | Initialise engine/world objects and essential script managers |
| `WAITING_FOR_PLAYERS` | World ready but opening sequence not committed | Admit players, assign safe spawns, wait for world-loaded/ready acknowledgements |
| `INTRO_RUNNING` | Server authorises opening sequence | Advance gameplay events once; deliver presentation to the participating ready clients |
| `GAMEPLAY_ACTIVE` | Opening sequence completed or explicitly skipped | Accept server-side objectives, interactions, combat and trigger activations |
| `TRANSITION_PENDING` | Mirror interaction or transition trigger accepted | Debounce exits, coordinate end state, announce next world and load barrier |
| `WORLD_LOADING` (new epoch) | Next world begins | Invalidate previous-world sequence IDs and start fresh readiness barrier |

A `map_epoch` must increment on every world load/restart, even if the map name is unchanged; a `sequence_id` and ordered event number must uniquely identify each event within the epoch. On map change, reject stale acknowledgements and messages.

### Empty-server policy

**Recommended default:** the server loads and performs **required non-player world initialisation** but does not commit a player-facing opening cinematic or player-driven campaign progression until at least one fully loaded participant is ready. “World initialised” and “campaign started” are distinct states.

Do **not** implement this by globally stopping the original server tick or blindly skipping all script startup. Either could break objects, collision setup, AI, or networking. First establish the script lifecycle; gate only verified player-facing events and objective progress.

If all clients disconnect, preserve the authoritative campaign phase and world state. A configurable idle policy could defer new player-facing actions, but cannot safely pause arbitrary running engine timers/AI until their dependencies are understood. The initial implementation should avoid promising full pause/resume.

### First join and synchronised intro

1. World loads, the server records `WAITING_FOR_PLAYERS`, and required non-player initialisation remains possible.
2. Clients load the same world and explicitly signal readiness (not merely socket connected or player allocated).
3. The server fixes the participant set for the opening sequence, optionally with a bounded lobby/ready barrier. A client that has not finished loading must not block everyone forever.
4. The server issues a single authoritative `SEQUENCE_START` with sequence/epoch identity and a server-relative start tick; individual clients receive their local camera/audio/UI actions.
5. The server advances gameplay-affecting script actions once, publishes ordered milestones, and marks the sequence complete on a verified server-side completion condition. Presentation acknowledgements are not allowed to independently retrigger game effects.
6. Release input/camera locks and move to `GAMEPLAY_ACTIVE` under server control.

Do not infer synchronous presentation from equal FPS or arrival time; clients can have distinct loading/render delays. Prefer milestone/cue synchronisation to frame-level lockstep.

### Late join and reconnect

- **Before intro begins:** admit the newcomer to the ready group if the barrier is still open.
- **During intro:** safest first implementation is to put the newcomer in a protected pending state (no active input, no unsafe spawn exposure) until the next verified synchronisation checkpoint, then apply a state snapshot. Do not start the complete intro for only that client if it would replay gameplay effects.
- **After intro:** send the current world/objective/player state and any durable sequence outcomes; do not rerun opening scripts. The client should enter gameplay at a safe verified spawn/respawn position.
- **Reconnect:** treat connection IDs as ephemeral; reattach or allocate player identity only after authoritative lifecycle handling is known. Current trial reservations persist for the object module lifetime and **do not yet support safe slot reclamation**.
- **No clients remain:** avoid resetting completed sequences merely because player count reaches zero. Any explicit reset should create a new map epoch.

If an original client cannot resume a presentation at an arbitrary timestamp, provide a safe cue-boundary join or skip rather than pretending to offer frame-accurate mid-cinematic catch-up.

## Candidate protocol (illustrative, NOT an existing engine API)

```text
WORLD_READY      { map_epoch, map_id, state_revision }
CLIENT_READY     { map_epoch, connection_id, loaded_world }
SEQUENCE_START   { map_epoch, sequence_id, script_id, start_tick, participants }
SEQUENCE_CUE     { map_epoch, sequence_id, event_number, cue_id, server_tick }
SEQUENCE_END     { map_epoch, sequence_id, final_revision }
CAMPAIGN_SNAPSHOT{ map_epoch, state_revision, phase, objectives, sequence_results }
TRANSITION_START { map_epoch, transition_id, next_world, start_anchor }
CLIENT_ACK       { map_epoch, message_id, observed_revision }
```

Requirements before implementation:

- Inspect existing 1.13 server-to-client message facilities; reuse a verified ordered/reliable path instead of assuming custom packets are supported. Track version/feature capability and preserve ordinary clients/DM when co-op is disabled.
- Server event handlers must be idempotent: deduplicate on epoch/sequence/event number. Acknowledgements cannot execute the same action twice.
- A snapshot and event stream must have a clear revision boundary: buffer events while taking a snapshot, then replay only those newer than its revision to avoid gaps or duplication.
- Server sequence decisions must not depend solely on an arbitrary client disconnecting, dropping packets, or finishing its local animation. Define timeout, failure and skip policies before implementation.
- Never trust client-supplied requests for arbitrary `script_id`, `next_world`, sequence completion, or campaign state changes.

## Script classification and level exits

Trace representative script actions and sort them into:

1. **World-initialisation:** required non-player object/script setup.
2. **Gameplay-authoritative:** doors, enemy spawns, objectives, pickups, boss phases, trigger state and world transition.
3. **Presentation-only:** local cameras, dialogue/subtitles, fade, HUD/input control.
4. **Mixed/unknown:** must remain unmodified until native execution context and side effects are understood.

Golden mirrors (right-click interactions) and corridor volume triggers are distinct input paths, but both should eventually request the **same server-authoritative transition coordinator**. The server validates the interaction once, rejects duplicate triggers while `TRANSITION_PENDING`, preserves the chosen `StartPointName`/entry anchor, and loads the next map exactly once. Confirm native script and world-load semantics before wiring either exit path.

## Read-only tracing plan (next engineering milestone)

For **R1M2B**, instrument only known validated official 1.13 binaries and guard all prospective hooks by original hashes, layout and expected instruction bytes. Preserve the existing runtime-only / fail-closed policy and do not modify original `object.lto` on disk.

1. **Zero-client control:** load R1M2B dedicated; record world load completion, `starting_cinematic` lookup, *actual script dispatch*, first script callback/action, and any world-state change over an observed interval without connecting a client.
2. **First client:** join only after the zero-client control; distinguish socket connection, player creation, world-loaded readiness, script start, client notification and completion.
3. **Second client:** join before opening, during opening, and after opening in separate clean sessions; record whether engine script actions execute again and whether both clients receive the same presentation.
4. **Developer-browser control:** load the same original world in the known working client-hosted mode; compare dispatch/action paths and server/client presentation messages to the dedicated test.
5. **Exit controls:** separately activate a golden mirror and known hallway trigger (where present), tracing upstream contact/use handlers, trigger object activation, game-type gate at `0x2d455`, and subsequent world-change call.
6. **No-player/reconnect/map reset:** disconnect all, reconnect, and restart/change world; observe script timers, sequence lifetimes, object state and spawn reservations.
7. **Negative controls:** run a DM map (PM01), `[Coop] TraceCampaign=0`, `[ServerTools] Enabled=0` and both supported regional binary profiles.

Log at least: map ID/epoch, server tick, connected/ready counts, source object, script name/instance, native action ID/callsite, dispatch outcome, player recipient (if any), trigger activation, game type and transition result. Where only a lookup is currently observable, label it **LOOKUP**, not **STARTED** or **PLAYED**.

Before adding a new hook, recover and check the original callsite/ABI, thread context, receiver, argument ownership, return semantics and relevant control-flow paths. No guessed engine method calls.

## Initial acceptance tests (future implementation)

| Scenario | Expected outcome |
| --- | --- |
| Start server empty for a period, then first player joins | Opening presentation remains available to that player; essential world setup is intact |
| Two ready clients at opening | One authoritative sequence and gameplay event stream; both see local presentation |
| Different FPS / slow client loading | No duplicate effects or permanent readiness deadlock; documented lag policy |
| Join mid-opening | Player enters at a defined checkpoint or after intro with current state; no replay of effects |
| Join after door/objective completed | Snapshot shows completed outcome; no regenerated enemy/door action |
| Reconnect during campaign | State survives; safe spawn/slot reuse after lifecycle gates pass |
| Activate hallway transition; activate mirror transition | Exactly one transition through each verified path; everyone loads same next map and anchor |
| All clients disconnect and rejoin | No accidental campaign reset; safe idle policy |
| Restart same map | New epoch discards old events and a fresh intro policy applies |
| Ordinary DM / co-op disabled | Native behaviour unaffected; no unwanted script/cinematic interception |

This work is **not** a prerequisite for leaving the existing read-only spawn diagnostics enabled, but is required before claiming playable campaign progression. Implement it in separate verified gates: **(A) native lifecycle trace; (B) minimal first-player activation; (C) authoritative exits; (D) client-side presentation synchronisation; (E) late-join snapshots.** Do not bundle an experimental campaign runtime into a stable Server Tools or KPCFix release without Windows tests.
