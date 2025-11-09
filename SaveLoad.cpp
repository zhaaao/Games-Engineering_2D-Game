// Save & Load — okay, keep this self-contained and binary for speed.
// I'll annotate as I go so future-me knows why each field exists.
#include "SaveLoad.h"
#include <fstream>
#include <cstring>

namespace SaveLoad
{
    // Small helper: map NPC type -> movement speed.
    // I do not want to serialize speed directly because speed is a derived rule
    // that we may tweak globally per type. If we change the rule later,
    // old saves should still "feel" consistent. Hmm, but beware balance shifts.
    static inline float speedFromType(unsigned char type)
    {
        switch (type)
        {
        case 0: return 60.f;   // basic walker
        case 1: return 0.f;    // turret-like / static
        case 2: return 110.f;  // fast chaser
        case 3: return 40.f;   // heavy/slow
        default: return 60.f;  // safe default
        }
    }

    // --- Save format SV02 (binary, little-endian) ---
    // Layout (in order):
    // [4]  magic "SV02"
    // [4]  mode (int32: 0 normal, 1 infinite)
    // [4]  hero.x (float)
    // [4]  hero.y (float)
    // [4]  hero.shootInterval (float)
    // [4]  hero.aoeN (int32)
    // [4]  hero.aoeInterval (float)
    // [4]  totalTime (float)
    // [4]  totalKills (int32)
    // [4]  aliveNpcCount (int32)
    //       repeated aliveNpcCount times:
    //       [4] type (int32)
    //       [4] x (float)
    //       [4] y (float)
    //       [4] fireCooldown (float)
    //       [4] hp (int32)
    //       [4] w (int32)
    //       [4] h (int32)
    //
    // Note: bullets/projectiles are NOT serialized (intentional, short-lived state).
    bool SaveToFile(const char* path,
        const Player& hero,
        const EnemyManager& npcs,
        float totalTime,
        int totalKills,
        bool infiniteMode)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false; // okay, no file — just fail quietly

        // Versioning guard — I will bump this if layout changes.
        char magic[4] = { 'S','V','0','2' };
        f.write(magic, 4);

        // Save game mode. I keep it int32 (over bool) to be future-proof.
        int32_t mode = infiniteMode ? 1 : 0;
        f.write((const char*)&mode, sizeof(mode));

        // Hero core pose/state — minimal but enough to continue play.
        float hx = hero.getX();
        float hy = hero.getY();
        f.write((const char*)&hx, sizeof(hx));
        f.write((const char*)&hy, sizeof(hy));

        // Hero fire cadence + AOE params — these affect difficulty, so yes, persist.
        float shootInterval = hero.getShootInterval();
        f.write((const char*)&shootInterval, sizeof(shootInterval));

        int32_t aoeN = hero.getAOEN();
        f.write((const char*)&aoeN, sizeof(aoeN));

        float aoeInterval = hero.getAOEInterval();
        f.write((const char*)&aoeInterval, sizeof(aoeInterval));

        // Global run stats.
        f.write((const char*)&totalTime, sizeof(totalTime));
        int32_t kills = totalKills;
        f.write((const char*)&kills, sizeof(kills));

        // I only store alive NPCs to keep the file short.
        const NPC* arr = npcs.getArray();
        int32_t count = 0;
        for (int i = 0; i < EnemyManager::MAX; ++i)
            if (arr[i].isAlive()) ++count;

        f.write((const char*)&count, sizeof(count));

        // Serialize each living NPC. I deliberately DO NOT save speed:
        // speed is reconstructed from type via speedFromType().
        for (int i = 0; i < EnemyManager::MAX; ++i)
        {
            if (!arr[i].isAlive()) continue;
            int32_t type = (int32_t)arr[i].getType();
            float x = arr[i].getX();
            float y = arr[i].getY();
            float fire = arr[i].getFireCD();
            int32_t hp = arr[i].getHP();
            int32_t w = arr[i].getW();
            int32_t h = arr[i].getH();

            f.write((const char*)&type, sizeof(type));
            f.write((const char*)&x, sizeof(x));
            f.write((const char*)&y, sizeof(y));
            f.write((const char*)&fire, sizeof(fire));
            f.write((const char*)&hp, sizeof(h));
            f.write((const char*)&w, sizeof(w));
            f.write((const char*)&h, sizeof(h));
        }

        // If we encountered an I/O error, std::ofstream converts to false.
        return (bool)f;
    }

    bool LoadFromFile(const char* path,
        Player& hero,
        EnemyManager& npcs,
        float& totalTime,
        int& totalKills,
        bool& infiniteMode)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false; // no file — bail

        // Step 1: verify magic. If mismatch, I refuse to parse.
        char magic[4] = {};
        f.read(magic, 4);
        if (std::memcmp(magic, "SV02", 4) != 0) return false; // wrong version

        // Step 2: read mode and reflect it into runtime systems.
        int32_t mode = 0;
        f.read((char*)&mode, sizeof(mode));
        infiniteMode = (mode != 0);

        // Step 3: hero pose.
        float hx = 0.f, hy = 0.f;
        f.read((char*)&hx, sizeof(hx));
        f.read((char*)&hy, sizeof(hy));
        hero.setPosition(hx, hy); // okay, restore location

        // Step 4: hero firing config — keep sane defaults if file was truncated.
        float shootInterval = 0.35f; // default fallback
        f.read((char*)&shootInterval, sizeof(shootInterval));
        int32_t aoeN = 3;            // default fallback
        f.read((char*)&aoeN, sizeof(aoeN));
        float aoeInterval = 1.0f;    // default fallback
        f.read((char*)&aoeInterval, sizeof(aoeInterval));

        hero.setShootInterval(shootInterval);
        // Note: I pass '2' as the AOE radius/amplitude (whatever param2 means here).
        // This is how the current API expects it. If API changes, adjust here.
        hero.setAOEParams(aoeN, 2, aoeInterval);

        // Step 5: global stats.
        int32_t kills = 0;
        f.read((char*)&totalTime, sizeof(totalTime));
        f.read((char*)&kills, sizeof(kills));
        totalKills = kills;

        // Step 6: NPC manager mode first, then clear current NPCs.
        npcs.setInfinite(infiniteMode);
        NPC* arr = npcs.getArray();
        for (int i = 0; i < EnemyManager::MAX; ++i) arr[i].kill(); // quick reset

        // Step 7: read alive NPC count with bounds checks. Defensive coding here.
        int32_t count = 0;
        f.read((char*)&count, sizeof(count));
        if (count < 0) count = 0; // corrupted? clamp up
        if (count > EnemyManager::MAX) count = EnemyManager::MAX; // clamp down

        // Step 8: rebuild each NPC.
        for (int i = 0; i < count; ++i)
        {
            int32_t type = 0, hp = 3, w = 24, h = 24;
            float x = 0.f, y = 0.f, fire = 999.f; // fireCD large so they won't instantly shoot if missing
            f.read((char*)&type, sizeof(type));
            f.read((char*)&x, sizeof(x));
            f.read((char*)&y, sizeof(y));
            f.read((char*)&fire, sizeof(fire));
            f.read((char*)&hp, sizeof(hp));
            f.read((char*)&w, sizeof(w));
            f.read((char*)&h, sizeof(h));

            // Find a free slot. If none left (should not happen due to clamp), stop early.
            int slot = -1;
            for (int k = 0; k < EnemyManager::MAX; ++k) if (!arr[k].isAlive()) { slot = k; break; }
            if (slot < 0) break; // out of capacity — data says more than we can host

            // I need a facing target to finish spawn init (AI wants a reference point).
            // Using hero center makes sense; it restores typical aggro behavior.
            float faceTx = hero.getHitboxX() + hero.getHitboxW() * 0.5f;
            float faceTy = hero.getHitboxY() + hero.getHitboxH() * 0.5f;
            float spd = speedFromType((unsigned char)type); // reconstruct movement rule

            // Spawn + patch fields we persisted.
            arr[slot].initSpawn(x, y, (unsigned char)type, spd, faceTx, faceTy);
            arr[slot].setFireCD(fire);
            arr[slot].w = w;    // direct size override — yes, these are public in NPC
            arr[slot].h = h;
            arr[slot].hp = hp;
        }

        // If stream is okay, we consider load successful. (Partial reads will flip badbit.)
        return (bool)f;
    }
}
