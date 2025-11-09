#pragma once
#include "GamesEngineeringBase.h"
using namespace GamesEngineeringBase;


// Forward declarations so friend functions can see the exact signatures
class Player;
class EnemyManager;
namespace SaveLoad {
    bool SaveToFile(const char* path,
        const Player& hero,
        const EnemyManager& npcs,
        float totalTime,
        int totalKills,
        bool infiniteMode);
    bool LoadFromFile(const char* path,
        Player& hero,
        EnemyManager& npcs,
        float& totalTime,
        int& totalKills,
        bool& infiniteMode);
}

/************************************  NPC  ************************************
 * Single enemy unit. Owns its state, update, and draw; lifetime and pooling are
 * handled externally by EnemyManager (no STL usage here). Rectangle hitbox (w,h).
 *******************************************************************************/
class NPC
{
public:
    // Types: 0 = chaser, 1 = turret (static, fire handled by system), 
    //        2 = light/fast, 3 = heavy/slow
    unsigned char type = 0;
    bool  alive = false;

    // Simple cooldown for turret behavior. Counted down by the system;
    // when <= 0 the turret may fire and the system resets it.
    float fireCD = 0.0f;

private:
    // World-space top-left position
    float x = 0.f, y = 0.f;

    // Normalized facing / movement direction
    float vx = 0.f, vy = 0.f;

    // Speed in pixels per second
    float speed = 0.f;

    // Axis-aligned rectangle size
    int   w = 24, h = 24;

    // Optional health; levels that ignore HP still allow forced kill()
    int   hp = 3;

    // Produce a unit vector from (dx,dy); returns (0,0) for near-zero length
    static void unitVector(float dx, float dy, float& ox, float& oy)
    {
        float len2 = dx * dx + dy * dy;
        if (len2 <= 1e-6f) { ox = 0.f; oy = 0.f; return; }
        float inv = 1.0f / (float)sqrt(len2);
        ox = dx * inv; oy = dy * inv;
    }

public:
    friend class EnemyManager;

    // ---- Lifetime control ----
    void kill() { alive = false; }
    bool isAlive() const { return alive; }

    // ---- One-shot spawn initialization ----
    // Sets position, type, base speed, and initial facing toward a target point.
    void initSpawn(float sx, float sy, unsigned char _type, float _speed, float faceTx, float faceTy)
    {
        x = sx; y = sy; type = _type; speed = _speed; alive = true; hp = 3;
        unitVector(faceTx - x, faceTy - y, vx, vy); // face player on spawn
    }

    // ---- Per-frame update ----
    // For non-turret types, steer smoothly toward target and move.
    // Clamps position inside [0, worldW-w] × [0, worldH-h].
    void update(float dt, float targetX, float targetY, int worldW, int worldH)
    {
        if (!alive) return;

        if (type != 1) { // move if not a turret
            // Steering sensitivity per type:
            // fast(2) turns quicker; heavy(3) turns slower; chaser(0) is in-between
            float steer = (type == 2 ? 0.35f : (type == 3 ? 0.15f : 0.20f));
            float inertia = 1.0f - steer;

            float ux, uy;
            unitVector(targetX - x, targetY - y, ux, uy);

            // Exponential smoothing toward target direction, then renormalize
            vx = inertia * vx + steer * ux;
            vy = inertia * vy + steer * uy;
            unitVector(vx, vy, vx, vy);

            x += vx * speed * dt;
            y += vy * speed * dt;
        }
        // type == 1 (turret) remains stationary; firing handled by system

        // Clamp inside world bounds
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x > worldW - w) x = (float)(worldW - w);
        if (y > worldH - h) y = (float)(worldH - h);

        // Optional: if hp <= 0, kill(); handled elsewhere or by applyDamage()
    }

    // ---- Rendering (camera top-left to screen space) ----
    void draw(Window& win, float camX, float camY)
    {
        if (!alive) return;
        const int W = (int)win.getWidth(), H = (int)win.getHeight();

        int sx = (int)(x - camX);
        int sy = (int)(y - camY);
        if (sx + w < 0 || sy + h < 0 || sx >= W || sy >= H) return;

        // Color per type for quick visual identification
        unsigned char r = 255, g = 0, b = 0;
        switch (type)
        {
        case 0: r = 255; g = 60;  b = 60;  break; // chaser
        case 1: r = 180; g = 0;   b = 255; break; // turret
        case 2: r = 40;  g = 230; b = 200; break; // light/fast
        case 3: r = 255; g = 150; b = 40;  break; // heavy
        default: r = 255; g = 0;  b = 0;   break;
        }

        for (int yy = 0; yy < h; ++yy) {
            int py = sy + yy; if (py < 0 || py >= H) continue;
            int base = py * W;
            for (int xx = 0; xx < w; ++xx) {
                int px = sx + xx; if (px < 0 || px >= W) continue;
                win.draw(base + px, r, g, b);
            }
        }
    }

    // Apply damage and handle death
    void applyDamage(int dmg) {
        if (!alive || dmg <= 0) return;
        if (hp > 0) {
            hp -= dmg;
            if (hp <= 0) kill();
        }
        else {
            // If HP is not used by the level, still allow forced kill
            kill();
        }
    }

    // ---- Read-only accessors (useful for collisions, etc.) ----
    float getX() const { return x; }
    float getY() const { return y; }
    int   getW() const { return w; }
    int   getH() const { return h; }

    // Unified hitbox access (aligned with Player naming)
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }

    // Type and simple attributes exposed for systems and serialization
    int   getType() const { return type; }
    void  setType(int t) { type = (unsigned char)t; }

    float getFireCD() const { return fireCD; }
    void  setFireCD(float v) { fireCD = v; }
    int   getHP() const { return hp; }

    // Allow SaveLoad to serialize/deserialize private fields safely
    friend bool SaveLoad::SaveToFile(const char*, const Player&, const EnemyManager&, float, int, bool);
    friend bool SaveLoad::LoadFromFile(const char*, Player&, EnemyManager&, float&, int&, bool&);
};
