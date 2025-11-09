#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"  
#include "NPC.h"
using namespace GamesEngineeringBase;

class Player;

/****************************  EnemyProjectile (fixed pool)  ****************************
 * Minimal bullet primitive:
 *   - world position at top-left (x,y)
 *   - velocity in px/s (vx, vy)
 *   - lifetime countdown; collected when life <= 0
 *   - simple AABB (w,h) for collision
 * Keep the struct plain so the pool can be a raw array.
 ***************************************************************************************/
struct EnemyProjectile
{
    bool  alive = false;
    float x = 0.f, y = 0.f;     // world-space top-left
    float vx = 0.f, vy = 0.f;   // velocity (px/s)
    float life = 0.f;           // remaining life (s); reclaimed when <= 0
    int   w = 6, h = 6;         // AABB size (px)

    // Spawn with direction and speed. Normalize input direction to avoid scaling bugs.
    void spawn(float sx, float sy, float dirx, float diry, float speed, float ttl)
    {
        alive = true;
        x = sx; y = sy;

        float len = std::sqrt(dirx * dirx + diry * diry);
        if (len < 1e-6f) { dirx = 1.f; diry = 0.f; len = 1.f; } // fallback to +X if degenerate
        dirx /= len; diry /= len;

        vx = dirx * speed;
        vy = diry * speed;
        life = ttl;
    }

    // AABB accessors (aligned with Player / NPC naming to keep call-sites uniform)
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }
};

/****************************  PlayerProjectile (fixed pool)  ****************************
 * Similar to EnemyProjectile with a few gameplay tags:
 *   - tint for rendering (r,g,b)
 *   - damage value
 *   - isAOE flag (visual/debug hint; actual AOE handled by systems)
 ***************************************************************************************/
struct PlayerProjectile
{
    bool  alive = false;
    float x = 0.f, y = 0.f;     // world-space top-left (center vs corner is irrelevant as long as consistent)
    float vx = 0.f, vy = 0.f;   // velocity (px/s)
    float life = 0.f;           // remaining life (s)
    int   w = 6, h = 6;         // AABB size (px)

    unsigned char r = 40, g = 200, b = 255; // default cyan for straight shots
    int  damage = 1;                          // default damage
    bool isAOE = false;                       // rendering/debug hint only

    // Implemented in cpp: keeps symmetry with EnemyProjectile::spawn
    void spawn(float sx, float sy, float dirx, float diry, float speed, float ttl);
    // Implemented in cpp: integrates motion and decreases life
    void update(float dt);
};


/**********************************  EnemyManager  **********************************
 * Owns:
 *   - NPC pool (MAX = 128)
 *   - Enemy projectile pool (BULLET_MAX)
 *   - Player projectile pool (kPlayerProjectileCapacity) for simple weapon logic
 *
 * Spawning:
 *   - frequency ramps up over time with a lower bound
 *   - in infinite worlds, positions are chosen around the camera ring
 *
 * Updates:
 *   - steering and movement for chasers
 *   - turret cooldowns and firing
 *   - bullet integration and culling
 *
 * Rendering:
 *   - draws NPCs and both projectile sets, clipped to viewport
 *************************************************************************************/
class EnemyManager
{
public:
    static const int MAX = 128;
    static const int BULLET_MAX = 256;

    bool isInfiniteWorld = false;   // toggled by save/load and mode switches

private:
    NPC enemies[MAX];
    TileMap* tileMap = nullptr;

    float elapsedSeconds = 0.f;   // global clock for difficulty ramp
    float spawnAccumulator = 0.f;   // scheduler accumulator

    // ----------------- projectile pools -----------------
    EnemyProjectile  enemyProjectiles[BULLET_MAX];
    static const int kPlayerProjectileCapacity = 256;
    PlayerProjectile playerProjectiles[kPlayerProjectileCapacity];

    // cached world size in pixels; used for clamping and spawn bounds
    int worldWidthPx = 0, worldHeightPx = 0;

    // spawn cadence parameters
    static constexpr float kSpawnBaseInterval = 1.6f;
    static constexpr float kSpawnMinInterval = 0.35f;
    static constexpr float kSpawnAccelPerSec = 0.02f;

    // small helpers
    static float frand01() { return (float)rand() / (float)RAND_MAX; }
    static float fclamp(float v, float a, float b) { return (v < a) ? a : ((v > b) ? b : v); }

    // pool slot acquisition; -1 when full
    int allocIndex();

    // compute world size in pixels from tile dimensions
    void mapPixelSize(int& outW, int& outH);

    // spawn a single NPC near the camera ring; px,py are player center to set facing
    void spawnOne(float camX, float camY, int viewW, int viewH, float px, float py);

    // enemy projectile slot; -1 when none available
    int allocBullet();

    // direction from NPC center (A) to player center (B)
    static inline void dirFromAToB(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh,
        float& outx, float& outy)
    {
        float acx = ax + aw * 0.5f, acy = ay + ah * 0.5f;
        float bcx = bx + bw * 0.5f, bcy = by + bh * 0.5f;
        outx = (bcx - acx); outy = (bcy - acy);
    }

    // AABB overlap predicate (non-separating axis, just a hit test)
    static inline bool aabbOverlap(float ax, float ay, int aw, int ah,
        float bx, float by, int bw, int bh)
    {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

public:
    // Initialize with a tile map. Caches pixel dimensions and clears pools.
    void init(TileMap* m);

    // Public AABB convenience for other modules (consistent signature style)
    static inline bool aabbIntersect(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh)
    {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

    // Spawner tick: advances cadence over time and emits units when due
    void trySpawn(float dt, float camX, float camY, int viewW, int viewH, float px, float py);

    // Update all NPCs and enemy systems: steering AI, turret cooldowns, enemy firing
    void updateAll(float dt, float px, float py);

    // Draw all NPCs relative to camera
    void drawAll(Window& win, float camX, float camY);

    // Player vs all live NPCs: resolve collision with minimal separation
    void checkPlayerCollision(Player& hero);

    // Optional read-only/raw access for external queries
    const NPC* getArray() const { return enemies; }
    NPC* getArray() { return enemies; }

    // Linear nearest-neighbour: returns center of closest living NPC to (px,py)
    // (px,py) is typically the hero center; outputs (tx,ty) as NPC center
    bool findNearestAlive(float px, float py, float& tx, float& ty) const;

    // Enemy projectiles: integrate motion, cull on lifetime or out-of-bounds
    void updateBullets(float dt);

    // Enemy bullets vs hero: apply knockback/damage as needed and destroy bullet
    void checkHeroHit(Player& hero);

    // Render enemy projectiles with viewport clipping
    void drawBullets(Window& win, float camX, float camY);

    // Emit one player projectile (straight shot or AOE-seeded)
    void spawnHeroBullet(float sx, float sy, float dirx, float diry, float speed, float ttl);

    // Integrate player projectiles and cull expired ones
    void updateHeroBullets(float dt);

    // Render player projectiles; mirror enemy bullet drawing path to keep behavior consistent
    void drawHeroBullets(Window& win, float camX, float camY);

    // Collision: player projectiles vs NPCs; apply damage/kill and return kill count
    int checkNPCHit();

    // AOE support:
    //  - aoeStrikeTopN: damage top-N nearest targets around (heroCx, heroCy)
    //  - spawnAoeBullet: helper to visualize or seed homing-like arcs
    int  aoeStrikeTopN(int N, int damage, float heroCx, float heroCy);
    void spawnAoeBullet(float sx, float sy, float tx, float ty, float speed, float ttl, int dmg);

    // Toggle infinite world mode; affects spawn logic and positional wrapping elsewhere
    void setInfinite(bool v) { isInfiniteWorld = v; }
};
