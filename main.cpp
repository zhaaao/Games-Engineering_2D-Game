#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
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

// main.cpp 顶部（include 之后）
enum class GameMode { Fixed, Infinite };
GameMode gMode = GameMode::Fixed;


// ✅ 辅助函数：id→颜色映射
static void idToColor(int id, unsigned char& r, unsigned char& g, unsigned char& b)
{
    switch (id) {
    case 0:  r = 30;  g = 30;  b = 30;  break;
    case 4:  r = 60;  g = 180; b = 75;  break;
    case 5:  r = 80;  g = 160; b = 90;  break;
    case 6:  r = 50;  g = 140; b = 70;  break;
    case 7:  r = 120; g = 110; b = 80;  break;
    case 8:  r = 150; g = 150; b = 160; break;
    case 9:  r = 120; g = 120; b = 130; break;
    case 10: r = 200; g = 180; b = 120; break;
    case 11: r = 180; g = 160; b = 110; break;
    case 12: r = 100; g = 80;  b = 60;  break;
    default: r = (id * 40) % 200 + 40; g = (id * 53) % 200 + 40; b = (id * 71) % 200 + 40; break;
    }
}

// ====== 简易像素/HUD工具（5x7字库；仅含需要的字符） ======
static inline void putPix(GamesEngineeringBase::Window& w, int x, int y,
    unsigned char r, unsigned char g, unsigned char b)
{
    const int W = (int)w.getWidth(), H = (int)w.getHeight();
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
    w.draw(y * W + x, r, g, b);
}

static void fillRectUI(GamesEngineeringBase::Window& w, int x0, int y0, int wdt, int hgt,
    unsigned char r, unsigned char g, unsigned char b)
{
    const int W = (int)w.getWidth(), H = (int)w.getHeight();
    int x1 = x0 + wdt, y1 = y0 + hgt;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; ++y) {
        int base = y * W;
        for (int x = x0; x < x1; ++x) {
            w.draw(base + x, r, g, b);
        }
    }
}

// --- 5x7 字形：每个字形 7 行，每行低 5 位为像素（1=点亮）
struct Glyph5x7 { unsigned char row[7]; };

static const Glyph5x7* getGlyph5x7(char c)
{
    // 数字 0-9
    static const Glyph5x7 G0 = { {0x1E,0x11,0x13,0x15,0x19,0x11,0x1E} }; // 0
    static const Glyph5x7 G1 = { {0x04,0x0C,0x14,0x04,0x04,0x04,0x1F} }; // 1
    static const Glyph5x7 G2 = { {0x1E,0x01,0x01,0x1E,0x10,0x10,0x1F} }; // 2
    static const Glyph5x7 G3 = { {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E} }; // 3
    static const Glyph5x7 G4 = { {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02} }; // 4
    static const Glyph5x7 G5 = { {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E} }; // 5
    static const Glyph5x7 G6 = { {0x0E,0x10,0x10,0x1E,0x11,0x11,0x1E} }; // 6
    static const Glyph5x7 G7 = { {0x1F,0x01,0x02,0x04,0x08,0x08,0x08} }; // 7
    static const Glyph5x7 G8 = { {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E} }; // 8
    static const Glyph5x7 G9 = { {0x1E,0x11,0x11,0x1E,0x01,0x01,0x0E} }; // 9

    // 大写：T I M E F P S K L O R
    static const Glyph5x7 GT = { {0x1F,0x04,0x04,0x04,0x04,0x04,0x04} };
    static const Glyph5x7 GI = { {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F} };
    static const Glyph5x7 GM = { {0x11,0x1B,0x15,0x11,0x11,0x11,0x11} };
    static const Glyph5x7 GE = { {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F} };
    static const Glyph5x7 GF = { {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10} };
    static const Glyph5x7 GP = { {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10} };
    static const Glyph5x7 GS = { {0x0F,0x10,0x10,0x1E,0x01,0x01,0x1E} };
    static const Glyph5x7 GK = { {0x11,0x12,0x14,0x18,0x14,0x12,0x11} };
    static const Glyph5x7 GL = { {0x10,0x10,0x10,0x10,0x10,0x10,0x1F} };
    static const Glyph5x7 GO = { {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E} };
    static const Glyph5x7 GR = { {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11} };

    // 标点：冒号 :
    static const Glyph5x7 GCOL = { {0x00,0x04,0x00,0x00,0x00,0x04,0x00} };

    if (c >= '0' && c <= '9') {
        static const Glyph5x7* DIG[10] = { &G0,&G1,&G2,&G3,&G4,&G5,&G6,&G7,&G8,&G9 };
        return DIG[c - '0'];
    }
    switch (c) {
    case 'T': return &GT; case 'I': return &GI; case 'M': return &GM; case 'E': return &GE;
    case 'F': return &GF; case 'P': return &GP; case 'S': return &GS;
    case 'K': return &GK; case 'L': return &GL; case 'O': return &GO; case 'R': return &GR;
    case ':': return &GCOL;
    }
    return nullptr;
}

static void drawChar5x7(GamesEngineeringBase::Window& w, int x, int y,
    char c, unsigned char r, unsigned char g, unsigned char b, int scale = 2)
{
    const Glyph5x7* gph = getGlyph5x7(c);
    if (!gph) return;
    for (int ry = 0; ry < 7; ++ry) {
        unsigned char mask = gph->row[ry];
        for (int rx = 0; rx < 5; ++rx) {
            if (mask & (1 << (4 - rx))) {
                // 放大绘制
                for (int yy = 0; yy < scale; ++yy)
                    for (int xx = 0; xx < scale; ++xx)
                        putPix(w, x + rx * scale + xx, y + ry * scale + yy, r, g, b);
            }
        }
    }
}

static void drawText5x7(GamesEngineeringBase::Window& w, int x, int y,
    const char* s, unsigned char r, unsigned char g, unsigned char b, int scale = 2, int spacing = 1)
{
    int cx = x;
    for (int i = 0; s[i]; ++i) {
        drawChar5x7(w, cx, y, s[i], r, g, b, scale);
        cx += 5 * scale + spacing;
    }
}

static void drawNumber(GamesEngineeringBase::Window& w, int x, int y, int v,
    unsigned char r, unsigned char g, unsigned char b, int scale = 2)
{
    // 简单非 STL：把数字拆成逆序，再倒着画
    if (v == 0) { drawChar5x7(w, x, y, '0', r, g, b, scale); return; }
    int buf[12]; int n = 0;
    int t = (v < 0 ? -v : v);
    while (t > 0 && n < 12) { buf[n++] = t % 10; t /= 10; }
    if (v < 0) { drawChar5x7(w, x, y, '-', r, g, b, scale); x += 5 * scale + 1; }
    for (int i = n - 1; i >= 0; --i) {
        drawChar5x7(w, x, y, (char)('0' + buf[i]), r, g, b, scale);
        x += 5 * scale + 1;
    }
}

// 半透明感的棋盘点阵（伪透明）：每隔一个像素画一个点
static void fillRectStipple(GamesEngineeringBase::Window& w, int x0, int y0, int wdt, int hgt,
    unsigned char r, unsigned char g, unsigned char b, int pattern = 0)
{
    const int W = (int)w.getWidth(), H = (int)w.getHeight();
    int x1 = x0 + wdt, y1 = y0 + hgt;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;

    for (int y = y0; y < y1; ++y) {
        int base = y * W;
        for (int x = x0; x < x1; ++x) {
            // pattern 0: (x+y) 偶数点；pattern 1: 2x2 更稀疏
            bool paint = (pattern == 0) ? (((x ^ y) & 1) == 0)
                : (((x & 1) == 0) && ((y & 1) == 0));
            if (paint) w.draw(base + x, r, g, b);
        }
    }
}

// 一个简单的 HUD 面板，显示：TIME、FPS、KILLS
static void drawHUD(GamesEngineeringBase::Window& w,
    int remainSec, int fpsInt, int kills)
{
    // 背板（半透明感：用深灰+留白边）
    const int panelX = 10, panelY = 10, panelW = 220, panelH = 80;
    fillRectStipple(w, panelX, panelY, panelW, panelH, 18, 18, 18, /*pattern=*/0);
    // 如果想更淡一点，用 pattern=1
    // fillRectStipple(w, panelX, panelY, panelW, panelH, 18,18,18, 1);

    // 标题行
    drawText5x7(w, panelX + 8, panelY + 8, "TIME:", 255, 240, 180, 2);
    drawNumber(w, panelX + 92, panelY + 8, remainSec, 255, 240, 180, 2);

    drawText5x7(w, panelX + 8, panelY + 32, "FPS:", 180, 220, 255, 2);
    drawNumber(w, panelX + 72, panelY + 32, fpsInt, 180, 220, 255, 2);

    drawText5x7(w, panelX + 8, panelY + 56, "KILLS:", 200, 255, 200, 2);
    drawNumber(w, panelX + 96, panelY + 56, kills, 200, 255, 200, 2);
}


// === Game Over 屏幕：显示统计并等待按键退出 ===
static void showGameOverScreen(GamesEngineeringBase::Window& w,
    int kills, int fpsInt)
{
    const int W = (int)w.getWidth();
    const int H = (int)w.getHeight();

    // 半透明遮罩（棋盘点阵伪透明）
    fillRectStipple(w, 0, 0, W, H, 0, 0, 0, 1);

    // 中央面板
    const int panelW = 360, panelH = 200;
    const int panelX = (W - panelW) / 2;
    const int panelY = (H - panelH) / 2;
    fillRectUI(w, panelX, panelY, panelW, panelH, 24, 24, 24);

    // 标题
    drawText5x7(w, panelX + 24, panelY + 20, "== GAME OVER ==", 255, 220, 180, 3);

    // 统计
    drawText5x7(w, panelX + 24, panelY + 80, "KILLS:", 200, 255, 200, 2);
    drawNumber(w, panelX + 130, panelY + 80, kills, 200, 255, 200, 2);

    drawText5x7(w, panelX + 24, panelY + 110, "FPS:", 180, 220, 255, 2);
    drawNumber(w, panelX + 100, panelY + 110, fpsInt, 180, 220, 255, 2);

    // 提示
    drawText5x7(w, panelX + 24, panelY + 150, "Press ENTER / Q / ESC to quit",
        255, 240, 180, 2);

    // 先把这一帧显示出来
    w.present();

    // 等待按键（窗口保持显示）
    for (;;)
    {
        w.checkInput();
        if (w.keyPressed(VK_RETURN) || w.keyPressed(VK_ESCAPE) || w.keyPressed('Q'))
            break;
        // 轻微让出CPU（可要可不要）
        Sleep(10);

        // 如果你想让标题有轻微闪烁效果，也可以每帧重画：
        // （简单起见，这里不做动画；保持静止一帧画面）
    }
}


// === FPS 统计：滑动窗口（更稳定）===
static const int FPS_SAMPLES = 120; // 约1~2秒窗口
float  dtBuf[FPS_SAMPLES];
int    dtIdx = 0;
int    dtCount = 0;
double dtSum = 0.0;
int    fpsDisplay = 60;
// —— 立即初始化缓冲，避免未定义值影响统计 —— 
void initFpsBuffer() {
    for (int i = 0; i < FPS_SAMPLES; ++i) dtBuf[i] = 0.0f;
    dtIdx = 0; dtCount = 0; dtSum = 0.0; fpsDisplay = 60;
}

int main()
{
    // 1) 创建窗口
    Window canvas;
    canvas.create(960, 540, "prototype"); // Window::create :contentReference[oaicite:3]{index=3}
    TileMap map;
    if (!map.load("Resources/tiles.txt")) {
        printf("❌ Failed to load tiles.txt\n");
        return 1;
    }
    map.setImageFolder("Resources/"); // 假设 0.png、1.png… 放在 .\tiles\ 目录下
    // 3) 载入主角精灵（请改为你的真实帧尺寸）
   //    约定：前四行表示 下/右/左/上；每行四列做行走动画
   
    // === 选择游戏模式 ===
    printf("Select mode: [1] Fixed world   [2] Infinite (wrapping) world\n");
    printf("Your choice: ");
    int m = 1;
    std::cin >> m;
    gMode = (m == 2 ? GameMode::Infinite : GameMode::Fixed);

    // === 设置地图是否循环 ===
    map.setWrap(gMode == GameMode::Infinite);

    SpriteSheet heroSheet;
    if (!heroSheet.load("Resources/Abigail.png", /*frameW*/16, /*frameH*/32, /*rows*/13, /*cols*/4)) {
        printf("❌ Failed to load Abigail sprite\n");
        return 1;
    }

    // 4) 创建玩家并设置初始位置（例：地图中央）
    Player hero;
    hero.attachSprite(&heroSheet);
    hero.bindMap(&map);   // ★ 绑定地图，启用地形碰撞
    hero.setSpeed(150.f);

    // …… 你的地图加载、英雄初始化、bindMap(&map) 之后
    EnemyManager npcSys;
    npcSys.init(&map);
    npcSys.setInfinite(gMode == GameMode::Infinite);
    // ★ 让 NPC 系统知道地图尺寸
    // 
    // === 增益果实系统 ===
    PickupSystem pickups;
    pickups.init(&map);
    pickups.setInfinite(gMode == GameMode::Infinite);

    // 起点放在地图中心（防止相机起跳）
    float startX = (float)(map.getPixelWidth() / 2 - hero.getW() / 2);
    float startY = (float)(map.getPixelHeight() / 2 - hero.getH() / 2);
    hero.setPosition(startX, startY);


    // 2) 定时器用于帧间隔（dt）
    Timer timer; // 调用 dt() 会返回并重置起点，建议每帧只调用一次 :contentReference[oaicite:4]{index=4}

    // === HUD统计 ===
    float totalTime = 0.f;         // 累计时间
    int   totalKills = 0;          // 累计击杀
    float fpsSmoothed = 60.f;      // 平滑 FPS（EMA）
    


    // 3) 相机位置（以像素为单位），WASD 移动
    float camX = 0.0f;
    float camY = 0.0f;
    const float camSpeed = 220.0f; // px/s

    // 4) 瓦片尺寸（与作业要求保持一致的 32×32）
    const int TILE = 32;


    initFpsBuffer();



    // 5) 主循环
    while (true)
    {
        // -- 输入与时间步长
        float dt = timer.dt();         // 每帧的秒数（已重置）:contentReference[oaicite:5]{index=5}
        // —— FPS 统计（滑动平均 + 简单去极值）——
        {
            float dtClamp = dt;
            // 过滤异常尖峰（窗口切换等），>100ms 当 100ms
            if (dtClamp > 0.100f) dtClamp = 0.100f;
            // 过滤零或负值（极少见），给个最小正数
            if (dtClamp <= 0.000001f) dtClamp = 0.000001f;

            dtSum -= dtBuf[dtIdx];
            dtBuf[dtIdx] = dtClamp;
            dtSum += dtClamp;

            dtIdx = (dtIdx + 1) % FPS_SAMPLES;
            if (dtCount < FPS_SAMPLES) dtCount++;

            double avgDt = (dtCount > 0 ? dtSum / dtCount : 0.0167);
            // 保护：避免除零
            double fpsCalc = (avgDt > 1e-6 ? 1.0 / avgDt : (double)fpsDisplay);
            // 限个上限，防止偶发飙高显示
            if (fpsCalc > 240.0) fpsCalc = 240.0;
            fpsDisplay = (int)(fpsCalc + 0.5);
        }

        canvas.checkInput();           // 处理消息/键鼠状态 :contentReference[oaicite:6]{index=6}
        if (canvas.keyPressed(VK_ESCAPE)) break; // 退出键 :contentReference[oaicite:7]{index=7}


        // === 存档/读档按键 ===
// F5 保存；F9 读取
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
            bool infinite = (gMode == GameMode::Infinite);
            float t = totalTime;
            int   kills = totalKills;

            bool ok = SaveLoad::LoadFromFile("save.dat",
                hero, npcSys,
                t, kills, infinite);
            if (ok)
            {
                totalTime = t;
                totalKills = kills;

                // 切换模式（影响TileMap wrap与系统标志）
                gMode = (infinite ? GameMode::Infinite : GameMode::Fixed);
                map.setWrap(infinite);
                npcSys.setInfinite(infinite);

                // 重新定位相机以跟随玩家
                camX = hero.getX() - (canvas.getWidth() * 0.5f) + hero.getW() * 0.5f;
                camY = hero.getY() - (canvas.getHeight() * 0.5f) + hero.getH() * 0.5f;

                printf("[LOAD] save.dat loaded (mode=%s)\n", infinite ? "Infinite" : "Fixed");
            }
            else
            {
                printf("[LOAD] failed to load save.dat\n");
            }
        }


        // --- 玩家更新（移动 + 动画）---
        hero.update(canvas, dt);
        // 1) 英雄自动攻击（放在 hero.update(...) 之后）
        hero.updateAttack(dt, npcSys);
        hero.updateAOE(dt, npcSys, canvas);
        // 假设你已有这些变量：camX, camY 为相机左上；win 是 Window；hero 有 getX/getY/getW/getH
// 若你的命名不同，按实际变量替换即可
        int viewW = (int)canvas.getWidth();
        int viewH = (int)canvas.getHeight();

        // —— 先尝试按“随时间加快”的节奏在相机外生成 NPC —— 
        npcSys.trySpawn(dt, (float)camX, (float)camY, viewW, viewH, hero.getX(), hero.getY());

        // —— 再更新所有 NPC（追踪玩家等）——
        npcSys.updateAll(
            dt,
            hero.getHitboxX() + hero.getHitboxW() * 0.5f,   // 传中心X
            hero.getHitboxY() + hero.getHitboxH() * 0.5f    // 传中心Y
        );

        npcSys.updateBullets(dt);             // ★ 新增：更新敌方子弹
        npcSys.checkPlayerCollision(hero);
        npcSys.checkHeroHit(hero);      // ★ 新增：子弹命中英雄
        // 2) 英雄子弹更新（放在 npc 更新之后、绘制之前）
        npcSys.updateHeroBullets(dt);
        // 3) 命中结算（可用于加分/统计；即使不用分数也要调用以移除命中的 NPC）
        int killsThisFrame = npcSys.checkNPCHit();
        // 统计
        totalTime += dt;
        totalKills += killsThisFrame;

        // FPS 平滑
        float fpsInstant = (dt > 1e-6f ? 1.f / dt : 0.f);
        fpsSmoothed = 0.90f * fpsSmoothed + 0.10f * fpsInstant;


        // —— 刷新果实 & 碰撞拾取 —— 
        pickups.trySpawn(dt, camX, camY);
        pickups.updateAndCollide(hero);


        // --- 边界限制：保证角色不离开地图 ---
        if (gMode == GameMode::Fixed) {
            float maxHeroX = (float)map.getPixelWidth() - hero.getW();
            float maxHeroY = (float)map.getPixelHeight() - hero.getH();
            if (maxHeroX < 0) maxHeroX = 0;
            if (maxHeroY < 0) maxHeroY = 0;
            hero.clampPosition(0.0f, 0.0f, maxHeroX, maxHeroY);
        }
        // --- 计算相机：始终把玩家放在屏幕中央（可偏移半帧使观感更居中）---
        camX = hero.getX() - (canvas.getWidth() * 0.5f) + hero.getW() * 0.5f;
        camY = hero.getY() - (canvas.getHeight() * 0.5f) + hero.getH() * 0.5f;

        // --- 夹紧相机不出地图 ---
        if (gMode == GameMode::Fixed) {
            float maxX = (float)map.getPixelWidth() - canvas.getWidth();
            float maxY = (float)map.getPixelHeight() - canvas.getHeight();
            if (camX < 0) camX = 0; if (camY < 0) camY = 0;
            if (camX > maxX) camX = maxX; if (camY > maxY) camY = maxY;
        }
        // Infinite 模式不夹紧；渲染时 TileMap 会用 get() 包装成循环贴图


        // -- 渲染
        canvas.clear(); // 先清屏（内部把 back buffer 置黑）:contentReference[oaicite:8]{index=8}

        map.draw(canvas, camX, camY);
        // …… 你已有的 drawTileMap(camX, camY) 等
        npcSys.drawAll(canvas, (float)camX, (float)camY);
        npcSys.drawBullets(canvas, (float)camX, (float)camY); // ★ 新增：绘制敌方子弹
        npcSys.drawHeroBullets(canvas, (float)camX, (float)camY); // ★ 英雄弹

        pickups.draw(canvas, (float)camX, (float)camY);


        // …… 再画英雄 / UI

        hero.draw(canvas, camX, camY);         // 玩家立刻叠在上面
        // === HUD 显示（窗口内）===
        int remain = (int)((120.f - totalTime) + 0.999f); if (remain < 0) remain = 0;
        drawHUD(canvas, remain, fpsDisplay, totalKills);




        // 显示到屏幕（把 back buffer 推给 GPU 并 Present）
        canvas.present(); // UpdateSubresource + Draw + SwapChain::Present 已封装好 :contentReference[oaicite:9]{index=9}

        // === 两分钟后结束 ===
        if (totalTime >= 120.f) {
            // 可选：清一下屏作为“结束画面”的留白
            canvas.clear();
            canvas.present();

            // 作业允许在结束时显示成绩（用控制台输出最稳妥）
            printf("\n===== GAME OVER (2 minutes) =====\n");
            printf("Time:   %.1f s\n", totalTime);
            printf("Kills:  %d\n", totalKills);
            printf("=================================\n");

            break; // 退出主循环
        }

    }

    if (totalTime >= 120.f)
    {
        // 计算一次最终 fpsInt（你已有 fpsSmoothed 或 fpsDisplay 就用它）
        int fpsInt = fpsDisplay;  // 或者 (int)(fpsSmoothed + 0.5f);

        // 窗口内显示结束界面，等待按键
        showGameOverScreen(canvas, totalKills, fpsInt);

        // 这时用户已按键确认，安全退出
        return 0;
    }

}
