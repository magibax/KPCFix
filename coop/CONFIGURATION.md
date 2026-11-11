# Experimental co-op configuration

These settings apply **only** to the `feature/coop-campaign` Server Tools
DLL. The stable R3 Server Tools release on `main` does not implement them.
All values shown are the shipped defaults in
`server-tools/KPCServerTools-default.ini`. Restart the dedicated server
after changing a switch.

| Setting under `[Coop]` | Default | Behaviour and evidence limit |
| --- | ---: | --- |
| `TraceSpawns` | 0 | Records a bounded number of actual engine position-set calls; records requested, not validated transforms |
| `TrialSeparateSpawns` | 0 | Enables fixed, map-specific R1M2B 3-player offsets only; maxclients must be 3; no general safe allocation |
| `TraceCollisionABI` | 0 | Opt-in bounded original-engine segment-query diagnostic on known R1M2B positions |
| `TracePlayerClearance` | 0 | Samples floor/body and logs fail-closed verdicts; `UNSUPPORTED` is not `SAFE` |
| `TraceBspVolume` | 0 | Whole-player-box primary-BSP classifier, hash-gated to one original 1.13 DLL; implies clearance sampling |
| `TraceModelRegistry` | 0 | Read-only named WorldModel resource and reciprocal object reference survey; implies clearance sampling |
| `TraceModelState` | 0 | Read-only raw model type/flags/vector statistics; requires `TraceModelRegistry=1` |
| `TraceCampaign` | 0 | Opt-in research trace of authored intro and campaign exit paths; not campaign synchronization |
| `CoopSession` | 0 | Co-op MVP session (kpc-coop/1): chat lobby, host start, no telefrag between players, R1M2B placements, server-coordinated changes along R1M2B → R1M2C → R1M3A (natural exits from v2; not yet Windows-tested). Written by the launcher co-op checkbox. See [COOP-MVP.md](COOP-MVP.md) |
| `CoopMinPlayers` | 2 | 1–3 players required before the host can `!start` |
| `CoopMaxPlayers` | 3 | 1–3 session capacity; keep server `maxclients` at or below |

These are eleven `[Coop]` keys (eight diagnostics plus three session keys). The ninth INI-related setting
often counted in audits is `[ServerTools] AllowCheats`, a stable
hosting permission independent of co-op. `[ServerTools] Enabled=1`
is additionally required for experimental hooks.

Do **not** infer that all flags/bounds returned by the model diagnostics
identify live solid obstacles. Other WorldModels, dynamic actors,
scripted movement, triggers and map resets remain unverified.

For reproducibility capture:

```powershell
(Get-FileHash .\KPCServerTools.dll -Algorithm SHA256).Hash
(Get-Content .\SERVER_TOOLS_BUILD.json -Raw | ConvertFrom-Json).proxy_sha256
```

The first value must equal the second after installing the intended
experimental package. Capture the effective `KPCServerTools.ini`
settings, build marker and the full `KPCServerTools.log` for each
Windows observation.

Read [co-op evidence status](README.md) and the
[world geometry investigation](BSP-VOLUME-ABI.md).
