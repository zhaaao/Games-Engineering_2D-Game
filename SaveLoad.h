#pragma once
#include "Player.h"
#include "NPCSystem.h"

// This header defines a very lightweight binary save/load interface.
// The idea is to keep it minimal — just the essentials to resume a session.
// We don’t serialize textures, maps, or transient effects; only the core state.
//
// Save includes:
//   - game mode (normal / infinite)
//   - player position + weapon timing params
//   - total elapsed time and total kills
//   - snapshot of each alive NPC (type/x/y/fireCD/...)
// 
// Load does the reverse — it wipes current NPCs, rebuilds them, 
// and restores player state so the session can continue seamlessly.
//
// Note: NPC dimensions and HP are restored directly from file.
// If we ever add a setter API later, we should switch to that 
// instead of poking into internals (see implementation notes).

namespace SaveLoad
{
    // --- SaveToFile ---
    // Writes the current run state into a binary file.
    // Returns true on success, false on any I/O failure.
    //
    // Parameters:
    //   path         - destination file path (e.g., "save.bin")
    //   hero         - the player character whose core state is saved
    //   npcs         - enemy manager containing all active NPCs
    //   totalTime    - elapsed time (in seconds)
    //   totalKills   - total enemy kills so far
    //   infiniteMode - whether the run is in infinite mode
    //
    // Future me: if we ever add inventory or buffs, this is the place to expand.
    bool SaveToFile(const char* path,
        const Player& hero,
        const EnemyManager& npcs,
        float totalTime,
        int totalKills,
        bool infiniteMode);

    // --- LoadFromFile ---
    // Reads a previously saved state and reconstructs gameplay objects.
    // This will clear all NPCs in the manager and repopulate them from the file.
    // It also updates the Player and EnemyManager states accordingly.
    //
    // After loading:
    //   - EnemyManager::setInfinite() will be called internally.
    //   - Caller should update TileMap wrapping mode (see main.cpp usage example).
    //
    // Returns true if the file could be opened and parsed correctly.
    bool LoadFromFile(const char* path,
        Player& hero,
        EnemyManager& npcs,
        float& totalTime,
        int& totalKills,
        bool& infiniteMode);
}
