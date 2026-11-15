# Engine notes for co-op

These are reverse-engineering facts the co-op code depends on, for the 1.13 binaries only:
- `object.lto` `3a03f879…` (base `0x10000000`);
- `cshell.dll` `703813a4…` (Europe/USA, base `0x20000000`);
- `client.exe` (base `0x1ca0000`);
- `server.dll` `83b7bce8…` (Europe family) and `93178237…` (USA);
- `dedicated.exe` `b940fd6d…` (Europe family).

Addresses are RVAs unless written in full. "Live" means confirmed in play; everything else comes from disassembly and emulation. For the campaign worlds themselves, see [CAMPAIGN-MAP-INVENTORY.md](CAMPAIGN-MAP-INVENTORY.md).

## object.lto

| What | Where |
|---|---|
| Server shell | `[0x1016de60]`. Fields: `+0x1d0` game type (0 = single player), maplist `+0x210` (50 × `0x7f` bytes), count `+0x1ae0`, pending NextLevel `+0x1ae9`, intermission `+0x1ae8`. Maplist cursor `0x1016da88`. |
| `g_pServerDE` | `[0x1016de80]`. Function pointers: `+0x1d0` GetObjectPos, `+0x1e0` SetObjectPos, `+0x20` RemoveObject, `+0xf4` GetClientUserData, `+0x90` IntersectSegment. Message vtable at `[de]`: `+0xac` Start, `+0x8` byte, `+0x14` string, `+0xb4` End. |
| Players | List at `[0x10157724]+4` → `{HOBJECT, next}`. `ee7d0(&hobj)` → `CPlayer*`. CPlayer fields: `+8` HOBJECT, `+0x54` inventory, `+0x120` client, `+0x198` state (1 alive, 3 dying, 2 dead; SetState `f0670`), `+0x1e8` name, `+0x260` armour bits, `+0x264` class. Vtable `0x1011efb0` (`+0x50` damage `f3bc0`, `+0x60` is dead). |
| Player box | (−20, −46, −20)..(20, 46, 20); crouched top 14 (`0xee750`, `0xee780`). |
| Spawning | Spawn paths `0xf12a0` / `0xf12be` / `0xf12f1` / `0xf1333` / `0xf08a0`; telefrag `0xf13a4` / `0xf13c4` / `0xf167a`; touch side effects `0xf10c0`. With no multiplayer start, the engine falls back to the single-player start. |
| Death and respawn | The damage handler drops the weapon at `f3f16` → DropWeapon `f3660` (ids 7–14). Multiplayer respawn on fire or jump: `ef960` → `f15c0` (`ef9dc`). |
| Items | Id table `0x141e40` (stride `0x20`). Give `f3440(player, id, n, notify)`, has `f25f0`, count `dd190`, inventory list `dd170`, inventory vtable `0x11ef98` (`+4` trims ammo). Item update `cbf10`, hide/remove `cc570(item, force)`, respawn `cc080`. Lifetime timer `+0x9c` (`cc780`, death drops only). Monster-drop expiry `+0xe4`; the stock bug is about 2 × server time + 5 s (`70ec9`–`70eea`). |
| Scripts | Play `c5340(name)`, playing `c55b0`. Loaded registry `[0x1015a0b8]` / `[0x1015a0b4]`, running list `[0x1015a0a8]`. MID_CINEMATIC `0x3d` → `c5830` at `feb3e` (`0x80` continue, `0x81` skip → `c54f0`). Starting-cinematic gate `0x104830`. |
| Exits | `CExitTrigger` slot `0x2d450` (game-type gate at `0x2d455`). `0x2d530` handles `menu`, `endgame` and message 0x19. ExitLevel `0xfaf60`. No script command changes the level. |
| Messages | `CServerShell::OnMessage` `fe3a0` (sender at `[esp+0x1d4]` inside). Cheats `0x3b` → `fb910`. OnClientEnterWorld `0xfe2a0`. |
| Breakables | `CWindow` vtable `0x1011a1dc`. `CCrate` (a subclass that removes itself) `0x101199f0`. |
| Doors and switches | `CSwitch` `0x44dd6` (not yet host-gated). `CDoorSliding` lookup `0x2c420`, state `+0x28c`, open `0x2cce0`, close `0x2ccf0` (for the R1M2A lift). |
| Narration streams | Handles `0x159c74` (4), names `0x159a78`, count `0x159c84`. |
| Cutscene FOV | Current `0x13dc14`, target `0x13dc18` (radians, default π/2), pending `0x159e95`, time `0x159e98` (`camera_fov` `b28a0` → `c2cb0`). |

## server.dll (Europe family, `83b7bce8…`)

| What | Where |
|---|---|
| Frame | The interface Update `0x2358c` calls the frame function `0x2a05f(flags, time)` at `0x235b0` (thiscall, `ret 8`). Each step is clamped to at least 10 ms (`0x2a19c`–`0x2a1d0`). Server object: time base `+0x1f8`, step `+0x214`, last time `+0x218`. |
| Sound timers | Call site `0x2a1fc` → sound-track updater `0x29f0b` (thiscall, one float dt, `ret 4`). |
| BSP point walker | `0x1068f`: 75 bytes, read-only. Node: `+0x00` flags (bit 2 = class zero, bit 1 = class one), `+0x08` plane (normal + distance), `+0x2c` / `+0x30` children. Root: server manager `+0x19c` → `+0x08` → `+0x44`. `GetPointStatus` wraps it at `0x2440a`. |
| AABB compare | `0x18991` (cdecl, 5 args). Used only by a diagnostic. |
| CreateServer / DeleteServer | `0x23684` / `0x238d6`. USA: `0x2372e` / `0x23980`. |

The USA `server.dll` addresses for the frame and sound hooks are not mapped, so those hooks stay off there.

## IntersectSegment

`uint8 __cdecl IntersectSegment(query*, info*)` at `g_pServerDE+0x90`. The original callers are `0xf2fae` and `0xebc7`; the caller cleans up the stack. Query layout (`coop_intersect_abi.h`):

| Offset | Field |
|---|---|
| `+0x00` | From (3 floats) |
| `+0x0c` | To |
| `+0x18` | Direction |
| `+0x24` | Flags (5 = objects + polys) |
| `+0x28` | Filter callback (the original `0x102730`) |
| `+0x2c` | Filter user data |
| `+0x30` | WORD `0x6003`. Without it, floor rays miss (live, R1M2B). |

Result: hit point at `+0x00`, plane normal `+0x0c`, distance `+0x18`, HOBJECT `+0x1c`. The fields after the plane are tentative.

A segment hit or miss doesn't prove a player fits. The spawn spread and `!goto` therefore add two more checks:
- a floor within 256 units, with normal.y ≥ 0.7, within ±16 of the start's floor height;
- a whole-box BSP walk (`coop_bsp_box.h`), which visits both children wherever the box touches a plane. It returns `INTERIOR_ONLY` or fails closed.

It still doesn't check doors, other world models, volumes or live actors.

## Loading and connection

- Client connect: the client loads the world, then replies `0x17`, and the server calls OnClientEnterWorld. KPCFix clients hold that reply while the loading picture is up or a `0x70` sync is pending.
- **KPCFix messages from server to client:**

  | Id | Meaning |
  |---|---|
  | `0x70` | Hold, with the next world's path |
  | `0x71` | Go |
  | `0x74` | Intro started |
  | `0x75` | Music stop |

  Ids above `0x73` reach the stock default case and are ignored by stock clients. `0x72` and `0x73` are stock ids.

## cshell.dll (Europe/USA)

| What | Where |
|---|---|
| Client shell | `[0x201c8de8]`. State `+0x1398` (1 in world, 5 changing level, 7 intermission). Screen manager `+0x138c`, with type at `+4`: 0 loading art, 2 server scoreboard, 4 key scoreboard. Show `ad1b0(type)`, close `ad3c0`. |
| Commands | Vtable `0xff720`. OnCommandOn `+4` (`0xff724` → `c9d80`). OnCommandOff `+8` (`0xff728` → `cab60`, empty). Scoreboard is action 41 (`ca64c`). |
| Messages | OnMessage `+0x14` (`0xff734` → `cad30`; switch `0x13`–`0x73`; the default case `cbe35` prints "unhandled"). The `0x46` handler `0xcdc80` flips stale buffers during a load (clear `0xcd910`, render `0xcd990`). |
| Loading | PreLoadWorld `0xcd360`. Loading background rule `0xad7b0`. Single-player loading screen type 5 (Show `0xcb8d7`, close at entry `0xc7d8a`, pause request `0xcd7b0`). |
| Music | Manager `[0x201d2788]`. Stop `d9980`, hold `d9d00`, apply held `d9d20`, unload server songs `d96f0` (called at entry `0xc8622`). Every start re-composes from bar 1 (`d9730`). The FADEOUT `0x53` handler `0xd9d60` is only `ret 4`. Death intensity call `0xcacb1` → `0xd99a0`. |
| Chat | Editor at shell `+0x8c` (vtable `0x1004c0`, OnKeyDown `0x1004ec` → `d8040`, text `+0x178` max 127, cursor `+0x1244`). Message list `[0x201d24b8]` (add `d56d0`, called for `0x45` at `0xcb441`). Note: `d56d0` uses a 0x200-byte stack copy and expands `##word` without bounds checks. Fonts at editor `+0x27c` / `+0x7b8` (`interface/arial.pcf` `0x11ca58`). Chat gate `0xca8e8`. |
| Menus and console | Main-menu Quit call `0x21dfb` → `0x2b800`. ClientDE `[0x1c8dfc]` `+0x14` Disconnect. Console object `[0x1c8de0]` (`+0x28` run, `+0x40` register). Collectables HUD list `0xa41c8`. |
| Effects | Respawn Player bolt branch `0x7b5b0`. |

## client.exe

| What | Where |
|---|---|
| Update rate | Byte `0x1d41adc` |
| Sounds | Sound manager `[0xaacd0]+0x284`: instances `+0x330`, count `+0x530`, kill `0x73a20`. Client-made sounds have `inst+0x14 & 0x100` set. |
| Focus | WM_ACTIVATEAPP handler `0x6c4f` (renderer calls at `0x6c97` / `0x6d1d`). `d3d.ren` restores lost surfaces only inside RenderScene (`0x1d542`). |

## Research tools

`coop/tools/` holds the asset-free checkers used to pin these facts. Each needs the original files; their unit tests use synthetic data.

| Script | Checks |
|---|---|
| `check_campaign_table.py` | `coop_campaign.h` against `PSYCHO.REZ` |
| `check_spawn_site.py`, `check_server_bsp_site.py`, `check_server_aabb_site.py` | The original bytes at the sites above |
| `analyze_spawn_points.py` | Census of player starts in the world files: 59 maps, 58 GameType-0 and 132 GameType-1 starts |
| `decode_spawn_log.py` | Decodes spawn trace lines from the log |

The `Trace*` research switches (`TraceCollisionABI`, `TracePlayerClearance`, `TraceBspVolume`, `TraceModelRegistry`, `TraceModelState`) log these probes at R1M2B. The WorldModel registry survey behind the last two found nothing the code relies on.
