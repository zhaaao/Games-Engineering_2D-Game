#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"      // needs map bounds and tile size
#include "SpriteSheet.h"
#include "Animator.h"
#include "NPCSystem.h"
using namespace GamesEngineeringBase;


/******************************  Player (Hero)  *********************************
 * Holds world position (float), processes WASD input with diagonal normalization,
 * selects animation row from facing direction, and plays columns while moving.
 * draw() converts camera-space to screen-space.
 *******************************************************************************/
class Player
{
public:
    enum Dir { Down = 0, Right = 1, Up = 2, Left = 3 };

    void bindMap(TileMap* m) { map = m; }

    // Adjustable hitbox (defaults to frame size for consistent feel)
    int hitboxW = 0;
    int hitboxH = 0;

private:
    // World-space top-left (pixels)
    float x = 0.f, y = 0.f;

    // Base move speed; gameplay tuning lives here
    float speed = 150.f;

    // Facing decides sprite-sheet row
    Dir   dir = Down;

    // Visuals and frame-advancement
    SpriteSheet* sheet = nullptr;
    Animator     anim;     // advances column: 0..3

    // Map for tile-block collisions and clamping
    TileMap* map = nullptr;

    // Knockback state: velocity-like impulse and remaining time
    float kx = 0.f, ky = 0.f;
    float kTime = 0.f;
    float hitCooldown = 0.f; // brief i-frame to avoid multiple triggers per frame

    // Weapon cadence (auto-fire on nearest NPC)
    float shootCD = 0.0f;  // countdown
    float shootInterval = 0.35f; // seconds per shot; powerups can lower this

    // AOE cadence and parameters
    float aoeCD = 0.0f;
    float aoeInterval = 1.0f;  // intentionally long cooldown
    int   aoeN = 3;     // pick top-N by HP
    int   aoeDamage = 2;     // damage per target

    // Frame size helpers (fallback 32×32 before sprite attached)
    int getFrameW() const { return sheet ? sheet->getFrameW() : 32; }
    int getFrameH() const { return sheet ? sheet->getFrameH() : 32; }

public:
    // Constructor: safe defaults so gameplay is viable before assets attach
    Player()
        : x(0.f), y(0.f), speed(150.f), dir(Down),
        sheet(nullptr), anim(), map(nullptr)
    {
        // Start with a conservative box; attachSprite will overwrite with real frame size
        setHitbox(32, 32);
    }

    void attachSprite(SpriteSheet* s) {
        sheet = s;
        if (sheet && sheet->valid()) {
            // Always sync hitbox to frame size to avoid stale boxes after asset swaps
            setHitbox(sheet->getFrameW(), sheet->getFrameH());
        }
    }

    void setPosition(float px, float py) { x = px; y = py; }
    void setSpeed(float s) { speed = s; }

    // Set after construction/init; using frame size or slightly smaller is typical
    void setHitbox(int w, int h) { hitboxW = w; hitboxH = h; }

    // Centered hitbox relative to the rendered sprite
    float getHitboxX() const { return x + (getFrameW() - hitboxW) * 0.5f; }
    float getHitboxY() const { return y + (getFrameH() - hitboxH) * 0.5f; }
    int   getHitboxW() const { return hitboxW; }
    int   getHitboxH() const { return hitboxH; }

    // Rendering helpers
    float getX() const { return x; }
    float getY() const { return y; }
    int   getW() const { return sheet ? sheet->getFrameW() : 0; }
    int   getH() const { return sheet ? sheet->getFrameH() : 0; }

    // Powerup-facing accessors
    float getShootInterval() const { return shootInterval; }
    void  setShootInterval(float v) { if (v > 0.f) shootInterval = v; }
    int   getAOEN() const { return aoeN; }
    float getAOEInterval() const { return aoeInterval; }

    // World clamp used by external systems that compute bounds
    void clampPosition(float minX, float minY, float maxX, float maxY)
    {
        if (x < minX) x = minX;
        if (y < minY) y = minY;
        if (x > maxX) x = maxX;
        if (y > maxY) y = maxY;
    }

    // Input + animation step
    void update(Window& input, float dt)
    {
        // 1) Read input → raw move vector
        float vx = 0.f, vy = 0.f;
        if (input.keyPressed('W')) vy -= 1.f;
        if (input.keyPressed('S')) vy += 1.f;
        if (input.keyPressed('A')) vx -= 1.f;
        if (input.keyPressed('D')) vx += 1.f;

        // 2) Facing follows most recent non-zero axis
        if (vy > 0.0f)      dir = Down;
        else if (vy < 0.0f) dir = Up;
        if (vx > 0.0f)      dir = Right;
        else if (vx < 0.0f) dir = Left;

        // 3) Normalize to remove diagonal speed boost
        float len = std::sqrt(vx * vx + vy * vy);
        if (len > 0.0001f) { vx /= len; vy /= len; }

        // Displacement this frame = input motion + knockback contribution
        float dx = vx * speed * dt;
        float dy = vy * speed * dt;

        // Knockback decay (exponential damping) and simple i-frame
        if (kTime > 0.f) {
            dx += kx * dt;
            dy += ky * dt;
            kTime -= dt;

            float damp = std::exp(-6.f * dt); // higher → stops faster
            kx *= damp; ky *= damp;

            if (kTime <= 0.f) { kTime = 0.f; kx = 0.f; ky = 0.f; }
        }
        if (hitCooldown > 0.f) hitCooldown -= dt;

        // 4) Movement with tile collision on the centered hitbox
        if (!map) {
            x += dx;
            y += dy;
        }
        else {
            const int tw = map->getTileW(), th = map->getTileH();

            // Prepare frame and box sizes and the center offset
            const int fw = getFrameW();
            const int fh = getFrameH();
            const int hw = getHitboxW();
            const int hh = getHitboxH();
            const float offX = (fw - hw) * 0.5f;
            const float offY = (fh - hh) * 0.5f;

            // Candidate render position (already includes knockback)
            float nx = x + dx;
            float ny = y + dy;

            // Convert to hitbox-space for collision math
            float hx = nx + offX;
            float hy = ny + offY;

            // Horizontal sweep: test against tiles along movement edge
            if (dx > 0.f) {
                int rightPix = (int)(hx + hw - 1);
                int tx = rightPix / tw;
                int topRow = (int)hy / th;
                int botRow = (int)(hy + hh - 1) / th;
                for (int ty = topRow; ty <= botRow; ++ty) {
                    if (map->isBlockedAt(tx, ty)) {
                        hx = (float)(tx * tw - hw); // stop left of the blocking tile
                        break;
                    }
                }
            }
            else if (dx < 0.f) {
                int leftPix = (int)hx;
                int tx = leftPix / tw;
                int topRow = (int)hy / th;
                int botRow = (int)(hy + hh - 1) / th;
                for (int ty = topRow; ty <= botRow; ++ty) {
                    if (map->isBlockedAt(tx, ty)) {
                        hx = (float)((tx + 1) * tw); // stop right of the blocking tile
                        break;
                    }
                }
            }

            // Vertical sweep: same idea on Y axis
            if (dy > 0.f) {
                int bottomPix = (int)(hy + hh - 1);
                int ty = bottomPix / th;
                int leftCol = (int)hx / tw;
                int rightCol = (int)(hx + hw - 1) / tw;
                for (int tx = leftCol; tx <= rightCol; ++tx) {
                    if (map->isBlockedAt(tx, ty)) {
                        hy = (float)(ty * th - hh); // stop above the blocking tile
                        break;
                    }
                }
            }
            else if (dy < 0.f) {
                int topTile = (int)hy / th;
                int leftCol = (int)hx / tw;
                int rightCol = (int)(hx + hw - 1) / tw;
                for (int tx = leftCol; tx <= rightCol; ++tx) {
                    if (map->isBlockedAt(tx, topTile)) {
                        hy = (float)((topTile + 1) * th); // stop below the blocking tile
                        break;
                    }
                }
            }

            // Convert hitbox-space back to render-space
            nx = hx - offX;
            ny = hy - offY;

            x = nx;
            y = ny;
        }

        // 5) Animation: play while moving; freeze on column 0 while idle
        if (len > 0.0f) anim.start(); else anim.stop();
        anim.update(dt);
    }

    // Draw at camera-relative position
    void draw(Window& w, float camX, float camY)
    {
        if (!sheet || !sheet->valid()) return;
        int sx = (int)(x - camX);
        int sy = (int)(y - camY);
        sheet->drawFrame(w, (int)dir, anim.current(), sx, sy);
    }

    // Apply a knockback impulse: (dirX,dirY) direction, power in px/s, duration in s
    void applyKnockback(float dirX, float dirY, float power, float duration)
    {
        if (hitCooldown > 0.f) return;   // debounce against multi-hit in one frame
        float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len < 1e-6f) return;
        dirX /= len; dirY /= len;

        kx = dirX * power;
        ky = dirY * power;
        kTime = duration;

        hitCooldown = 0.10f;             // brief cooldown window
    }

    // Auto-attack: find nearest NPC and request a hero bullet from EnemyManager
    // Kept separate from update() to keep input/motion decoupled from combat
    void updateAttack(float dt, class EnemyManager& npcs)
    {
        // Cooldown gate
        if (shootCD > 0.f) { shootCD -= dt; return; }

        // Query nearest target; bail if none
        float tx, ty;
        if (!npcs.findNearestAlive(x, y, tx, ty)) return;

        // Fire from hero center toward target center
        float sx = getHitboxX() + getHitboxW() * 0.5f;
        float sy = getHitboxY() + getHitboxH() * 0.5f;
        float dx = tx - sx, dy = ty - sy;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-5f) return;
        dx /= len; dy /= len;

        // Request projectile; 420 px/s, 1.2 s TTL as current tuning
        npcs.spawnHeroBullet(sx, sy, dx, dy, 420.f, 1.2f);

        // Reset cadence
        shootCD = shootInterval;
    }

    // Configure AOE parameters atomically
    void setAOEParams(int N, int dmg, float interval) {
        if (N > 0) aoeN = N;
        if (dmg > 0) aoeDamage = dmg;
        if (interval > 0.f) aoeInterval = interval;
    }

    // AOE trigger: on 'J' and when cooldown is ready, fire at top-N HP targets
    void updateAOE(float dt, EnemyManager& npcs, Window& input)
    {
        if (aoeCD > 0.f) aoeCD -= dt;

        if (aoeCD <= 0.f && input.keyPressed('J')) {
            float cx = getHitboxX() + getHitboxW() * 0.5f;
            float cy = getHitboxY() + getHitboxH() * 0.5f;

            npcs.aoeStrikeTopN(aoeN, aoeDamage, cx, cy);

            aoeCD = aoeInterval; // long cooldown by design
        }
    }
};
