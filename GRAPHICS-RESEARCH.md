# KPCFix — graphics research (proposed, not implemented)

This is a contributor research queue, not a list of shipped features.
Existing client behaviour and stable releases are documented in README.md
and CONFIGURATION.md. Changes must be profiled and verified against the
four supported original 1.13 regions before release.

- Engine performance optimisation: Inspect the engine for opportunities to improve FPS, reduce rendering overhead and optimise performance without introducing instability.
- Anisotropic filtering & antialiasing: Investigate enabling or improving anisotropic filtering and antialiasing, including within the game's menus.
- Graphics configuration: Investigate c_GlobalDetail, GibDetail, LodOffset and other graphics settings to understand their behaviour, performance impact and sensible defaults for modern hardware.
- Graphics fidelity improvements: Investigate engine-level methods to improve rendering quality, texture filtering, model detail, draw distances and other visual effects.

See [TESTING.md](TESTING.md) and [RELEASING.md](RELEASING.md) for acceptance gates.
