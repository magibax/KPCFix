# Co-op campaign: architecture, protocol and UI design

Goal: a co-op campaign as close as possible to the shipped single player. The flow is:
* lobby;
* realm/campaign selection;
* per-map loading screen;
* every client confirmed loaded;
* a common 15-second real-time, music-playing wait;
* the original scripted intro or world unlock;
* natural progression through all realms and Nightmare;
* completion, then back to the lobby.

One dedicated server is authoritative for world state and gameplay effects; every client
gets the presentation. Evidence for the world data is in
[CAMPAIGN-MAP-INVENTORY.md](CAMPAIGN-MAP-INVENTORY.md). Earlier verified behaviour is in
[COOP-MVP.md](COOP-MVP.md).

All RVAs are for the stock 1.13 binaries:
* object.lto `3a03f879…` (base `0x10000000`);
* server.dll `83b7bce8…`;
* cshell.dll `703813a4…` (Europe/USA, base `0x20000000`).

Tags: **[C]** confirmed by disassembly or data, **[I]** inferred.

## Milestone status (2026-11-11)

| Milestone | State | Evidence |
| --- | --- | --- |
| M0 inventory: world order, transitions, R?M0 intros, Nightmare, loading backgrounds | **DONE** | [CAMPAIGN-MAP-INVENTORY.md](CAMPAIGN-MAP-INVENTORY.md); `check_campaign_table.py` PASS against the installed REZ |
| M1 campaign graph, host selection, completion/abort to the lobby (`CoopCampaign=1`) | **IMPLEMENTED** (commit `abeebbb`, DLL `009eacd6…`) | unit tests (64/32-bit), emulator 34/34, native server smoke. **No client has played it** |
| M2 loading screen and client-ready ACK | **RESEARCHED, designed below, not implemented** | static RE only |
| M3 15 s wall-clock hold, gameplay freeze, music pre-roll, realm-intro entry | **RESEARCHED, designed below, not implemented**. The R?M0 intros are already allowed (M1) | static RE only |
| M4 realm-by-realm gameplay validation | NOT STARTED (needs two-PC play) | — |
| M5 two-PC acceptance at 60/165 FPS, packaging | PENDING (user) | — |

## 1. Session state machine

```text
LOBBY ──!start (host, all present ready)──► TRANSITION ─NextLevel─► WORLD_LOADING
  ▲                                                                   │
  │ completion / !abort / off-route change / everybody left           ▼
  │                                                          INTRO_PENDING (barrier)
  │                                                                   │ all ready (+hold, M3)
  │                                               ┌───────────────────┴──────────────┐
  │                                          INTRO_RUNNING                        GAMEPLAY
  │                                       (script once, server)                      │
  │                                               │ finished / own exit (INTRO world)│ exit to successor
  └──────────────────────────── TRANSITION ◄──────┴──────────────────────────────────┘
```

* The epoch increments on every NextLevel.
* Generations: the lobby generation increments on completion or `!abort`, and when everybody
  leaves.
* Host election, the 1–3 player limits, `!ready`, `!unready`, `!join`, `!leave`, `!help`,
  `!status` and `!coop` are unchanged.

## 2. Campaign graph (M1, implemented)

* **Table:** `coop_campaign.h`, 48 worlds in `WORLDORDER.TXT` order, with realm, stage and the
  INTRO/WORLDMAP/first-in-stage flags. TUTORIAL is not part of the campaign. The test
  `check_campaign_table.py` compares the table with the user's REZ.
* **Selection** (host only, lobby only; shown to everyone; persists across generations until
  changed; clears everyone's readiness):

  | Command | Route | Completes when |
  | --- | --- | --- |
  | `!campaign full` (default) | R1M0 … R5M1B (48) | the R5M1B exit (`Endgame`) fires |
  | `!campaign from <realm>` | first world of the realm … R5M1B | as above |
  | `!campaign realm <realm>` | that realm only (Nightmare: R5M1A, R5M1B) | the realm's last world exits to its WORLDORDER successor (e.g. R1M4C → R2M0) |
  | `!campaign` | shows the selection | — |
  | `!abort` | host: back to the lobby world | — |

  `<realm>` is one of:
  * `water|earth|air|fire|nightmare` or `1–5`;
  * the realm character names (confirmed): `starbearer` = Water, `beastking` = Earth,
    `celestial` = Air, `demon` = Fire.

  Invalid names give a usage reply. Non-hosts get "Only the host".
* **Exits:**
  * the exit's `NextWorld` must equal the next route world (case- and slash-insensitive;
    stock exits all do);
  * one change per epoch;
  * everything else is refused once per epoch.
  * `!next` stays a host test override.
  * An INTRO world (R1M0–R4M0) accepts its own scripted exit while the intro runs. Each
    `R?BLACKROOM.SCR` ends with `trigger_generic CExitTrigger0`, at 79 / 93 / 56.5 / 72.5 s.
* **World change:** `NextLevel` through the native maplist cursor.
  * The maplist holds only 50 names (**[C]** object.lto `0xfdd3f`: `0x32` entries of `0x7f`
    bytes at `shell+0x210`).
  * In campaign mode a target missing from the maplist is written into the slot `NextLevel`
    takes next. This is in memory only and logged `maplist slot=… world=…`.
* **Lobby world:** `CoopLobbyWorld`, or else the first world of the selection.
* **Intros:**
  * R1M3A (user-tested) and R1M0–R4M0 are allowed.
  * Others need `CoopIntroAll=1` (test only), because their scripts have not been classified
    for multiplayer.

## 3. Lobby (design, partly open)

* **World:** R1M0 is the safest lobby **[I]**. It has no monsters, spawners, pickups or hazards;
  its exit is inert in multiplayer; it has a single start (multiplayer falls back to it, and
  the session skips telefrag). The intro never plays in the lobby (only started sessions arm
  intros).
  * Open: the launcher's co-op mode still insists on starting R1M2B
    (`Start-KPC-Selected-Server.ps1`). The follow-up is to allow `worlds\realm1\R1M0` when
    `CoopCampaign=1`.
  * Until then, set `CoopLobbyWorld=worlds\realm1\R1M0` for returns, and the first lobby runs
    on R1M2B.
* **Scoreboard in the lobby: feasible with stock clients.**
  * Chat opens and sends while the scoreboard (state 7) is shown **[C]**:
    * command `0x2a` → `0xca8d4` has no state or screen gate;
    * the input line is placed above the scoreboard at `0xca968`;
    * keys reach it via `0xccb83`;
    * `MID_MULTIMSG 0x6f` is sent by `0xce110`.
  * Design:
    1. send `0x48` to each lobby member after its spawn, again after a reconnect, and never
       twice without a `0x69` in between (HUD flag);
    2. close with `0x69` at `!start`.
  * The stock screen shows names (and stock deathmatch fields) but not readiness or host. Those
    stay in chat (`!coop`).
  * Not yet implemented. The pass-2 `CoopStartScoreboard` already covers the `!start` → intro
    window.

## 4. Loading screens (M2 design)

* **Single player [C]:**
  * The loader `0xad7b0` builds `screens/LoadingBackgrounds/<R?M?>.pcx` from the world name.
  * `0xad710`/`0xad750` keep `R<digit>M<digits>`: R1M1A uses R1M1.pcx, R5M1A/B use R5M1.pcx, and
    R5M2.PCX is unused. Anything else falls back to `menu/titlescreen.pcx`.
  * It is drawn by screen type 5, but only through the single-player exit message `0x19`
    (`0xcb79d`, gated off in multiplayer).
* **Multiplayer [C]:**
  * After `0x46` the client is in state 5. It draws no world and no scoreboard, only chat text,
    then blocks in its own load (no frames).
  * Screen type 0 (titlescreen, MultiLoad font) is used only on the first connect.
* **There is no loading bar in the stock game [C].** Both loading screens draw a static
  "LOADING..." string, and the engine has no progress callback into the shell. A "truthful
  loading bar" therefore cannot come from the stock client. The honest status is:
  * per-client stages the server observes (sent `0x46`; engine connect stage reached; entered
    the world; ACK; see §5);
  * shown as chat/`MID_CLIENT_PRINT 0x45` (`0xcb35c`, drawn every frame including state 5)
    before and after each client's blocking load.
* **Client patch (KPCFix client, edition-gated):**
  * hook the engine callback `PreLoadWorld` (shell table slot 7, function table entry
    `0xff73c` = `0xcd360`; thiscall ECX = shell, `char *world`, `ret 4`; called by client.exe
    `0x15d8f` only on the remote-client load path);
  * in multiplayer:
    1. switch the screen manager (`shell+0x138c`) to type 0;
    2. replace its surface (`+0xc`) with `0xad7b0(world)`;
    3. render a few frames as `0xcdc80` does;
    4. then call the original.
  * The stock `0x69` already closes type 0 on entry.
  * Optional: `0x46` → `cdc80(1)` (bytes at `0xcb47a`) shows the loading screen instead of
    black until the name is known.
  * Germany/Italy shells: not available locally. The byte patterns to search for are in
    `C:\KPCTest\scratch-loading\NOTES.md`; the edition gate keeps unknown shells stock.
  * Risks:
    * rendering inside `PreLoadWorld` (inferred safe; `0xcdc80` already renders from
      OnExitWorld);
    * a `0x46` with no load leaves the screen up until disconnect.
* **Scoreboard-in-state-5 alternative** (`0xcdb6f` `32 c0` → `b0 01`): it only freezes the last
  scoreboard frame. Not recommended.
* **Compatibility:**
  * stock client: black screen with chat during the load (unchanged);
  * patched client: the map's PCX;
  * server behaviour is identical for both.

## 5. Client-ready ACK (M2 design)

**Connect stages [C]** (server.dll):
* LOADWORLD (packet 6) sets the client state `+0x2b4` = 0.
* The client runs DoLoadWorld synchronously and then sends packet 9 byte 0 (`client.exe 0x6990c`).
* The server (`0x21542`) streams the object data (`0x1dd92`).
* The client purges stale objects and sends packet 9 byte 1 (`0x6d465`).
* The server then calls **OnClientEnterWorld** (object.lto vtable `0x1011f1c0` slot 4 =
  `0xfe2a0`).

**Shell handshake [C]:**
* OnClientEnterWorld → `0xfdfb0` sends `MID 0x17` (PLAYER_MULTIPLAYER_INIT).
* The client replies with `0x17` and `0x15`.
* The server's `0x17` case (`0xfebae`) sends `0x1a`, **spawns** (`0xf12a0` → `f13a4`), sends the
  MOTD and `0x69` (`0xfecce`).

**What the existing spawn-based readiness already proves:** the spawn is the server's reaction
to a **client message**, and it can only happen after the client:
* finished loading the world;
* acknowledged the object stream;
* ran its shell's multiplayer init.

It does **not** prove that the client processed `0x69` (rendering enabled). The pass-1 "3 s
settle" covered exactly that gap and is to be replaced.

**Deterministic ACK (design): echo `0x17`.**
1. Hook OnMessage dispatch `0xfe40b` (`8b c7 25 ff 00 00 00`). At entry: EDI = message id,
   hClient = `[esp+0x1d8]`, hMsg = `[esp+0x1e0]`. Return `id&0xff` to pass, or `0x25` to
   swallow.
2. Hook `0xfecce` (`e8 9d a3 ff ff` → `0xf9070`). Forward it, then mark client C
   `ECHO_PENDING(epoch E)` and re-send an identical `0x17` via
   `0xfdfb0(GetClientUserData(C), C)`.
3. The next `0x17` from C is the echo. Guaranteed messages are delivered in order **[I]**, so C
   processed `0x69` first. Mark C **READY(E)** and swallow that `0x17` and the following
   `0x15`, which would otherwise respawn the player.
4. Reset on OnClientExitWorld, on a new OnClientEnterWorld and on epoch change. Stale replies
   are dropped: user data is NULL after exit (`0xfebc3`), and the new connect stages come
   first.
5. Risk: the client's `0x17` handler (`0xbea70`) must be idempotent. This needs a live test.

Fallback (heuristic, documented as such): the first `MID_PLAYER_UPDATE 0x11` (sent at about
15 Hz by `0xc8650`) arriving at least RTT + 200 ms after `0x69`.

## 6. 15-second hold, freeze and music (M3 design, not implemented)

> **Status (2026-11-12): never built; superseded.** No 15-second countdown exists in any build. The shipped flow is the client spawn hold behind the loading picture (client `CampaignLoadingHoldSpawn`) and an intro that starts with the last spawn (`CoopIntroSettleMs`, campaign default 0). The freeze evidence below is still the planned way to stop the world running behind the loading picture (playtest item 9 in [the handoff](NEXT-SESSION-HANDOFF.md)): pause the server while members are loading, without the countdown.

**Freeze: the server pause flag [C].**
* `g_pServerDE` `+0x2c` GetServerFlags and `+0x30` SetServerFlags (cdecl). Bit 0 sets mgr
  `+0x194`.
* Paused, the server stops:
  * game time;
  * the sound countdown;
  * the whole object.lto Update `0xffb70`: script clock, health bars, NextLevel and the
    proxy's `0xffbbf` tick;
  * the object loop `0x29beb`: AI, physics, triggers and spawners.
* Paused, it keeps running: connect stages, OnClientEnterWorld, OnMessage (chat, ACK) and
  outgoing messages.
* Resume is exact: the last-frame time is refreshed while paused, so there is no dt spike.
* **Do not** use the stock pause `0x1004a0`: it broadcasts `0x41`/`0x42` (pause screen, closes
  screens).

**Hold sequence (per world, after the session's barrier):**
1. Every present member READY(E) (§5) → pause the world (SetServerFlags | 1) and lock input
   (`user_input off`, `0xc5a10(0)`).
2. A common **15 000 ms wall-clock** countdown (`GetTickCount`), with chat/`0x45` status:
   `Loaded 2/3`, then `Starting in 15`, 10, 5, 3, 2, 1.
3. While paused the proxy needs its own tick: a call-site hook at server.dll `0x2a292`
   (`e8 52 fb ff ff`, `call 0x29de9`), which runs in both paths. It carries music delivery,
   the countdown, the scoreboard pump and ACK bookkeeping.
4. In OnMessage, swallow attack `0x31`, use `0x1f` (also mirrors), `0x5e`, `0x6e` and `0x36`
   until release. Let `0x32` through.
5. Release (one server event):
   1. close any scoreboard (`0x69`);
   2. unpause;
   3. either start the intro script (`0xc5340`, which locks input itself) or send
      `user_input on`.
6. The script clock and sound clock must exclude paused time from their wall-time debt.

**Music pre-roll:** `CoopMusic` already delivers the world's original music to each player
1.5 s after its spawn, so the music runs during the hold. Delivery must move to the paused
tick.

**Group changes:**
* A member leaving during the hold is dropped, and the hold continues if anyone remains.
* A new member arriving during the hold restarts the countdown once it is READY.
* The barrier stays bounded (60 s default) with a visible reason. When it expires, the session
  starts with the READY members only, never with zero.
* Repeated `!start` or fire/use cannot shorten the hold, because those messages are swallowed
  or ignored during it.

## 7. Gameplay fidelity work items (M4)

* **AI and invulnerability:**
  * `disable_ai` setter `0x6bd20(byte)` (global `0x10156ed4`) is callable in multiplayer; only
    the script event `0xc28a0` is gated. Use it for letterbox phases, after checking that every
    monster class uses the bases `0x6ba55`/`0x6ffe0`.
  * `player invulnerable` (`0xc1a80` → `0xf4da0` → `0xf6890`) is gated in multiplayer and
    affects only the first player. Not usable yet.
* **Monster spawners:** pass-2 `CoopSpawners` restores the authored spawners. `AttackActivator`
  targets the head of player list 0 (the most recently joined player). Deterministic targeting
  for 1–3 players is still to be designed.
* **World map:** in multiplayer the client gets no exit message, so there is no world map
  between stages. This is an accepted divergence, to be documented in play.
* **Nightmare:**
  * R5M1A/R5M1B intros carry `cheatscript cheat5` (class Celestial, armour, weapons). This
    runs only in single player with `CheatScripts` set (`0xb8ad0`).
  * The intended character setup for Nightmare in co-op is open.
  * The finale exit `Endgame` → `MID_ENDWORLD 0x72` → `movies\EndCine.smk` on clients. Co-op
    completes the route there and returns to the lobby.
* **Persistence (inventory, character, one-time pickups, checkpoints):** not designed yet.
  Multiplayer `keepitems 1` keeps inventory across worlds natively. Save/resume is out of
  scope.

## 8. Diagnostics

Existing bounded lines:
* `world-loaded`;
* `maplist slot=…`;
* `intro-*`, `scoreboard …`;
* `music-send`, `health-bar`, `world-content`;
* `exit-seen` and `exit-refused`;
* `presence`.

Selection changes appear as `say` lines.

Planned (M2/M3): `ready client=… epoch=… stage=enter|init|echo`, `hold start/remaining/release`
(once per second at most), and `pause on/off`.
