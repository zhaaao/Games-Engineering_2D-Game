#include "SaveLoad.h"
#include <fstream>
#include <cstring>

namespace SaveLoad
{
    static inline float speedFromType(unsigned char type)
    {
        switch (type)
        {
        case 0: return 60.f;
        case 1: return 0.f;
        case 2: return 110.f;
        case 3: return 40.f;
        default: return 60.f;
        }
    }

    bool SaveToFile(const char* path,
        const Player& hero,
        const EnemyManager& npcs,
        float totalTime,
        int totalKills,
        bool infiniteMode)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        char magic[4] = { 'S','V','0','2' };
        f.write(magic, 4);

        int32_t mode = infiniteMode ? 1 : 0;
        f.write((const char*)&mode, sizeof(mode));

        float hx = hero.getX();
        float hy = hero.getY();
        f.write((const char*)&hx, sizeof(hx));
        f.write((const char*)&hy, sizeof(hy));

        float shootInterval = hero.getShootInterval();
        f.write((const char*)&shootInterval, sizeof(shootInterval));

        int32_t aoeN = hero.getAOEN();
        f.write((const char*)&aoeN, sizeof(aoeN));

        float aoeInterval = hero.getAOEInterval();
        f.write((const char*)&aoeInterval, sizeof(aoeInterval));

        f.write((const char*)&totalTime, sizeof(totalTime));
        int32_t kills = totalKills;
        f.write((const char*)&kills, sizeof(kills));

        const NPC* arr = npcs.getArray();
        int32_t count = 0;
        for (int i = 0; i < EnemyManager::MAX; ++i)
            if (arr[i].isAlive()) ++count;

        f.write((const char*)&count, sizeof(count));

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
            f.write((const char*)&hp, sizeof(hp));
            f.write((const char*)&w, sizeof(w));
            f.write((const char*)&h, sizeof(h));
        }

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
        if (!f) return false;

        char magic[4] = {};
        f.read(magic, 4);
        if (std::memcmp(magic, "SV02", 4) != 0) return false;

        int32_t mode = 0;
        f.read((char*)&mode, sizeof(mode));
        infiniteMode = (mode != 0);

        float hx = 0.f, hy = 0.f;
        f.read((char*)&hx, sizeof(hx));
        f.read((char*)&hy, sizeof(hy));
        hero.setPosition(hx, hy);

        float shootInterval = 0.35f;
        f.read((char*)&shootInterval, sizeof(shootInterval));
        int32_t aoeN = 3;
        f.read((char*)&aoeN, sizeof(aoeN));
        float aoeInterval = 1.0f;
        f.read((char*)&aoeInterval, sizeof(aoeInterval));

        hero.setShootInterval(shootInterval);
        hero.setAOEParams(aoeN, 2, aoeInterval);

        int32_t kills = 0;
        f.read((char*)&totalTime, sizeof(totalTime));
        f.read((char*)&kills, sizeof(kills));
        totalKills = kills;

        npcs.setInfinite(infiniteMode);
        NPC* arr = npcs.getArray();
        for (int i = 0; i < EnemyManager::MAX; ++i) arr[i].kill();

        int32_t count = 0;
        f.read((char*)&count, sizeof(count));
        if (count < 0) count = 0;
        if (count > EnemyManager::MAX) count = EnemyManager::MAX;

        for (int i = 0; i < count; ++i)
        {
            int32_t type = 0, hp = 3, w = 24, h = 24;
            float x = 0.f, y = 0.f, fire = 999.f;
            f.read((char*)&type, sizeof(type));
            f.read((char*)&x, sizeof(x));
            f.read((char*)&y, sizeof(y));
            f.read((char*)&fire, sizeof(fire));
            f.read((char*)&hp, sizeof(hp));
            f.read((char*)&w, sizeof(w));
            f.read((char*)&h, sizeof(h));

            int slot = -1;
            for (int k = 0; k < EnemyManager::MAX; ++k) if (!arr[k].isAlive()) { slot = k; break; }
            if (slot < 0) break;

            float faceTx = hero.getHitboxX() + hero.getHitboxW() * 0.5f;
            float faceTy = hero.getHitboxY() + hero.getHitboxH() * 0.5f;
            float spd = speedFromType((unsigned char)type);

            arr[slot].initSpawn(x, y, (unsigned char)type, spd, faceTx, faceTy);
            arr[slot].setFireCD(fire);
            arr[slot].w = w;
            arr[slot].h = h;
            arr[slot].hp = hp;
        }

        return (bool)f;
    }
}
