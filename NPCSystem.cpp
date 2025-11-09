#include "NPCSystem.h"
#include "Player.h"      // needs full Player definition
#include <cmath>

/* PlayerProjectile -----------------------------------------------------------
 * Spawn: reset to a clean straight-shot state (tint/damage/flags) and set kinematics.
 * Update: integrate motion, age out by TTL.
 * Keep logic minimal because the pool calls this per frame for up to 256 entries.
 * ---------------------------------------------------------------------------*/
void PlayerProjectile::spawn(float sx, float sy, float dirx, float diry, float speed, float ttl)
{
    alive = true;
    x = sx; y = sy;

    // Direction is assumed normalized by caller sometimes; enforce here to be safe.
    vx = dirx * speed;
    vy = diry * speed;
    life = ttl;

    // Fixed AABB for now (fast raster blit, predictable collision)
    w = 6;
    h = 6;

    // Reset semantic payload each time, so any reused slot doesn’t leak old state.
    r = 40; g = 200; b = 255;  // straight shot = cyan
    damage = 1;
    isAOE = false;
}

void PlayerProjectile::update(float dt) {
    if (!alive) return;

    // Integrate position
    x += vx * dt;
    y += vy * dt;

    // Age-out
    life -= dt;
    if (life <= 0.f) alive = false;
}

/* EnemyManager internals -----------------------------------------------------
 * Slot allocation, world size cache, spawn logic, bullet allocation.
 * The manager owns raw arrays so all helpers are O(N) and branch-light.
 * ---------------------------------------------------------------------------*/
int EnemyManager::allocIndex()
{
    // Find first dead slot; return -1 when the pool is saturated.
    for (int i = 0; i < MAX; ++i) if (!enemies[i].isAlive()) return i;
    return -1;
}

void EnemyManager::mapPixelSize(int& outW, int& outH)
{
    // Derive world size in pixels from tile grid; cache elsewhere for draw/cull.
    if (!tileMap) { outW = outH = 0; return; }
    outW = tileMap->getWidth() * tileMap->getTileW();
    outH = tileMap->getHeight() * tileMap->getTileH();
}

/* Spawning -------------------------------------------------------------------
 * Spawn around the camera “ring” so enemies enter from off-screen.
 * Infinite world: no clamping; finite world: clamp to map bounds.
 * Type distribution is weighted and each type gets tailored stats.
 * ---------------------------------------------------------------------------*/
void EnemyManager::spawnOne(float camX, float camY, int viewW, int viewH, float px, float py)
{
    int idx = allocIndex();
    if (idx < 0) return;

    int worldW, worldH;
    mapPixelSize(worldW, worldH);
    if (worldW <= 0 || worldH <= 0) return;

    // Choose an entry edge (0..3) and randomize along that edge; keep a margin off-screen.
    const int margin = 64;
    float sx = 0.f, sy = 0.f;
    int edge = (int)(frand01() * 4.f);
    if (edge == 0) { sx = camX - margin - 24;   sy = camY + frand01() * viewH; }
    else if (edge == 1) { sx = camX + viewW + margin; sy = camY + frand01() * viewH; }
    else if (edge == 2) { sx = camX + frand01() * viewW; sy = camY - margin - 24; }
    else { sx = camX + frand01() * viewW; sy = camY + viewH + margin; }

    // Finite maps clamp to valid range. Infinite maps allow free placement.
    if (!isInfiniteWorld) {
        sx = fclamp(sx, 0.f, (float)(worldW - 24));
        sy = fclamp(sy, 0.f, (float)(worldH - 24));
    }

    // Weighted type choice keeps the battlefield mix interesting.
    // 0: chaser (60%), 1: turret (20%), 2: light/fast (10%), 3: heavy (10%)
    float r = frand01();
    unsigned char type = (r < 0.60f) ? 0 : (r < 0.80f) ? 1 : (r < 0.90f) ? 2 : 3;

    // Per-type defaults. Speed drives pathing difficulty; HP/size alter TTK and collision risk.
    float spd = 0.f;
    int   hp = 3;
    int   w = 24, h = 24;

    switch (type)
    {
    case 0: spd = 60.f;  hp = 3;  w = h = 24; break; // chaser: mid-speed, mid HP
    case 1: spd = 0.f;   hp = 4;  w = h = 24; break; // turret: static, tougher
    case 2: spd = 110.f; hp = 1;  w = h = 20; break; // light: fast, fragile, smaller
    case 3: spd = 40.f;  hp = 6;  w = h = 28; break; // heavy: slow, tanky, larger
    }

    // Spawn looking at the player so the initial steering direction is sensible.
    enemies[idx].initSpawn(sx, sy, type, spd, px, py);

    // Patch size/HP via friend access — serialization relies on these exact fields.
    enemies[idx].w = w;
    enemies[idx].h = h;
    enemies[idx].hp = hp;

    // Turrets get an immediate near-term cooldown; movers get a huge CD to disable stray shots.
    if (type == 1) {
        enemies[idx].setFireCD(0.2f + 0.2f * frand01()); // 0.2–0.4s
    }
    else {
        enemies[idx].setFireCD(999.f);
    }
}

int EnemyManager::allocBullet()
{
    // First-fit for enemy projectile slots
    for (int i = 0; i < BULLET_MAX; ++i)
        if (!enemyProjectiles[i].alive) return i;
    return -1;
}

/* Initialization --------------------------------------------------------------
 * Reset pools and cached state. Seed RNG deterministically for reproducibility
 * (okay for coursework; switch to nondeterministic in production if needed).
 * ---------------------------------------------------------------------------*/
void EnemyManager::init(TileMap* m)
{
    tileMap = m;
    elapsedSeconds = 0.f;
    spawnAccumulator = 0.f;

    for (int i = 0; i < MAX; ++i) enemies[i].kill();

    srand(12345); // replace or remove if a global RNG policy exists

    // Clear projectile pools
    for (int i = 0; i < BULLET_MAX; ++i)               enemyProjectiles[i].alive = false;
    for (int i = 0; i < kPlayerProjectileCapacity; ++i) playerProjectiles[i].alive = false;

    // Cache world size for culling and clamp math
    mapPixelSize(worldWidthPx, worldHeightPx);
}

/* Spawner tick ----------------------------------------------------------------
 * Interval decreases linearly with elapsed time then clamps at a minimum.
 * After 60s, emit two per tick to push pressure up without changing cadence.
 * ---------------------------------------------------------------------------*/
void EnemyManager::trySpawn(float dt, float camX, float camY, int viewW, int viewH, float px, float py)
{
    elapsedSeconds += dt;

    float interval = kSpawnBaseInterval - kSpawnAccelPerSec * elapsedSeconds;
    if (interval < kSpawnMinInterval) interval = kSpawnMinInterval;

    spawnAccumulator += dt;
    if (spawnAccumulator >= interval) {
        spawnAccumulator -= interval;

        int count = (elapsedSeconds > 60.f) ? 2 : 1;
        for (int i = 0; i < count; ++i)
            spawnOne(camX, camY, viewW, viewH, px, py);
    }
}

/* Update loop -----------------------------------------------------------------
 * Compute effective world bounds (disable clamping for infinite mode).
 * Step each NPC, then handle turret fire with randomized cooldown desync.
 * ---------------------------------------------------------------------------*/
void EnemyManager::updateAll(float dt, float px, float py)
{
    int worldW, worldH;
    mapPixelSize(worldW, worldH);

    // In infinite mode, make the bounds effectively unreachable so NPC::update clamps don’t trigger.
    if (isInfiniteWorld) {
        worldW = 1 << 29;  // huge bounds; avoids overflow, keeps integer math cheap
        worldH = 1 << 29;
    }

    for (int i = 0; i < MAX; ++i)
    {
        if (!enemies[i].isAlive()) continue;

        // NPC owns its own steering/velocity integration
        enemies[i].update(dt, px, py, worldW, worldH);

        // For finite maps, enforce post-step clamping (robust even if internal logic changes later).
        if (!isInfiniteWorld) {
            float x = enemies[i].getX();
            float y = enemies[i].getY();
            int   w = enemies[i].getW();
            int   h = enemies[i].getH();

            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x > worldW - w) x = (float)(worldW - w);
            if (y > worldH - h) y = (float)(worldH - h);

            enemies[i].x = x; // friend write
            enemies[i].y = y;
        }
        // Infinite mode: no clamps, allow roam beyond original map.

        // Turret-only firing
        if (enemies[i].type == 1)
        {
            enemies[i].fireCD -= dt;

            if (enemies[i].fireCD <= 0.f)
            {
                // Fire from turret center toward player center
                float cx = enemies[i].getX() + enemies[i].getHitboxW() * 0.5f;
                float cy = enemies[i].getY() + enemies[i].getHitboxH() * 0.5f;
                float dirx = px - cx;
                float diry = py - cy;

                int bi = allocBullet();
                if (bi >= 0)
                {
                    float sx = cx - 3.f; // center a 6×6 projectile
                    float sy = cy - 3.f;

                    const float BULLET_SPEED = 280.f;
                    const float BULLET_TTL = 3.0f;
                    enemyProjectiles[bi].spawn(sx, sy, dirx, diry, BULLET_SPEED, BULLET_TTL);
                }

                // Desync turrets to avoid a single global beat
                enemies[i].fireCD = 1.0f + 0.4f * frand01();
            }
        }
    }
}

/* Render NPCs -----------------------------------------------------------------*/
void EnemyManager::drawAll(Window& win, float camX, float camY)
{
    for (int i = 0; i < MAX; ++i)
        if (enemies[i].isAlive())
            enemies[i].draw(win, camX, camY);
}

/* Player vs NPC collision ------------------------------------------------------
 * Resolve minimal separation along the axis of least overlap to prevent tunneling.
 * Apply a short knockback impulse using the engine’s existing helper.
 * ---------------------------------------------------------------------------*/
void EnemyManager::checkPlayerCollision(Player& hero)
{
    // Player AABB in world space
    float hx = hero.getHitboxX();
    float hy = hero.getHitboxY();
    float hw = (float)hero.getHitboxW();
    float hh = (float)hero.getHitboxH();

    // Convert from hitbox back to render-space anchor after resolution
    float offX = (hero.getW() - hw) * 0.5f;
    float offY = (hero.getH() - hh) * 0.5f;

    for (int i = 0; i < MAX; ++i)
    {
        if (!enemies[i].isAlive()) continue;

        float nx = enemies[i].getHitboxX();
        float ny = enemies[i].getHitboxY();
        float nw = (float)enemies[i].getHitboxW();
        float nh = (float)enemies[i].getHitboxH();

        if (!aabbIntersect(hx, hy, hw, hh, nx, ny, nw, nh))
            continue;

        // Minimal translation vector: compare overlap on X vs Y
        float hcX = hx + hw * 0.5f, hcY = hy + hh * 0.5f;
        float ncX = nx + nw * 0.5f, ncY = ny + nh * 0.5f;
        float dx = hcX - ncX, dy = hcY - ncY;
        float ox = (hw * 0.5f + nw * 0.5f) - (dx >= 0 ? dx : -dx);
        float oy = (hh * 0.5f + nh * 0.5f) - (dy >= 0 ? dy : -dy);

        if (ox < oy) hx += (dx >= 0 ? ox : -ox);
        else         hy += (dy >= 0 ? oy : -oy);

        // Convert resolved hitbox back to player position
        hero.setPosition(hx - offX, hy - offY);

        // Apply short knockback away from the NPC center
        float heroCx = (hx + hw * 0.5f);
        float heroCy = (hy + hh * 0.5f);
        float npcCx = (nx + nw * 0.5f);
        float npcCy = (ny + nh * 0.5f);
        hero.applyKnockback(heroCx - npcCx, heroCy - npcCy,
            220.f /*power px/s*/, 0.12f /*duration s*/);

        // Optional: hook for damage/i-frames/audio
        // hero.takeHit();
    }
}

/* Nearest target query --------------------------------------------------------
 * Linear scan is fine for ≤128 NPCs; returns center of the closest alive NPC.
 * Useful for homing, aim assist, or AOE targeting.
 * ---------------------------------------------------------------------------*/
bool EnemyManager::findNearestAlive(float px, float py, float& tx, float& ty) const
{
    float bestD2 = 1e30f;
    bool found = false;

    for (int i = 0; i < MAX; ++i) {
        const NPC& n = enemies[i];
        if (!n.alive) continue;

        float cx = n.getHitboxX() + n.getHitboxW() * 0.5f;
        float cy = n.getHitboxY() + n.getHitboxH() * 0.5f;

        float dx = cx - px, dy = cy - py;
        float d2 = dx * dx + dy * dy;

        if (d2 < bestD2) { bestD2 = d2; tx = cx; ty = cy; found = true; }
    }
    return found;
}

/* Enemy bullets ---------------------------------------------------------------
 * Integrate motion, age out, and cull by world bounds in finite mode.
 * Infinite mode keeps bullets alive until TTL expires.
 * ---------------------------------------------------------------------------*/
void EnemyManager::updateBullets(float dt)
{
    for (int i = 0; i < BULLET_MAX; ++i)
    {
        if (!enemyProjectiles[i].alive) continue;

        enemyProjectiles[i].x += enemyProjectiles[i].vx * dt;
        enemyProjectiles[i].y += enemyProjectiles[i].vy * dt;
        enemyProjectiles[i].life -= dt;

        bool outByWorld = (enemyProjectiles[i].x + enemyProjectiles[i].w < 0 ||
            enemyProjectiles[i].y + enemyProjectiles[i].h < 0 ||
            enemyProjectiles[i].x > worldWidthPx ||
            enemyProjectiles[i].y > worldHeightPx);

        if (enemyProjectiles[i].life <= 0.f || (!isInfiniteWorld && outByWorld)) {
            enemyProjectiles[i].alive = false;
        }
    }
}

/* Enemy bullets vs hero -------------------------------------------------------
 * On hit: apply a short knockback away from bullet center, then destroy bullet.
 * Hook for damage/i-frames if needed.
 * ---------------------------------------------------------------------------*/
void EnemyManager::checkHeroHit(Player& hero)
{
    float hx = hero.getHitboxX();
    float hy = hero.getHitboxY();
    int   hw = hero.getHitboxW();
    int   hh = hero.getHitboxH();

    float hcx = hx + hw * 0.5f;
    float hcy = hy + hh * 0.5f;

    for (int i = 0; i < BULLET_MAX; ++i)
    {
        if (!enemyProjectiles[i].alive) continue;

        if (aabbOverlap(enemyProjectiles[i].getHitboxX(), enemyProjectiles[i].getHitboxY(),
            enemyProjectiles[i].getHitboxW(), enemyProjectiles[i].getHitboxH(),
            hx, hy, hw, hh))
        {
            float bx = enemyProjectiles[i].x + enemyProjectiles[i].w * 0.5f;
            float by = enemyProjectiles[i].y + enemyProjectiles[i].h * 0.5f;

            hero.applyKnockback(hcx - bx, hcy - by, 220.f, 0.12f);

            // Optional: hero.takeHit();
            enemyProjectiles[i].alive = false;
        }
    }
}

/* Draw enemy bullets ----------------------------------------------------------
 * Clip against viewport to avoid out-of-bounds writes; draw small red squares.
 * ---------------------------------------------------------------------------*/
void EnemyManager::drawBullets(Window& win, float camX, float camY)
{
    const int W = (int)win.getWidth();
    const int H = (int)win.getHeight();

    for (int i = 0; i < BULLET_MAX; ++i)
    {
        if (!enemyProjectiles[i].alive) continue;

        int sx = (int)(enemyProjectiles[i].x - camX);
        int sy = (int)(enemyProjectiles[i].y - camY);
        int bw = enemyProjectiles[i].h;   // 6×6 square
        int bh = enemyProjectiles[i].h;

        if (sx >= W || sy >= H || sx + bw <= 0 || sy + bh <= 0) continue;

        int x0 = sx < 0 ? 0 : sx;
        int y0 = sy < 0 ? 0 : sy;
        int x1 = sx + bw; if (x1 > W) x1 = W;
        int y1 = sy + bh; if (y1 > H) y1 = H;

        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                win.draw(x, y, 255, 40, 40);
    }
}

/* Player projectiles ----------------------------------------------------------
 * Spawn into first free slot; drop if saturated (could upgrade to ring buffer).
 * ---------------------------------------------------------------------------*/
void EnemyManager::spawnHeroBullet(float sx, float sy, float dirx, float diry, float speed, float ttl)
{
    for (int i = 0; i < kPlayerProjectileCapacity; ++i) {
        if (!playerProjectiles[i].alive) {
            playerProjectiles[i].spawn(sx, sy, dirx, diry, speed, ttl);
            return;
        }
    }
    // pool full -> drop; alternative: overwrite oldest
}

void EnemyManager::updateHeroBullets(float dt)
{
    for (int i = 0; i < kPlayerProjectileCapacity; ++i)
        if (playerProjectiles[i].alive)
            playerProjectiles[i].update(dt);
}

/* Draw player bullets ---------------------------------------------------------
 * Same clipping strategy as enemy bullets; tint taken from projectile payload.
 * ---------------------------------------------------------------------------*/
void EnemyManager::drawHeroBullets(Window& win, float camX, float camY)
{
    const int W = (int)win.getWidth();
    const int H = (int)win.getHeight();

    for (int i = 0; i < kPlayerProjectileCapacity; ++i) {
        const PlayerProjectile& b = playerProjectiles[i];
        if (!b.alive) continue;

        int sx = (int)(b.x - camX);
        int sy = (int)(b.y - camY);
        if (sx + b.w < 0 || sy + b.h < 0 || sx >= W || sy >= H) continue;

        int x0 = sx < 0 ? 0 : sx;
        int y0 = sy < 0 ? 0 : sy;
        int x1 = sx + b.w; if (x1 > W) x1 = W;
        int y1 = sy + b.h; if (y1 > H) y1 = H;

        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                win.draw(x, y, b.r, b.g, b.b);
    }
}

/* Collision: player bullets vs NPCs ------------------------------------------
 * On hit: consume the projectile, apply its damage to the NPC, kill on <= 0.
 * Returns number of kills this frame (useful for scoring).
 * ---------------------------------------------------------------------------*/
int EnemyManager::checkNPCHit()
{
    int kills = 0;

    for (int bi = 0; bi < kPlayerProjectileCapacity; ++bi) {
        PlayerProjectile& b = playerProjectiles[bi];
        if (!b.alive) continue;

        for (int i = 0; i < MAX; ++i) {
            NPC& n = enemies[i];
            if (!n.alive) continue;

            if (aabbOverlap(b.x, b.y, b.w, b.h, n.x, n.y, n.w, n.h)) {
                b.alive = false;

                if (n.hp > 0) {
                    n.hp -= b.damage;        // honor per-projectile damage
                    if (n.hp <= 0) { n.kill(); ++kills; }
                }
                else {
                    // If HP is not configured for this level, allow forced kill
                    n.kill(); ++kills;
                }
                break; // next projectile
            }
        }
    }
    return kills;
}

/* AOE selection: top-N by HP --------------------------------------------------
 * Pick the N highest-HP living targets (no repeats), then fire magenta AOE shots
 * from the hero center toward each target. Returns the number of shots emitted.
 * ---------------------------------------------------------------------------*/
int EnemyManager::aoeStrikeTopN(int N, int damage, float heroCx, float heroCy)
{
    if (N <= 0 || damage <= 0) return 0;

    bool picked[MAX];           // mark targets already chosen
    for (int i = 0; i < MAX; ++i) picked[i] = false;

    int fired = 0;

    for (int k = 0; k < N; ++k)
    {
        int bestIdx = -1;
        int bestHP = -1;

        // Highest-HP policy biases AOE to priority targets
        for (int i = 0; i < MAX; ++i)
        {
            if (!enemies[i].alive || picked[i]) continue;
            int hp = (enemies[i].hp > 0 ? enemies[i].hp : 1); // fallback when hp uninitialized
            if (hp > bestHP) { bestHP = hp; bestIdx = i; }
        }

        if (bestIdx < 0) break;      // nothing left to target
        picked[bestIdx] = true;

        float tx = enemies[bestIdx].x + enemies[bestIdx].w * 0.5f;
        float ty = enemies[bestIdx].y + enemies[bestIdx].h * 0.5f;

        // AOE projectiles: faster, shorter TTL, distinct tint
        spawnAoeBullet(heroCx, heroCy, tx, ty, /*speed*/520.f, /*ttl*/0.9f, damage);
        ++fired;
    }

    return fired;
}

/* AOE projectile spawn --------------------------------------------------------
 * Reuses the player projectile pool. Normalizes direction, colors magenta, and
 * stamps damage/flag so render and hit logic can differentiate it.
 * ---------------------------------------------------------------------------*/
void EnemyManager::spawnAoeBullet(float sx, float sy, float tx, float ty, float speed, float ttl, int dmg)
{
    for (int i = 0; i < kPlayerProjectileCapacity; ++i) {
        if (!playerProjectiles[i].alive) {
            float dx = tx - sx, dy = ty - sy;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) { dx = 1.f; dy = 0.f; len = 1.f; }
            dx /= len; dy /= len;

            playerProjectiles[i].spawn(sx, sy, dx, dy, speed, ttl);

            // Visual identity for AOE rounds
            playerProjectiles[i].r = 255;
            playerProjectiles[i].g = 50;
            playerProjectiles[i].b = 200;

            playerProjectiles[i].damage = (dmg > 0 ? dmg : 1);
            playerProjectiles[i].isAOE = true;
            return;
        }
    }
    // Pool saturated: skip emission
}
