Original prompt: continue until the game boots and loads just like the original brood war - ful parity and fiellity

- 2026-03-29: Initial continuation pass. No prior `progress.md` existed.
- Immediate blocker found: Windows build currently fails in `mini-openbwapi/openbwapi.cpp` with MSVC error `C1128` ("number of sections exceeded object file format limit"), so no fresh `gfxtest.exe` is being produced.
- Added `/bigobj` wiring at the top-level CMake targets first; next step is to patch the BWAPI subproject too if the build still dies there.
- Goal for this pass: get a clean bootable native executable, then iterate on the startup/load path toward Brood War-style behavior.
- Build fix completed: `cmake --build build --config Release -j 6` now succeeds and produces `build/Release/gfxtest.exe`.
- Boot-path improvement completed: `gfxtest` now supports `--data-dir <path>`, auto-discovers Brood War MPQ locations from common directories / `OPENSNOWSTORM_DATA_DIR`, and reports a clear actionable startup error instead of exiting silently.
- Current machine state: no `StarDat.mpq`, `BrooDat.mpq`, or `Patch_rt.mpq` were found on the scanned local drives (`C:\`, `D:\`, `P:\`), so full native boot into real Brood War content is currently blocked by missing game assets rather than code.
- Current native startup result on this machine: `gfxtest --headless` prints the searched paths and instructs the user to provide `--data-dir` or set `OPENSNOWSTORM_DATA_DIR`.
- Next useful step once assets are available: point `gfxtest` at a valid Brood War install plus a map/replay, then continue on frontend/client-fidelity work instead of loader plumbing.
