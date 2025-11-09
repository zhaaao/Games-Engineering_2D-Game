#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include "GamesEngineeringBase.h"
#include "gfx_utils.h"
#include "blit.h"
#include "TileMap.h"
#include "SpriteSheet.h"
#include "Animator.h"
#include "Player.h" 
#include "NPCSystem.h"
#include "PickupSystem.h"
#include "SaveLoad.h"

using namespace GamesEngineeringBase;
using namespace std;

// -----------------------------------------------------------------------------
// Runtime mode toggle. Fixed = finite map with camera clamping.
// Infinite = wrapping map; spawn/culling rules differ in systems.
// -----------------------------------------------------------------------------
enum class GameMode { Fixed, Infinite };
GameMode gMode = GameMode::Fixed;

struct PerfLogger {
    std::ofstream out;
    double accum = 0.0;
    bool ready = false;

    void init(const char* path) {
        std::filesystem::create_directories("logs");
        out.open(path, std::ios::out | std::ios::trunc);
        if (out) {
            out << "time,fps,enemies\n"; // CSV header
            ready = true;
        }
    }
    void tick(float dt,
        float totalTime,
        float fpsSmoothed,
        const EnemyManager& mgr)
    {
        if (!ready) return;
        accum += dt;
        if (accum < 1.0) return;          // log every ~1s
        accum -= 1.0;

        // count alive enemies without touching internals
        int alive = 0;
        const NPC* arr = mgr.getArray();
        for (int i = 0; i < EnemyManager::MAX; ++i)
            if (arr[i].isAlive()) ++alive;

        out << std::fixed << std::setprecision(2)
            << totalTime << ","
            << (double)fpsSmoothed << ","
            << alive << "\n";
        out.flush();
    }
    void close() { if (out) out.close(); }
};
// ============================== FPS smoothing =================================
// Sliding window of recent dt values. Clamp spikes and zeros to keep display sane.
// ============================================================================== 
static const int FPS_SAMPLES = 120; // ~1–2s window
float  dtBuf[FPS_SAMPLES];
int    dtIdx = 0;
int    dtCount = 0;
double dtSum = 0.0;
int    fpsDisplay = 60;

// Initialize buffer so first few frames don’t read uninitialized memory.
void initFpsBuffer() {
    for (int i = 0; i < FPS_SAMPLES; ++i) dtBuf[i] = 0.0f;
    dtIdx = 0; dtCount = 0; dtSum = 0.0; fpsDisplay = 60;
}

int main()
{
    // Window + content bootstrap
    Window canvas;
    canvas.create(960, 540, "prototype");

    TileMap map;
    if (!map.load("Resources/tiles.txt")) {
        printf("Failed to load tiles.txt\n");
        return 1;
    }
    map.setImageFolder("Resources/"); // expects 0.png, 1.png, ... co-located

    // Mode selection at startup (console)
    printf("Select mode: [1] Fixed world   [2] Infinite (wrapping) world\n");
    printf("Your choice: ");
    int m = 1;
    std::cin >> m;
    gMode = (m == 2 ? GameMode::Infinite : GameMode::Fixed);

    // Map wrapping mirrors the chosen mode
    map.setWrap(gMode == GameMode::Infinite);

    // Hero sprite (rows: down/right/left/up in that order; 4 columns for walk cycle)
    SpriteSheet heroSheet;
    if (!heroSheet.load("Resources/Abigail.png", /*frameW*/16, /*frameH*/32, /*rows*/13, /*cols*/4)) {
        printf("❌ Failed to load Abigail sprite\n");
        return 1;
    }

    // Player wiring
    Player hero;
    hero.attachSprite(&heroSheet);
    hero.bindMap(&map);   // enable tile-block collision
    hero.setSpeed(150.f);

    // Enemy system: tie to map for sizes and wrapping flags
    EnemyManager npcSys;
    npcSys.init(&map);
    npcSys.setInfinite(gMode == GameMode::Infinite);

    // Powerup pickups: same wrapping policy as world
    PickupSystem pickups;
    pickups.init(&map);
    pickups.setInfinite(gMode == GameMode::Infinite);

    // Start centered to avoid a large initial camera jump
    float startX = (float)(map.getPixelWidth() / 2 - hero.getW() / 2);
    float startY = (float)(map.getPixelHeight() / 2 - hero.getH() / 2);
    hero.setPosition(startX, startY);

    // Timing utilities
    Timer timer;

    // HUD counters
    float totalTime = 0.f;
    int   totalKills = 0;
    float fpsSmoothed = 60.f;

    // Camera anchors the hero; recomputed every frame
    float camX = 0.0f;
    float camY = 0.0f;
    const float camSpeed = 220.0f; // reserved if manual camera ever needed

    // Tile size used by level content (kept explicit to match coursework)
    const int TILE = 32;

    initFpsBuffer();
    PerfLogger perf;
    bool isInfinite = (gMode == GameMode::Infinite);
    if (isInfinite)
        perf.init("logs/performance_infinite.csv");
    else
        perf.init("logs/performance_fixed.csv");
    // logs/performance.csv
    // ================================ Main loop ==============================
    while (true)
    {
        // --- Frame timing
        float dt = timer.dt();

        // FPS smoothing: clamp spikes (alt-tab) and zeros, then slide/avg.
        {
            float dtClamp = dt;
            if (dtClamp > 0.100f)     dtClamp = 0.100f;      // cap long frames at 100 ms
            if (dtClamp <= 0.000001f) dtClamp = 0.000001f;   // avoid zero/negative

            dtSum -= dtBuf[dtIdx];
            dtBuf[dtIdx] = dtClamp;
            dtSum += dtClamp;

            dtIdx = (dtIdx + 1) % FPS_SAMPLES;
            if (dtCount < FPS_SAMPLES) dtCount++;

            const double avgDt = (dtCount > 0 ? dtSum / dtCount : 0.0167);
            double fpsCalc = (avgDt > 1e-6 ? 1.0 / avgDt : (double)fpsDisplay);
            if (fpsCalc > 240.0) fpsCalc = 240.0; // clip visual noise
            fpsDisplay = (int)(fpsCalc + 0.5);
        }
         
        // Input / exit
        canvas.checkInput();
        if (canvas.keyPressed(VK_ESCAPE)) break;

        // ============================== Save / Load ==========================
        // F5 = save; F9 = load. The save blob includes hero, NPC pool, time, kills, mode.
        if (canvas.keyPressed(VK_F5))
        {
            bool ok = SaveLoad::SaveToFile("save.dat",
                hero, npcSys,
                totalTime, totalKills,
                (gMode == GameMode::Infinite));
            printf(ok ? "[SAVE] save.dat written\n" : "[SAVE] failed to write save.dat\n");
        }
        if (canvas.keyPressed(VK_F9))
        {
            bool  infinite = (gMode == GameMode::Infinite);
            float t = totalTime;
            int   kills = totalKills;

            bool ok = SaveLoad::LoadFromFile("save.dat",
                hero, npcSys,
                t, kills, infinite);
            if (ok)
            {
                totalTime = t;
                totalKills = kills;

                // Mode may change on load → propagate to map and systems
                gMode = (infinite ? GameMode::Infinite : GameMode::Fixed);
                map.setWrap(infinite);
                npcSys.setInfinite(infinite);

                // Recenter camera on the restored hero position
                camX = hero.getX() - (canvas.getWidth() * 0.5f) + hero.getW() * 0.5f;
                camY = hero.getY() - (canvas.getHeight() * 0.5f) + hero.getH() * 0.5f;

                printf("[LOAD] save.dat loaded (mode=%s)\n", infinite ? "Infinite" : "Fixed");
            }
            else
            {
                printf("[LOAD] failed to load save.dat\n");
            }
        }

        // =============================== Update ==============================
        // Player movement/animation then combat (kept separate for clarity)
        hero.update(canvas, dt);
        hero.updateAttack(dt, npcSys);
        hero.updateAOE(dt, npcSys, canvas);

        // Viewport size used for spawn ring calculation
        int viewW = (int)canvas.getWidth();
        int viewH = (int)canvas.getHeight();

        // Spawn cadence accelerates over time; spawn just outside camera
        npcSys.trySpawn(dt, (float)camX, (float)camY, viewW, viewH, hero.getX(), hero.getY());

        // NPC brains step toward hero center; pass the hero's hitbox center as target
        npcSys.updateAll(
            dt,
            hero.getHitboxX() + hero.getHitboxW() * 0.5f,
            hero.getHitboxY() + hero.getHitboxH() * 0.5f
        );

        // Enemy bullets: motion + hero bullet-hit resolution
        npcSys.updateBullets(dt);
        npcSys.checkPlayerCollision(hero);
        npcSys.checkHeroHit(hero);

        // Hero bullets: motion then hit resolution against NPCs
        npcSys.updateHeroBullets(dt);
        int killsThisFrame = npcSys.checkNPCHit();

        // Stats
        totalTime += dt;
        totalKills += killsThisFrame;

        // Lightweight instantaneous FPS smoothing for an alternate readout
        float fpsInstant = (dt > 1e-6f ? 1.f / dt : 0.f);
        fpsSmoothed = 0.90f * fpsSmoothed + 0.10f * fpsInstant;

        perf.tick(dt, totalTime, fpsSmoothed, npcSys);

        // Pickups: spawn around camera (infinite) or on base map (fixed) and apply on touch
        pickups.trySpawn(dt, camX, camY);
        pickups.updateAndCollide(hero);

        // World clamp for fixed mode; infinite uses wrapping in TileMap draw calls
        if (gMode == GameMode::Fixed) {
            float maxHeroX = (float)map.getPixelWidth() - hero.getW();
            float maxHeroY = (float)map.getPixelHeight() - hero.getH();
            if (maxHeroX < 0) maxHeroX = 0;
            if (maxHeroY < 0) maxHeroY = 0;
            hero.clampPosition(0.0f, 0.0f, maxHeroX, maxHeroY);
        }

        // Camera follows hero (centered on sprite); slight half-frame bias keeps it centered
        camX = hero.getX() - (canvas.getWidth() * 0.5f) + hero.getW() * 0.5f;
        camY = hero.getY() - (canvas.getHeight() * 0.5f) + hero.getH() * 0.5f;

        // Clamp camera to map for fixed mode; infinite mode leaves camera free
        if (gMode == GameMode::Fixed) {
            float maxX = (float)map.getPixelWidth() - canvas.getWidth();
            float maxY = (float)map.getPixelHeight() - canvas.getHeight();
            if (camX < 0) camX = 0; if (camY < 0) camY = 0;
            if (camX > maxX) camX = maxX; if (camY > maxY) camY = maxY;
        }

        // =============================== Render ===============================
        canvas.clear();

        map.draw(canvas, camX, camY);                       // world tiles (wrapping if enabled)
        npcSys.drawAll(canvas, (float)camX, (float)camY);   // enemies
        npcSys.drawBullets(canvas, (float)camX, (float)camY);
        npcSys.drawHeroBullets(canvas, (float)camX, (float)camY);
        pickups.draw(canvas, (float)camX, (float)camY);
        hero.draw(canvas, camX, camY);                      // hero on top

        // HUD (time left clamps at 0 for neatness)
        int remain = (int)((120.f - totalTime) + 0.999f);
        if (remain < 0) remain = 0;
        drawHUD(canvas, remain, fpsDisplay, totalKills);

        // Present back buffer
        canvas.present();

        // Two-minute session ends the loop; show stats and exit cleanly
        if (totalTime >= 120.f) {
            canvas.clear();
            canvas.present();

            printf("\n===== GAME OVER (2 minutes) =====\n");
            printf("Time:   %.1f s\n", totalTime);
            printf("Kills:  %d\n", totalKills);
            printf("=================================\n");
            break;
        }
    }

    // Post-loop: in-session endcard with input wait, then return.
    if (totalTime >= 120.f)
    {
        int fpsInt = fpsDisplay; // could also use (int)(fpsSmoothed + 0.5f)
        showGameOverScreen(canvas, totalKills, fpsInt);
        perf.close();
        return 0;
    }
}
