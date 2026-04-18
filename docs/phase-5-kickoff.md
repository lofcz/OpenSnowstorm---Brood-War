# Phase 5 Kickoff — State Serialization & Persistence

## Goal
Enable persistent mid-mission saves and replay resume paths. Success means a player can save their game (F5), close the client, and resume exactly where they left off (including objectives and mission state) by loading (F8).

## Objectives
1.  **Metadata Serialization**: Expand the `.osv` (OpenSnowstorm Save) format to include non-simulation state:
    *   Mission current objectives text.
    *   Mission name / Map path.
    *   Pending next scenario (trigger-driven).
    *   Active portrait state (if any).
2.  **Filesystem Resilience**:
    *   Ensure saves are written to a dedicated `saves/` directory.
    *   Implement automatic directory creation.
    *   (Optional) Support multiple save slots (QuickSave, AutoSave, and named slots).
3.  **UI Feedback**:
    *   Improve HUD messaging for save/load status.
    *   Add "Load Game" selection to the startup frontend (Phase 5 fallback).

## Technical Approach
- Bump `OSV_VERSION` in `serialization.h`.
- Define a `save_metadata_t` struct to hold UI-level state.
- Update `state_serializer::save_full` and `load_full` to commit this metadata to disk.
- Update `main_t` in `gfxtest.cpp` to populate and apply this metadata.

## Success Criteria
- F5 writes a valid `.osv` file to disk.
- Closing and restarting the client, then pressing F8, restores the simulation AND the objectives panel.
- Replays can eventually use this mechanism to "Resume from Replay".
