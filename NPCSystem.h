#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"   // 需要地图尺寸与瓦片尺寸
#include "NPC.h"
using namespace GamesEngineeringBase;

/***************************  NPCSystem：池与生成器  **************************
 * - 固定容量数组（MAX=128），不使用 STL
 * - 生成：相机外环带；频率随时间线性加快（封顶）
 * - 更新：追踪玩家；NPC 可穿越水（保持你现有差异）
 ******************************************************************************/
class NPCSystem
{
public:
    static const int MAX = 128;

private:
    NPC npcs[MAX];
    TileMap* map = nullptr;
    float elapsed = 0.f;      // 累计时间
    float spawnTimer = 0.f;   // 生成计时
    // 频率参数（与你现有一致）
    static constexpr float SPAWN_BASE_INTERVAL = 1.6f;
    static constexpr float SPAWN_MIN_INTERVAL = 0.35f;
    static constexpr float SPAWN_ACCEL_PER_SEC = 0.02f;

    // 简单双精随机与夹紧
    static float frand01() { return (float)rand() / (float)RAND_MAX; }
    static float fclamp(float v, float a, float b) { return (v < a) ? a : ((v > b) ? b : v); }

    // 找空位；没空返回 -1
    int allocIndex()
    {
        for (int i = 0; i < MAX; ++i) if (!npcs[i].isAlive()) return i;
        return -1;
    }

    // 世界像素尺寸
    void mapPixelSize(int& outW, int& outH)
    {
        if (!map) { outW = outH = 0; return; }
        outW = map->getWidth() * map->getTileW();   // 你已有这些接口: getWidth/Height, getTileW/H
        outH = map->getHeight() * map->getTileH();   // :contentReference[oaicite:0]{index=0}
    }

    // 实际生成 1 个
    void spawnOne(float camX, float camY, int viewW, int viewH, float px, float py)
    {
        int idx = allocIndex(); if (idx < 0) return;

        int worldW, worldH; mapPixelSize(worldW, worldH);
        if (worldW <= 0 || worldH <= 0) return;

        const int margin = 64;
        float sx = 0.f, sy = 0.f;
        int edge = (int)(frand01() * 4.f);
        if (edge == 0) { sx = camX - margin - 24; sy = camY + frand01() * viewH; }
        else if (edge == 1) { sx = camX + viewW + margin; sy = camY + frand01() * viewH; }
        else if (edge == 2) { sx = camX + frand01() * viewW; sy = camY - margin - 24; }
        else { sx = camX + frand01() * viewW; sy = camY + viewH + margin; }

        sx = fclamp(sx, 0.f, (float)(worldW - 24));
        sy = fclamp(sy, 0.f, (float)(worldH - 24));

        unsigned char type = (frand01() < 0.75f) ? 0 : 1;
        float spd = (type == 0) ? 60.f : 0.f;
        npcs[idx].initSpawn(sx, sy, type, spd, px, py);
    }

public:
    void init(TileMap* m)
    {
        map = m; elapsed = 0.f; spawnTimer = 0.f;
        for (int i = 0; i < MAX; ++i) npcs[i].kill();
        srand(12345); // 如已有全局 srand，可移除
    }

    // 每帧：尝试生成（频率随时间加快）
    void trySpawn(float dt, float camX, float camY, int viewW, int viewH, float px, float py)
    {
        elapsed += dt;
        float interval = SPAWN_BASE_INTERVAL - SPAWN_ACCEL_PER_SEC * elapsed;
        if (interval < SPAWN_MIN_INTERVAL) interval = SPAWN_MIN_INTERVAL;

        spawnTimer += dt;
        if (spawnTimer >= interval) {
            spawnTimer -= interval;
            int count = (elapsed > 60.f) ? 2 : 1; // 过一分钟后稍微加量
            for (int i = 0; i < count; ++i) spawnOne(camX, camY, viewW, viewH, px, py);
        }
    }

    // 更新全体（传玩家坐标）
    void updateAll(float dt, float px, float py)
    {
        int worldW, worldH; mapPixelSize(worldW, worldH);
        for (int i = 0; i < MAX; ++i) if (npcs[i].isAlive())
            npcs[i].update(dt, px, py, worldW, worldH);
    }

    // 绘制全体（传相机左上）
    void drawAll(Window& win, float camX, float camY)
    {
        for (int i = 0; i < MAX; ++i) if (npcs[i].isAlive())
            npcs[i].draw(win, camX, camY);
    }

    // 若你后续做碰撞/查询，可提供一个只读指针
    const NPC* getArray() const { return npcs; }
    NPC* getArray() { return npcs; }
};
