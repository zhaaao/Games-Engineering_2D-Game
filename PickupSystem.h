#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"
#include "Player.h"

using namespace GamesEngineeringBase;

/************************  增益果实（随机刷新，拾取即生效） ************************
 * - 不用 STL，固定容量池
 * - 刷新间隔 7~11 秒，随机挑选“非阻挡”瓦片的中心投放
 * - 英雄触碰后：自动射速↑（冷却更短） + AOE 数量 +1（并略缩短 AOE 冷却）
 *******************************************************************************/
struct Pickup {
    bool  alive = false;
    float x = 0.f, y = 0.f;  // 世界坐标（左上）
    int   w = 12, h = 12;    // 大小
    // 简单颜色：亮绿色
    unsigned char r = 60, g = 240, b = 100;

    // AABB
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }
};

class PickupSystem {
public:
    static const int MAX = 32;

private:
    Pickup items[MAX];
    TileMap* map = nullptr;
    float spawnTimer = 0.f;         // 计时器
    float nextInterval = 8.0f;      // 下一次刷新的间隔（秒）
    // 简单随机
    static float frand01() { return (float)rand() / (float)RAND_MAX; }

    // AABB 判交
    static inline bool aabbOverlap(float ax, float ay, int aw, int ah,
        float bx, float by, int bw, int bh) {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

    // 找空位
    int allocIndex() {
        for (int i = 0; i < MAX; ++i) if (!items[i].alive) return i;
        return -1;
    }

    // 随机在“非阻挡瓦片”上投放
    void spawnOne() {
        if (!map) return;
        int idx = allocIndex(); if (idx < 0) return;

        const int mw = map->getWidth();
        const int mh = map->getHeight();
        const int tw = map->getTileW();
        const int th = map->getTileH();

        // 尝试最多 64 次找到一个非阻挡格
        for (int tries = 0; tries < 64; ++tries) {
            int tx = (int)(frand01() * mw);
            int ty = (int)(frand01() * mh);
            if (!map->isBlockedAt(tx, ty)) {
                // 放在该瓦片中心
                float cx = tx * tw + tw * 0.5f;
                float cy = ty * th + th * 0.5f;
                items[idx].x = cx - items[idx].w * 0.5f;
                items[idx].y = cy - items[idx].h * 0.5f;
                items[idx].alive = true;
                return;
            }
        }
        // 没找到就放弃本次（地图全阻挡的极端情况）
    }

    // 刷新“下一次间隔”
    void resetInterval() {
        // 7~11 秒之间
        nextInterval = 7.0f + 4.0f * frand01();
    }

public:
    void init(TileMap* m) {
        map = m; spawnTimer = 0.f;
        for (int i = 0; i < MAX; ++i) items[i].alive = false;
        srand(24680);
        resetInterval();
    }

    // 每帧尝试生成
    void trySpawn(float dt) {
        spawnTimer += dt;
        if (spawnTimer >= nextInterval) {
            spawnTimer -= nextInterval;
            resetInterval();
            spawnOne();
        }
    }

    // 英雄碰到则应用增益并移除
    // 增益规则：
    //  - 自动射速：cooldown *= 0.85（至少 0.18s）
    //  - AOE 数量：+1；AOE 冷却 *= 0.9（最多缩到 0.5s）
    void updateAndCollide(Player& hero) {
        float hx = hero.getHitboxX();
        float hy = hero.getHitboxY();
        int   hw = hero.getHitboxW();
        int   hh = hero.getHitboxH();

        for (int i = 0; i < MAX; ++i) {
            if (!items[i].alive) continue;
            if (aabbOverlap(items[i].getHitboxX(), items[i].getHitboxY(),
                items[i].getHitboxW(), items[i].getHitboxH(),
                hx, hy, hw, hh)) {

                // —— 应用增益 —— 
                // 自动射速
                float shoot = hero.getShootInterval();
                shoot *= 0.85f;
                if (shoot < 0.18f) shoot = 0.18f;
                hero.setShootInterval(shoot);

                // AOE：+1 且略缩短冷却
                int   n = hero.getAOEN();
                float acd = hero.getAOEInterval();
                n += 1;
                acd *= 0.90f;
                if (acd < 0.50f) acd = 0.50f;
                hero.setAOEParams(n, /*dmg*/2, acd);

                items[i].alive = false;

                // 控制台提示（方便作业展示）
                printf("[BUFF] Fruit picked: shootCD=%.2fs, AOE N=%d, AOE CD=%.2fs\n",
                    shoot, n, acd);
            }
        }
    }

    // 绘制（相机左上 -> 屏幕坐标）
    void draw(Window& win, float camX, float camY) {
        const int W = (int)win.getWidth();
        const int H = (int)win.getHeight();

        for (int i = 0; i < MAX; ++i) {
            if (!items[i].alive) continue;

            int sx = (int)(items[i].x - camX);
            int sy = (int)(items[i].y - camY);
            int w = items[i].w, h = items[i].h;

            if (sx + w < 0 || sy + h < 0 || sx >= W || sy >= H) continue;

            int x0 = sx < 0 ? 0 : sx;
            int y0 = sy < 0 ? 0 : sy;
            int x1 = sx + w; if (x1 > W) x1 = W;
            int y1 = sy + h; if (y1 > H) y1 = H;

            for (int y = y0; y < y1; ++y) {
                int base = y * W;
                for (int x = x0; x < x1; ++x) {
                    win.draw(base + x, items[i].r, items[i].g, items[i].b);
                }
            }

            // 简单“高光”点缀
            int cx = sx + w / 2, cy = sy + h / 2;
            if (cx >= 0 && cx < W && cy >= 0 && cy < H) {
                win.draw(cy * W + cx, 255, 255, 255);
            }
        }
    }
};

