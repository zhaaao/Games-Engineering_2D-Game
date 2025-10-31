#include "NPCSystem.h"
#include "Player.h"      // 需要完整 Player 定义
#include <cmath>

void HeroBullet::spawn(float sx, float sy, float dirx, float diry, float speed, float ttl)
{
    alive = true;
    x = sx; y = sy;
    vx = dirx * speed; vy = diry * speed;
    life = ttl;
    w = 6; h = 6;

    // ✅ 关键：复位为“普通直线子弹”的默认状态（青色、1伤害、非AOE）
    r = 40; g = 200; b = 255;
    damage = 1;
    isAOE = false;
}
void HeroBullet::update(float dt) {
    if (!alive) return;
    x += vx * dt; y += vy * dt;
    life -= dt;
    if (life <= 0.f) alive = false;
}
int NPCSystem::allocIndex()
{
    for (int i = 0; i < MAX; ++i) if (!npcs[i].isAlive()) return i;
    return -1;
}
void NPCSystem::mapPixelSize(int& outW, int& outH)
{
    if (!map) { outW = outH = 0; return; }
    outW = map->getWidth() * map->getTileW();   // 你已有这些接口: getWidth/Height, getTileW/H
    outH = map->getHeight() * map->getTileH();   // :contentReference[oaicite:0]{index=0}
}
void NPCSystem::spawnOne(float camX, float camY, int viewW, int viewH, float px, float py)
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

    // —— 4种类型的简单概率分布 —— 
    // 0: 追踪（60%）  1: 炮台（20%）  2: 冲锋（10%）  3: 坦克（10%）
    float r = frand01();
    unsigned char type = 0;
    if (r < 0.60f) type = 0;
    else if (r < 0.80f) type = 1;
    else if (r < 0.90f) type = 2;
    else type = 3;

    // 速度/体型/HP 参数
    float spd = 0.f;
    int   hp = 3;
    int   w = 24, h = 24;

    switch (type)
    {
    case 0: // 追踪：中速、HP中等
        spd = 60.f;  hp = 3;  w = h = 24; break;
    case 1: // 炮台：静止、HP偏高
        spd = 0.f;   hp = 4;  w = h = 24; break;
    case 2: // 轻型冲锋：高速、脆皮、小体型
        spd = 110.f; hp = 1;  w = h = 20; break;
    case 3: // 重装坦克：低速、很肉、大体型
        spd = 40.f;  hp = 6;  w = h = 28; break;
    }

    // 初始化朝向对准玩家
    npcs[idx].initSpawn(sx, sy, type, spd, px, py);

    // 通过 friend 访问，覆盖独有属性（体型/生命）
    npcs[idx].w = w;
    npcs[idx].h = h;
    npcs[idx].hp = hp;

    // 炮台冷却初始化；其他类型设置极大值避免误射
    if (type == 1) {
        npcs[idx].setFireCD(0.2f + 0.2f * frand01()); // 0.2~0.4s
    }
    else {
        npcs[idx].setFireCD(999.f);
    }
}
int NPCSystem::allocBullet()
{
    for (int i = 0; i < BULLET_MAX; ++i)
        if (!bullets[i].alive) return i;
    return -1;
}
void NPCSystem::init(TileMap* m)
{
    map = m; elapsed = 0.f; spawnTimer = 0.f;
    for (int i = 0; i < MAX; ++i) npcs[i].kill();
    srand(12345); // 如已有全局 srand，可移除
    // —— 子弹池清空 & 世界像素尺寸缓存 —— 
    for (int i = 0; i < BULLET_MAX; ++i) bullets[i].alive = false;
    for (int i = 0; i < HERO_BULLET_MAX; ++i) hbullets[i].alive = false;
    mapPixelSize(worldWpx, worldHpx);

}
void NPCSystem::trySpawn(float dt, float camX, float camY, int viewW, int viewH, float px, float py)
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
void NPCSystem::updateAll(float dt, float px, float py)
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
void NPCSystem::drawAll(Window& win, float camX, float camY)
{
    for (int i = 0; i < MAX; ++i) if (npcs[i].isAlive())
        npcs[i].draw(win, camX, camY);
}
void NPCSystem::checkPlayerCollision(Player& hero)
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
bool NPCSystem::findNearestAlive(float px, float py, float& tx, float& ty) const
{
    float bestD2 = 1e30f; bool found = false;
    for (int i = 0; i < MAX; ++i) {
        const NPC& n = npcs[i];
        if (!n.alive) continue;
        float cx = n.getHitboxX() + n.getHitboxW() * 0.5f;
        float cy = n.getHitboxY() + n.getHitboxH() * 0.5f;
        float dx = cx - px, dy = cy - py;
        float d2 = dx * dx + dy * dy;
        if (d2 < bestD2) { bestD2 = d2; tx = cx; ty = cy; found = true; }
    }
    return found;
}
void NPCSystem::updateBullets(float dt)
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
void NPCSystem::checkBulletHitHero(Player& hero)
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
void NPCSystem::drawBullets(Window& win, float camX, float camY)
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
void NPCSystem::spawnHeroBullet(float sx, float sy, float dirx, float diry, float speed, float ttl)
{
    // 找空位
    for (int i = 0; i < HERO_BULLET_MAX; ++i) {
        if (!hbullets[i].alive) {
            hbullets[i].spawn(sx, sy, dirx, diry, speed, ttl);
            return;
        }
    }
    // 满了就丢弃（也可覆盖最老的）
}
void NPCSystem::updateHeroBullets(float dt)
{
    for (int i = 0; i < HERO_BULLET_MAX; ++i) {
        if (hbullets[i].alive) hbullets[i].update(dt);
    }
}
void NPCSystem::drawHeroBullets(Window& win, float camX, float camY)
{
    const int W = (int)win.getWidth();
    const int H = (int)win.getHeight();
    for (int i = 0; i < HERO_BULLET_MAX; ++i) {
        const HeroBullet& b = hbullets[i];
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
                win.draw(x, y, b.r, b.g, b.b);  // ✅ 用子弹自身颜色
    }
}
int NPCSystem::checkHeroBulletsHitNPC()
{
    int kills = 0;
    for (int bi = 0; bi < HERO_BULLET_MAX; ++bi) {
        HeroBullet& b = hbullets[bi];
        if (!b.alive) continue;

        for (int i = 0; i < MAX; ++i) {
            NPC& n = npcs[i];
            if (!n.alive) continue;

            if (aabbOverlap(b.x, b.y, b.w, b.h, n.x, n.y, n.w, n.h)) {
                b.alive = false;
                if (n.hp > 0) {
                    n.hp -= b.damage;                // ✅ 使用每颗子弹的伤害
                    if (n.hp <= 0) { n.kill(); ++kills; }
                }
                else {
                    n.kill(); ++kills;
                }
                break;
            }
        }
    }
    return kills;
}
// NPCSystem.cpp
int NPCSystem::aoeStrikeTopN(int N, int damage, float heroCx, float heroCy)
{
    if (N <= 0 || damage <= 0) return 0;

    // 记录是否已被挑中，避免重复
    bool picked[MAX];
    for (int i = 0; i < MAX; ++i) picked[i] = false;

    int fired = 0;

    for (int k = 0; k < N; ++k)
    {
        int bestIdx = -1;
        int bestHP = -1;

        // 扫描找“HP 最高且未被挑过且存活”的 NPC
        for (int i = 0; i < MAX; ++i)
        {
            if (!npcs[i].alive || picked[i]) continue;
            int hp = (npcs[i].hp > 0 ? npcs[i].hp : 1); // 防止未初始化 hp 时挑不到
            if (hp > bestHP) { bestHP = hp; bestIdx = i; }
        }

        if (bestIdx < 0) break;      // 没有可选目标，提前结束
        picked[bestIdx] = true;

        // 目标中心点
        float tx = npcs[bestIdx].x + npcs[bestIdx].w * 0.5f;
        float ty = npcs[bestIdx].y + npcs[bestIdx].h * 0.5f;

        // 发射一发 “紫色 AOE 子弹”（速度稍快、寿命略短）
        spawnAoeBullet(heroCx, heroCy, tx, ty, /*speed*/520.f, /*ttl*/0.9f, damage);
        ++fired;
    }

    return fired; // 返回本次实际发射的 AOE 弹数（也可用于加分）
}
// NPCSystem.cpp
void NPCSystem::spawnAoeBullet(float sx, float sy, float tx, float ty, float speed, float ttl, int dmg)
{
    // 找空位（与 spawnHeroBullet 相同的做法）
    for (int i = 0; i < HERO_BULLET_MAX; ++i) {
        if (!hbullets[i].alive) {
            // 方向
            float dx = tx - sx, dy = ty - sy;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) { dx = 1.f; dy = 0.f; len = 1.f; }
            dx /= len; dy /= len;

            hbullets[i].spawn(sx, sy, dx, dy, speed, ttl);
            // ✅ 染色：AOE 子弹为“洋红/紫色”
            hbullets[i].r = 255; hbullets[i].g = 50; hbullets[i].b = 200;
            hbullets[i].damage = (dmg > 0 ? dmg : 1);
            hbullets[i].isAOE = true;
            return;
        }
    }
    // 满了就丢弃
}




