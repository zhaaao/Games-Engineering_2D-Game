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
using namespace GamesEngineeringBase;
using namespace std;


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
    NPCSystem npcSys;
    npcSys.init(&map);
    // ★ 让 NPC 系统知道地图尺寸


    // 起点放在地图中心（防止相机起跳）
    float startX = (float)(map.getPixelWidth() / 2 - hero.getW() / 2);
    float startY = (float)(map.getPixelHeight() / 2 - hero.getH() / 2);
    hero.setPosition(startX, startY);


    // 2) 定时器用于帧间隔（dt）
    Timer timer; // 调用 dt() 会返回并重置起点，建议每帧只调用一次 :contentReference[oaicite:4]{index=4}

    // 3) 相机位置（以像素为单位），WASD 移动
    float camX = 0.0f;
    float camY = 0.0f;
    const float camSpeed = 220.0f; // px/s

    // 4) 瓦片尺寸（与作业要求保持一致的 32×32）
    const int TILE = 32;

    // 5) 主循环
    while (true)
    {
        // -- 输入与时间步长
        float dt = timer.dt();         // 每帧的秒数（已重置）:contentReference[oaicite:5]{index=5}
        canvas.checkInput();           // 处理消息/键鼠状态 :contentReference[oaicite:6]{index=6}
        if (canvas.keyPressed(VK_ESCAPE)) break; // 退出键 :contentReference[oaicite:7]{index=7}

        // --- 玩家更新（移动 + 动画）---
        hero.update(canvas, dt);
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
        npcSys.checkBulletHitHero(hero);      // ★ 新增：子弹命中英雄
        // --- 边界限制：保证角色不离开地图 ---
        float maxHeroX = (float)map.getPixelWidth() - hero.getW();
        float maxHeroY = (float)map.getPixelHeight() - hero.getH();
        if (maxHeroX < 0) maxHeroX = 0;
        if (maxHeroY < 0) maxHeroY = 0;
        hero.clampPosition(0.0f, 0.0f, maxHeroX, maxHeroY);
        // --- 计算相机：始终把玩家放在屏幕中央（可偏移半帧使观感更居中）---
        camX = hero.getX() - (canvas.getWidth() * 0.5f) + hero.getW() * 0.5f;
        camY = hero.getY() - (canvas.getHeight() * 0.5f) + hero.getH() * 0.5f;

        // --- 夹紧相机不出地图 ---
        float maxX = (float)map.getPixelWidth() - canvas.getWidth();
        float maxY = (float)map.getPixelHeight() - canvas.getHeight();
        if (camX < 0) camX = 0; if (camY < 0) camY = 0;
        if (camX > maxX) camX = maxX; if (camY > maxY) camY = maxY;

        // -- 渲染
        canvas.clear(); // 先清屏（内部把 back buffer 置黑）:contentReference[oaicite:8]{index=8}

        map.draw(canvas, camX, camY);
        // …… 你已有的 drawTileMap(camX, camY) 等
        npcSys.drawAll(canvas, (float)camX, (float)camY);
        npcSys.drawBullets(canvas, (float)camX, (float)camY); // ★ 新增：绘制敌方子弹

        // …… 再画英雄 / UI

        hero.draw(canvas, camX, camY);         // 玩家立刻叠在上面
        // 显示到屏幕（把 back buffer 推给 GPU 并 Present）
        canvas.present(); // UpdateSubresource + Draw + SwapChain::Present 已封装好 :contentReference[oaicite:9]{index=9}
    }

    return 0;
}
