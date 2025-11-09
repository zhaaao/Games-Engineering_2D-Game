#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"
#include "Player.h"

using namespace GamesEngineeringBase;

/************************  Pickup System  ************************
 * Randomly spawns collectible “buff fruits” on non-blocking tiles.
 * Player gains permanent bonuses when touching a fruit.
 *
 *  - Uses a fixed-size pool instead of STL containers.
 *  - Respawn interval varies between 7–11 seconds.
 *  - Spawns at the center of a random non-blocking tile.
 *  - On pickup: increases fire rate and AOE count,
 *    slightly reduces AOE cooldown.
 *****************************************************************/
struct Pickup {
    bool  alive = false;
    float x = 0.f, y = 0.f;     // World-space top-left position
    int   w = 12, h = 12;       // Size in pixels
    unsigned char r = 60, g = 240, b = 100; // Simple bright-green tint

    // Axis-Aligned Bounding Box (AABB)
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }
};

class PickupSystem {
public:
    static const int MAX = 32;      // Fixed pool size
    bool infiniteWorld = false;     // Infinite map mode flag
private:
    Pickup items[MAX];
    TileMap* map = nullptr;
    float spawnTimer = 0.f;         // Time accumulator for spawn logic
    float nextInterval = 8.0f;      // Time until next spawn
    static float frand01() { return (float)rand() / (float)RAND_MAX; }

    // Basic AABB overlap test
    static inline bool aabbOverlap(float ax, float ay, int aw, int ah,
        float bx, float by, int bw, int bh) {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

    // Finds an unused pickup slot
    int allocIndex() {
        for (int i = 0; i < MAX; ++i) if (!items[i].alive) return i;
        return -1;
    }

    // Spawns one pickup at a random non-blocking tile
    void spawnOne() {
        if (!map) return;
        int idx = allocIndex();
        if (idx < 0) return;

        const int mw = map->getWidth();
        const int mh = map->getHeight();
        const int tw = map->getTileW();
        const int th = map->getTileH();

        // Attempts up to 64 random positions
        for (int tries = 0; tries < 64; ++tries) {
            int tx = (int)(frand01() * mw);
            int ty = (int)(frand01() * mh);
            if (!map->isBlockedAt(tx, ty)) {
                float cx = tx * tw + tw * 0.5f;
                float cy = ty * th + th * 0.5f;
                items[idx].x = cx - items[idx].w * 0.5f;
                items[idx].y = cy - items[idx].h * 0.5f;
                items[idx].alive = true;
                return;
            }
        }
        // No valid tile found; skip this cycle
    }

    // Resets next spawn interval between 7–11 seconds
    void resetInterval() {
        nextInterval = 7.0f + 4.0f * frand01();
    }

public:
    // Initializes the system
    void init(TileMap* m) {
        map = m;
        spawnTimer = 0.f;
        for (int i = 0; i < MAX; ++i) items[i].alive = false;
        srand(24680);
        resetInterval();
    }

    // Periodically spawns a pickup based on accumulated delta time
    void trySpawn(float dt, float camX, float camY) {
        spawnTimer += dt;
        if (spawnTimer >= nextInterval) {
            spawnTimer -= nextInterval;
            resetInterval();
            if (infiniteWorld) spawnOneAroundCamera(camX, camY);
            else               spawnOne();
        }
    }

    // Spawns near the camera position for infinite maps
    void spawnOneAroundCamera(float camX, float camY) {
        if (!map) return;
        int idx = allocIndex();
        if (idx < 0) return;

        const int tw = map->getTileW();
        const int th = map->getTileH();
        const int ring = 20; // Random range around camera in tiles
        int baseTx = (int)std::floor(camX / tw);
        int baseTy = (int)std::floor(camY / th);

        for (int tries = 0; tries < 64; ++tries) {
            int tx = baseTx + (int)((frand01() * 2 - 1) * ring);
            int ty = baseTy + (int)((frand01() * 2 - 1) * ring);
            if (!map->isBlockedAt(tx, ty)) {
                float cx = tx * tw + tw * 0.5f;
                float cy = ty * th + th * 0.5f;
                items[idx].x = cx - items[idx].w * 0.5f;
                items[idx].y = cy - items[idx].h * 0.5f;
                items[idx].alive = true;
                return;
            }
        }
    }

    // Checks for collisions between player and active pickups.
    // When collected, applies buffs and removes the pickup.
    //
    // Buff rules:
    //  - Fire rate: cooldown *= 0.85 (minimum 0.18s)
    //  - AOE count: +1
    //  - AOE cooldown *= 0.9 (minimum 0.5s)
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

                // Apply buffs
                float shoot = hero.getShootInterval();
                shoot *= 0.85f;
                if (shoot < 0.18f) shoot = 0.18f;
                hero.setShootInterval(shoot);

                int   n = hero.getAOEN();
                float acd = hero.getAOEInterval();
                n += 1;
                acd *= 0.90f;
                if (acd < 0.50f) acd = 0.50f;
                hero.setAOEParams(n, 2, acd);

                items[i].alive = false;

                printf("[BUFF] Fruit picked: shootCD=%.2fs, AOE N=%d, AOE CD=%.2fs\n",
                    shoot, n, acd);
            }
        }
    }

    // Renders all visible pickups in the current camera view
    void draw(Window& win, float camX, float camY) {
        const int W = (int)win.getWidth();
        const int H = (int)win.getHeight();

        for (int i = 0; i < MAX; ++i) {
            if (!items[i].alive) continue;

            int sx = (int)(items[i].x - camX);
            int sy = (int)(items[i].y - camY);
            int w = items[i].w, h = items[i].h;

            // Skip if completely outside viewport
            if (sx + w < 0 || sy + h < 0 || sx >= W || sy >= H) continue;

            int x0 = sx < 0 ? 0 : sx;
            int y0 = sy < 0 ? 0 : sy;
            int x1 = sx + w; if (x1 > W) x1 = W;
            int y1 = sy + h; if (y1 > H) y1 = H;

            // Draw colored rectangle
            for (int y = y0; y < y1; ++y) {
                int base = y * W;
                for (int x = x0; x < x1; ++x) {
                    win.draw(base + x, items[i].r, items[i].g, items[i].b);
                }
            }

            // Add a small highlight pixel at the center
            int cx = sx + w / 2, cy = sy + h / 2;
            if (cx >= 0 && cx < W && cy >= 0 && cy < H) {
                win.draw(cy * W + cx, 255, 255, 255);
            }
        }
    }

    void setInfinite(bool v) { infiniteWorld = v; }
};
