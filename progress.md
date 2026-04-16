Original prompt: continue until the game boots and loads just like the original brood war - ful parity and fiellity

- 2026-03-29: Initial continuation pass. No prior `progress.md` existed.
- Immediate blocker found: Windows build currently fails in `mini-openbwapi/openbwapi.cpp` with MSVC error `C1128` ("number of sections exceeded object file format limit"), so no fresh `gfxtest.exe` is being produced.
- Added `/bigobj` wiring at the top-level CMake targets first; next step is to patch the BWAPI subproject too if the build still dies there.
- Goal for this pass: get a clean bootable native executable, then iterate on the startup/load path toward Brood War-style behavior.
- Build fix completed: `cmake --build build --config Release -j 6` now succeeds and produces `build/Release/gfxtest.exe`.
- Boot-path improvement completed: `gfxtest` now supports `--data-dir <path>`, auto-discovers Brood War MPQ locations from common directories / `OPENSNOWSTORM_DATA_DIR`, and reports a clear actionable startup error instead of exiting silently.
- Interactive boot-flow improvement completed: launching `gfxtest` with no `--map` / `--replay` now opens a native startup frontend shell instead of hard-failing to `maps/p49.rep`.
- The new startup shell discovers nearby `.scx`, `.scm`, and `.rep` files from the install / working directories, shows selectable launch entries, and hands off into the existing replay or live-map runtime.
- Window title and CLI help were updated to reflect the frontend-driven default boot path.
- Current machine state: no `StarDat.mpq`, `BrooDat.mpq`, or `Patch_rt.mpq` were found on the scanned local drives (`C:\`, `D:\`, `P:\`), so full native boot into real Brood War content is currently blocked by missing game assets rather than code.
- Current native startup result on this machine: `gfxtest --headless` prints the searched paths and instructs the user to provide `--data-dir` or set `OPENSNOWSTORM_DATA_DIR`.
- Verification completed this pass: `cmake --build build --config Release -j 6` succeeds, and `build/Release/gfxtest.exe --help` prints the new interactive frontend usage.
- Next useful step once assets are available: point `gfxtest` at a valid Brood War install so the frontend shell can be exercised visually, then continue on briefing/debrief, sound/music, and higher-fidelity client polish.
- 2026-04-16 (continued): Single-player client shell upgrade in `ui/gfxtest.cpp` + `ui/ui.h`.
  - **Mission objectives panel**: `on_trigger_set_objectives` now stores the latest
    objectives text in `ui.current_objectives_text` (previously only logged).  A
    persistent top-left RGBA panel renders the objectives for the duration of the
    mission, soft-wrapped at ~40 characters, so the player can reference them
    without scrubbing HUD banner history.
  - **Post-mission debrief overlay**: victory and defeat now pause the game and
    render a large centred debrief panel with mission name, elapsed time, units
    built, buildings built, units lost, kills (tallied from enemy
    `unit_deaths`), minerals/gas gathered, and unit/building scores.  The overlay
    is tinted green on victory and red on defeat.  Enter advances (next
    campaign mission if resolved, otherwise the startup shell); F7 restarts;
    F8 quickloads; F10 / Esc returns to the startup shell.
  - **Quit-to-shell instead of process exit**: pressing Esc during any paused
    state (briefing, user pause, or result screen) now returns to the startup
    frontend shell via `enable_frontend(discover_startup_entries(...))`, instead
    of terminating the process.  F10 does the same unconditionally.
  - **Mission restart hotkey**: F7 re-loads the current mission file via
    `load_single_player_map(current_map_file)` so the player does not have to
    rely on quickload when no save exists.
  - **Pause overlay**: when the game is paused (briefing, user pause), a
    centred RGBA panel lists available actions (resume, save, load, restart,
    quit to menu).  The briefing variant shows the mission name instead and
    directs the player to press Space to begin.
  - **Persistent speed indicator**: a small top-right RGBA badge shows the
    current speed preset (0.5X / 1X / 2X / 4X / 8X+) or "PAUSED" so players
    always know the effective game speed without the debug overlay.
  - **ui_functions overlay hook**: `std::function<void(uint32_t*,int,int,int)>
    rgba_overlay_cb` renders on the RGBA surface after the final indexed blit
    and before the window update, giving `main_t` a single place to draw
    client chrome with its existing ASCII-font helpers in `gfxtest.cpp`.
  - **Chaining now gated on the debrief**: campaign transitions (trigger-driven
    or filesystem fallback) still resolve the next map on victory, but the
    client now shows the debrief first and waits for Enter/Esc/F7/F10
    instead of yanking the player into the next mission mid-celebration.
  - **Build signal**: `cmake --build build -j 4` succeeds for all three
    targets (`openbw_ui`, `gfxtest`, `mini-openbwapi`); no new warnings.
    `gfxtest --help` prints the updated keybindings and debrief/objectives
    summary.
- 2026-04-16: Campaign-playability slice landed in `ui/gfxtest.cpp`.
  - **Campaign progress persistence**: the client now writes a `campaign_progress.txt` file
    beside the executable every time a single-player map is launched.  The startup frontend
    injects a pinned "Continue" entry at the top of the shell when that file points at a
    reachable map, so relaunching the client resumes at the last-played mission instead of
    forcing the player to re-select it from the discovery list.
  - **Filesystem-chain fallback**: on victory, if no `Set Next Scenario` trigger fires, the
    client now walks the same folder as the current map and loads the next alphabetically-
    sorted `.scx`/`.scm` file.  This lets curated campaign map packs chain through the
    mission list even when individual maps do not embed the trigger-driven transition.
    Triggered transitions still take precedence when available.
  - **Pre-mission briefing gate**: each single-player map auto-pauses on load and pushes a
    "Mission: <name>" / "Press Space to begin." HUD pair.  The first unpause (Space/P)
    dismisses the briefing; subsequent pauses behave normally.  Because HUD message
    expiry is keyed to `st.current_frame` and the frame counter is frozen while paused,
    the briefing stays visible for the entire pre-mission pause.
  - **Defeat help text**: on local defeat, the client now pushes a "Press F8 to retry or
    Esc to quit." HUD message so players reach quickload without needing the CLI help.
  - **Build signal**: native build completes (`cmake --build build -j 4`) with no new
    warnings; `--help` prints the updated usage.
