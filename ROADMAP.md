# Roadmap — OpenSnowstorm

OpenSnowstorm is building toward being the **OpenMW of StarCraft: Brood War**: a fully open-source, community-maintained engine that runs Brood War content with complete fidelity — and eventually surpasses the original in moddability, platform support, and long-term preservation.

Like OpenMW, we get there in layers: first a deterministic, testable engine core; then a complete client experience; then enhancements the original never had.

For deeper technical breakdowns, see:

- `docs/modernization-roadmap.md` (detailed phased plan)
- `docs/architecture.md` (module map / ownership model)
- `docs/phase-1-kickoff.md` (active decomposition notes)
- `docs/phase-2-kickoff.md` (active sync/replay reliability slice)
- `docs/phase-3-kickoff.md` (platform/CI hardening kickoff)
- `docs/phase-4-kickoff.md` (complete client experience foundations kickoff)
- `docs/broodwar-compatibility.md` (compatibility matrix + validation backlog)

---

## North star (the OpenMW parallel)

OpenMW proved that a community of volunteers can:

1. Understand and replicate a commercial game engine at bit-perfect fidelity.
2. Build cleaner, more maintainable code than the original.
3. Add capabilities the original never had (64-bit, Lua scripting, GPU rendering, etc.).
4. Ensure the game can be preserved and played decades into the future.

**That is exactly what OpenSnowstorm is doing for StarCraft: Brood War.**

Concretely, "success" means:

- A player can install Brood War data files, point OpenSnowstorm at them, and play a complete game — single-player campaigns, multiplayer, custom maps — without touching Blizzard's binaries.
- Bot authors can embed the engine headlessly via the BWAPI-compatible API and test against a deterministic, debuggable simulation.
- Modders can add new units, mechanics, and campaigns using documented, stable engine APIs.
- The engine runs on Linux, Windows, macOS, and the Web (Emscripten) without proprietary dependencies.

---

## Milestones

### Phase 0 — Foundations (Now)

*Goal: make the engine safe to change and easy to contribute to.*

- **Determinism regression harness**
  - Replay-driven "golden" checks (frame hashes + key invariants)
  - CI: run on every PR touching simulation code
- **Build normalization**
  - Single top-level CMake entry (or presets) building engine + UI + shim consistently
  - Documented toolchain and dependency requirements
- **Contributor ergonomics**
  - Formatting/lint guidance (non-disruptive baseline)
  - Quick-start contributor docs, issue labels, good-first-issue list
- **Performance baseline**
  - `--bench` mode for headless simulation throughput measurement (done)
  - `perf_metrics.h` frame-timer infrastructure (done)
  - Pre-allocated unit-finder spatial index (done)

### Phase 1 — Architecture Decomposition (Landed)

*Goal: reduce the monolith blast radius and clarify subsystem boundaries.*

- **Split `bwgame.h`** into focused modules behind a compatibility facade (COMPLETE)
  - Extracted `bwgame_tables.h`, `bwgame_state.h`, `sync_protocol.h`.
  - Extracted `pathfinding.h`, `triggers.h`, `creep.h`.
  - Eliminated `simulation_constants.h` duplication.
- **Explicit ownership and lifetimes**
  - Clarified coupling between sync, actions, and state layers (COMPLETE).

### Phase 2 — Sync + Replay Reliability (COMPLETE)

*Goal: make multiplayer and replay infrastructure production-grade.*

- **Protocol boundaries and versioning**
  - Explicit message schema documentation + compatibility strategy
- **Desync diagnostics**
  - Structured desync report artifact: first divergent frame, recent actions/hashes, client ID
- **Replay format durability**
  - Validation tooling, forward-compat metadata, fuzz targets for binary readers

### Phase 3 — Platform & UI Hardening (LANDED)

*Goal: decouple SDL2 from the architecture; support more backends.*

- **UI backend abstraction** (COMPLETE)
  - Defined a narrow platform/window interface allowing for non-SDL2 backends.
  - Separated input translation and surface management from the physical window.
- **Headless entrypoint hardening** (COMPLETE)
  - Clean headless mode via `headless_backend.cpp` (no SDL2 linkage required).
  - Enables pure simulation for CI/CD and bot diagnostics.
- **Runtime UX improvements** (COMPLETE)
  - Decoupled hotkey structure ready for external config mapping (`options.ini` foundation).
  - Premium F3 debug overlay with Sim/Draw timing, frame counting, and sync status.

### Phase 4 — Complete Client Experience (LANDED & Polished)

*Goal: a player can run Brood War end-to-end with OpenSnowstorm.*

- **Single-player client foundation** (COMPLETE)
  - Cinematic launcher, mission selections, and briefing room integration.
  - Full game HUD with HP/Energy bars and multi-selection logic.
- **Audio Pipeline Foundation** (COMPLETE)
  - Native sound interface abstracted with simulation-driven SFX triggers.
- **Campaign engine completeness** (95%)
  - All standard triggers and mission logic validated.

### Phase 5 — State Serialization & Persistence (ACTIVE)

*Goal: enable persistent mid-mission saves and replay resume paths.*

- **Disk Serialization**: systematic save/restore for units, sprites, and campaign state to `.osv` files.
- **UI Integration**: Map Quick Save (F5) and Quick Load (F8) to actual filesystem operations.
- **Metadata Support**: Track mission timers, objectives, and campaign progress across sessions.

### Phase 6 — AI Script Engine (ACTIVE)

*Goal: functional computer players for a true campaign experience.*

- **AIScript Interpreter**: load and execute `aiscript.bin` (build orders, tactical AI).
- **Trigger Integration**: support "Run AI Script" triggers for complex mission behaviors.
- **AI Targeting**: implement computer-player priority logic for attacks and base defense.

### Phase 7 — Audio & Final Polish

*Goal: Bit-perfect audio fidelity and mission flow.*

- **SFX & Music Pipeline**: full SDL2 audio backend implementation (Voices, SFX, BG Music).
- **Campaign Flow**: automatic map chaining and persistent campaign "Episode" progress.
- **Render Enhancements**: hardware-accelerated sprite compositing.

*Goal: do what the original engine never could.*

---

## Contribution-sized "good first" work

- **Docs**: tighten build/run instructions, diagram updates, API surface explanations
- **Refactor slices**: extract a focused helper/module behind a compatibility facade
- **Testing infrastructure**: replay fixtures + hash checkpoints + harness wiring
- **Diagnostics**: log categories (`sync`, `replay`, `ui`, `data`, `perf`) and consistent error paths
- **Performance**: profile hot paths with `perf_metrics.h` instrumentation; submit findings as issues

---

## What we won't do (on purpose)

- Bundle Blizzard assets or ship proprietary game content
- Do a "big rewrite" that breaks determinism and compatibility in exchange for aesthetics
- Require proprietary tools or platforms to build and run

---

## Success metrics

- **Determinism**: 100% pass rate on golden-hash replay fixtures across Linux/Windows/WASM
- **Performance**: sim-only throughput ≥ 10× real-time on modern hardware (measure with `--bench`)
- **Compatibility**: all stock single-player campaign maps complete without divergence
- **Build time**: incremental rebuild < 30 seconds for a single-subsystem change
- **Onboarding**: new contributor can build and run a replay check in < 30 minutes
