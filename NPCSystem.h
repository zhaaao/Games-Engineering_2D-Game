#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"   // 需要地图尺寸与瓦片尺寸
#include "NPC.h"
using namespace GamesEngineeringBase;

/*********************  敌方子弹（固定容量池，不用 STL）  *********************/
struct EnemyBullet
{
    bool  alive = false;
    float x = 0.f, y = 0.f;     // 世界坐标（左上角）
    float vx = 0.f, vy = 0.f;   // 速度（px/s）
    float life = 0.f;           // 剩余寿命（秒），<=0 则回收
    int   w = 6, h = 6;         // 简单矩形尺寸（像素）

    void spawn(float sx, float sy, float dirx, float diry, float speed, float ttl)
    {
        alive = true;
        x = sx; y = sy;
        // 归一化方向
        float len = std::sqrt(dirx * dirx + diry * diry);
        if (len < 1e-6f) { dirx = 1.f; diry = 0.f; len = 1.f; } // 兜底
        dirx /= len; diry /= len;
        vx = dirx * speed;
        vy = diry * speed;
        life = ttl;
    }

    // AABB 碰撞盒（与 Player / NPC 的 getHitbox* 命名保持一致）
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }
};


/***************************  NPCSystem：池与生成器  **************************
 * - 固定容量数组（MAX=128），不使用 STL
 * - 生成：相机外环带；频率随时间线性加快（封顶）
 * - 更新：追踪玩家；NPC 可穿越水（保持你现有差异）
 ******************************************************************************/
class NPCSystem
{
public:
    static const int MAX = 128;
    static const int BULLET_MAX = 256;
private:
    NPC npcs[MAX];
    TileMap* map = nullptr;
    float elapsed = 0.f;      // 累计时间
    float spawnTimer = 0.f;   // 生成计时
    // ----------------- 子弹池 -----------------
    EnemyBullet bullets[BULLET_MAX];
    // 世界尺寸缓存（像素）
    int worldWpx = 0, worldHpx = 0;
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

        // ★★★ 新增：初始化炮台的首发冷却；非炮台给个超大CD避免误触 ★★★
        if (type == 1) {
            // 0.2~0.4 秒的随机首发，打散同频
            npcs[idx].setFireCD(0.2f + 0.2f * frand01());
        }
        else {
            npcs[idx].setFireCD(999.f);
        }
    }

    // 取一个空闲子弹槽
    int allocBullet()
    {
        for (int i = 0; i < BULLET_MAX; ++i)
            if (!bullets[i].alive) return i;
        return -1;
    }

    // 工具：从 NPC 中心到目标（玩家中心）的方向
    static inline void dirFromAToB(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh,
        float& outx, float& outy)
    {
        float acx = ax + aw * 0.5f, acy = ay + ah * 0.5f;
        float bcx = bx + bw * 0.5f, bcy = by + bh * 0.5f;
        outx = (bcx - acx); outy = (bcy - acy);
    }

    // AABB 判交（不分离，仅检测）
    static inline bool aabbOverlap(float ax, float ay, int aw, int ah,
        float bx, float by, int bw, int bh)
    {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

public:
    void init(TileMap* m)
    {
        map = m; elapsed = 0.f; spawnTimer = 0.f;
        for (int i = 0; i < MAX; ++i) npcs[i].kill();
        srand(12345); // 如已有全局 srand，可移除
        // —— 子弹池清空 & 世界像素尺寸缓存 —— 
        for (int i = 0; i < BULLET_MAX; ++i) bullets[i].alive = false;
        mapPixelSize(worldWpx, worldHpx);

    }

    // ---- AABB 矩形相交 ----
    static inline bool aabbIntersect(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh)
    {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
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

    // 更新全体（传玩家坐标）——含：追踪AI、炮台冷却与发射子弹
    void updateAll(float dt, float px, float py)
    {
        // 世界像素尺寸（供移动/越界等逻辑使用）
        int worldW, worldH;
        mapPixelSize(worldW, worldH);

        for (int i = 0; i < MAX; ++i)
        {
            if (!npcs[i].isAlive()) continue;

            // 先让各自的“移动/转向”等内部逻辑跑一遍（你原本就有）
            npcs[i].update(dt, px, py, worldW, worldH);

            // === 仅炮台（type==1）会发射子弹 ===
            if (npcs[i].type == 1)
            {
                // 冷却倒计时
                npcs[i].fireCD -= dt;

                if (npcs[i].fireCD <= 0.f)
                {
                    // —— 获取炮台中心坐标 —— 
                    float cx = npcs[i].getX() + npcs[i].getHitboxW() * 0.5f;
                    float cy = npcs[i].getY() + npcs[i].getHitboxH() * 0.5f;

                    // —— 计算朝玩家的方向向量 —— 
                    float dirx = px - cx;
                    float diry = py - cy;

                    // 取一个空闲子弹槽
                    int bi = allocBullet();
                    if (bi >= 0)
                    {
                        // 子弹从炮台中心出发；子弹自身是 6x6，所以 -3 让它居中
                        float sx = cx - 3.f;
                        float sy = cy - 3.f;

                        const float BULLET_SPEED = 280.f;   // 速度 px/s
                        const float BULLET_TTL = 3.0f;    // 最长寿命 s
                        bullets[bi].spawn(sx, sy, dirx, diry, BULLET_SPEED, BULLET_TTL);
                    }

                    // 重置冷却：1.0~1.4 秒之间轻微抖动，避免所有炮台同频
                    // 如果你项目已有 frand01()（返回 [0,1)），这里会直接用到；
                    // 没有的话见文末“兜底实现”小段。
                    npcs[i].fireCD = 1.0f + 0.4f * frand01();
                }
            }
        }
    }


    // 绘制全体（传相机左上）
    void drawAll(Window& win, float camX, float camY)
    {
        for (int i = 0; i < MAX; ++i) if (npcs[i].isAlive())
            npcs[i].draw(win, camX, camY);
    }

    // --- 玩家与所有存活 NPC 的碰撞检查（含最小分离）---
    void checkPlayerCollision(Player& hero)
    {
        // 取玩家碰撞盒
        float hx = hero.getHitboxX();
        float hy = hero.getHitboxY();
        float hw = (float)hero.getHitboxW();
        float hh = (float)hero.getHitboxH();

        // 计算碰撞盒相对于渲染矩形的偏移，用于修正坐标
        float offX = (hero.getW() - hw) * 0.5f;
        float offY = (hero.getH() - hh) * 0.5f;

        for (int i = 0; i < MAX; ++i)
        {
            if (!npcs[i].isAlive()) continue;

            // NPC 的碰撞盒（当前矩形即碰撞盒）
            float nx = npcs[i].getHitboxX();
            float ny = npcs[i].getHitboxY();
            float nw = (float)npcs[i].getHitboxW();
            float nh = (float)npcs[i].getHitboxH();

            if (!aabbIntersect(hx, hy, hw, hh, nx, ny, nw, nh))
                continue;

            // —— 最小分离：按中心点求 overlapX / overlapY，沿较小轴弹开玩家 —— 
            float hcX = hx + hw * 0.5f, hcY = hy + hh * 0.5f;
            float ncX = nx + nw * 0.5f, ncY = ny + nh * 0.5f;
            float dx = hcX - ncX, dy = hcY - ncY;
            float ox = (hw * 0.5f + nw * 0.5f) - (dx >= 0 ? dx : -dx);
            float oy = (hh * 0.5f + nh * 0.5f) - (dy >= 0 ? dy : -dy);

            // 沿最小重叠轴分离
            if (ox < oy) hx += (dx >= 0 ? ox : -ox);
            else         hy += (dy >= 0 ? oy : -oy);

            // 把碰撞盒左上换回玩家渲染坐标
            hero.setPosition(hx - offX, hy - offY);
            // === 加这一段（一次调用就够，冷却会防止多次触发） ===
            float heroCx = (hx + hw * 0.5f);   // 玩家碰撞盒中心
            float heroCy = (hy + hh * 0.5f);
            float npcCx = (nx + nw * 0.5f);   // NPC 碰撞盒中心（用你的 NPC 位置/尺寸变量）
            float npcCy = (ny + nh * 0.5f);

            // 从 NPC 指向玩家的方向（把玩家往外推）
            hero.applyKnockback(heroCx - npcCx, heroCy - npcCy,
                220.f /*power px/s*/, 0.12f /*duration s*/);

            // 可选：此处可加入受击判定 / 硬直 / 音效 / 减血逻辑
            // hero.takeHit();
        }
    }

    // 若你后续做碰撞/查询，可提供一个只读指针
    const NPC* getArray() const { return npcs; }
    NPC* getArray() { return npcs; }

    // —— 敌方子弹更新（越界或寿命到期即回收）——
    void updateBullets(float dt)
    {
        for (int i = 0; i < BULLET_MAX; ++i)
        {
            if (!bullets[i].alive) continue;
            bullets[i].x += bullets[i].vx * dt;
            bullets[i].y += bullets[i].vy * dt;
            bullets[i].life -= dt;

            // 世界边界剔除 / 寿命到
            if (bullets[i].life <= 0.f ||
                bullets[i].x + bullets[i].w < 0 || bullets[i].y + bullets[i].h < 0 ||
                bullets[i].x > worldWpx || bullets[i].y > worldHpx)
            {
                bullets[i].alive = false;
            }
        }
    }

    // —— 敌方子弹命中英雄：击退 + 销毁 —— 
    void checkBulletHitHero(Player& hero)
    {
        // 读取英雄的碰撞盒（你已实现居中/帧同步）
        float hx = hero.getHitboxX();
        float hy = hero.getHitboxY();
        int   hw = hero.getHitboxW();
        int   hh = hero.getHitboxH();

        // 英雄中心（用于击退方向）
        float hcx = hx + hw * 0.5f;
        float hcy = hy + hh * 0.5f;

        for (int i = 0; i < BULLET_MAX; ++i)
        {
            if (!bullets[i].alive) continue;

            if (aabbOverlap(bullets[i].getHitboxX(), bullets[i].getHitboxY(),
                bullets[i].getHitboxW(), bullets[i].getHitboxH(),
                hx, hy, hw, hh))
            {
                // 击退方向：从子弹指向英雄（把英雄往外推）
                float dirx = (hcx - (bullets[i].x + bullets[i].w * 0.5f));
                float diry = (hcy - (bullets[i].y + bullets[i].h * 0.5f));

                // 触发你已有的击退（与 NPC 碰撞一致的一套手感参数）
                hero.applyKnockback(dirx, diry, 220.f /*power*/, 0.12f /*duration*/);

                // 可在此加扣血/受击无敌等
                // hero.takeHit();

                // 子弹失活
                bullets[i].alive = false;
            }
        }
    }

    // —— 敌方子弹绘制 —— 
    void drawBullets(Window& win, float camX, float camY)
    {
        const int W = (int)win.getWidth();
        const int H = (int)win.getHeight();

        for (int i = 0; i < BULLET_MAX; ++i)
        {
            if (!bullets[i].alive) continue;

            // 屏幕坐标
            int sx = (int)(bullets[i].x - camX);
            int sy = (int)(bullets[i].y - camY);
            int bw = bullets[i].h;   // 小方块子弹，6x6
            int bh = bullets[i].h;

            // —— 矩形剪裁（完全在屏幕外则跳过）——
            if (sx >= W || sy >= H || sx + bw <= 0 || sy + bh <= 0) continue;

            // —— 裁出可画区域 —— 
            int x0 = sx < 0 ? 0 : sx;
            int y0 = sy < 0 ? 0 : sy;
            int x1 = sx + bw; if (x1 > W) x1 = W;
            int y1 = sy + bh; if (y1 > H) y1 = H;

            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                    win.draw(x, y, 255, 40, 40);  // ← 保证不会越界
        }
    }


};
