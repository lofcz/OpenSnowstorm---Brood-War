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
