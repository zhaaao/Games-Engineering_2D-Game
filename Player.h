#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"
#include "SpriteSheet.h"
#include "Animator.h"
#include "NPCSystem.h"
using namespace GamesEngineeringBase;


/******************************  玩家（Hero）类  *********************************
  - 维护世界坐标（float）
  - 处理 WASD 移动（对角归一，速度统一）
  - 根据移动方向选择动画行，移动时播放，静止时停在第0列
  - draw() 使用相机坐标 -> 屏幕坐标
*******************************************************************************/
class Player
{
public:
    enum Dir { Down = 0, Right = 1, Up = 2, Left = 3 };
    void bindMap(TileMap* m) { map = m; }
    // —— 可调碰撞盒（默认等于帧大小，先不改变手感）——
    int hitboxW = 0;
    int hitboxH = 0;

private:
    float x = 0.f, y = 0.f;     // 世界坐标（以像素为单位）
    float speed = 150.f;        // 像素/秒，按需要调
    Dir   dir = Down;           // 朝向（决定使用哪一行）
    SpriteSheet* sheet = nullptr;
    Animator anim;              // 列序动画器（0..3）
    TileMap* map = nullptr;
    // —— 击退速度（像素/秒）与剩余时长 —— 
    float kx = 0.f, ky = 0.f;
    float kTime = 0.f;
    float hitCooldown = 0.f;
    // —— 英雄武器冷却（线性自动射击最近NPC）——
    float shootCD = 0.0f;         // 射击冷却计时（秒）
    float shootInterval = 0.35f;  // 射速（秒/发），可被道具加成
    float aoeCD = 0.0f;          // AOE 当前冷却（秒）
    float aoeInterval = 1.0f;    // AOE 冷却时间（秒，作业要求“长冷却”）
    int   aoeN = 3;              // 一次命中的目标个数（按 HP 从高到低挑选）
    int   aoeDamage = 2;         // 每个目标承受的伤害
    // 如果没有的话，提供帧尺寸访问
    int getFrameW() const { return sheet ? sheet->getFrameW() : 32; }
    int getFrameH() const { return sheet ? sheet->getFrameH() : 32; }

public:
    // —— 构造：给 hitbox 一个安全默认值（sheet 还没绑定时用）——
    Player()
        : x(0.f), y(0.f), speed(150.f), dir(Down),
        sheet(nullptr), anim(), map(nullptr)
    {
        // 默认 32×32；若之后绑定了精灵表，会在 attachSprite 里用真实帧尺寸覆盖
        setHitbox(32, 32);
    }

    void attachSprite(SpriteSheet* s) {
        sheet = s;
        if (sheet && sheet->valid()) {
            // 如果之前是默认值（或你愿意总是跟随帧尺寸），就同步到帧尺寸
            // 建议：总是覆盖，避免资源更换后盒子没更新
            setHitbox(sheet->getFrameW(), sheet->getFrameH());
        }
    }

    void setPosition(float px, float py) { x = px; y = py; }
    void setSpeed(float s) { speed = s; }
     // 初始化后在构造/Init里，把它们设为帧宽高，或略小一点
    void setHitbox(int w, int h) { hitboxW = w; hitboxH = h; }

    // —— 中心对齐的碰撞盒 —— 
    float getHitboxX() const { 
        // 假设 x,y 是“精灵左上角”，把盒子居中到图像内部
        return x + (getFrameW() - hitboxW) * 0.5f; 
    }
    float getHitboxY() const { 
        return y + (getFrameH() - hitboxH) * 0.5f; 
    }
    int   getHitboxW() const { return hitboxW; }
    int   getHitboxH() const { return hitboxH; }
    float getX() const { return x; }
    float getY() const { return y; }
    int getW() const { return sheet ? sheet->getFrameW() : 0; }
    int getH() const { return sheet ? sheet->getFrameH() : 0; }
    // === 新增：给增益系统用的接口 ===
    float getShootInterval() const { return shootInterval; }
    void  setShootInterval(float v) { if (v > 0.f) shootInterval = v; }
    int   getAOEN() const { return aoeN; }
    float getAOEInterval() const { return aoeInterval; }

    void clampPosition(float minX, float minY, float maxX, float maxY)
    {
        if (x < minX) x = minX;
        if (y < minY) y = minY;
        if (x > maxX) x = maxX;
        if (y > maxY) y = maxY;
    }

    // 处理输入与更新动画
    void update(Window& input, float dt)
    {
        // 1) 读取输入，组成移动向量
        float vx = 0.f, vy = 0.f;
        if (input.keyPressed('W')) vy -= 1.f;
        if (input.keyPressed('S')) vy += 1.f;
        if (input.keyPressed('A')) vx -= 1.f;
        if (input.keyPressed('D')) vx += 1.f;

        // 2) 决定朝向（使用“最近一次非零输入”为面向）
        if (vy > 0.0f) dir = Down;
        else if (vy < 0.0f) dir = Up;
        if (vx > 0.0f) dir = Right;
        else if (vx < 0.0f) dir = Left;

        // 3) 归一化防止斜向加速
        float len = std::sqrt(vx * vx + vy * vy);
        if (len > 0.0001f) { vx /= len; vy /= len; }

        // === NEW: 本帧位移 = 输入位移 + 击退位移 ===
        float dx = vx * speed * dt;
        float dy = vy * speed * dt;

        // 击退位移叠加 + 衰减（需要类里有 kx,ky,kTime,hitCooldown 字段）
        if (kTime > 0.f) {
            dx += kx * dt;
            dy += ky * dt;
            kTime -= dt;

            // 指数衰减，系数可调（越大停得越快）
            float damp = std::exp(-6.f * dt);
            kx *= damp; ky *= damp;

            if (kTime <= 0.f) { kTime = 0.f; kx = 0.f; ky = 0.f; }
        }
        if (hitCooldown > 0.f) hitCooldown -= dt;

        // 4) 移动（用 Hitbox 做地形碰撞）
        if (!map) {
            x += dx;
            y += dy;;
        }
        else {
            const int tw = map->getTileW(), th = map->getTileH();

            // —— 准备帧尺寸与碰撞盒尺寸（要求 Player 提供下列接口）——
            const int fw = getFrameW();      // == sheet->getFrameW()
            const int fh = getFrameH();      // == sheet->getFrameH()
            const int hw = getHitboxW();     // 碰撞盒宽
            const int hh = getHitboxH();     // 碰撞盒高
            const float offX = (fw - hw) * 0.5f;   // 居中偏移
            const float offY = (fh - hh) * 0.5f;

            // —— 候选渲染位置 —— 
            float nx = x + dx;  // 包含击退后的总位移
            float ny = y + dy;

            // —— 换算到“碰撞盒左上角坐标” —— 
            float hx = nx + offX;
            float hy = ny + offY;

            // -------- 水平位移并处理阻挡（改：看 dx 的正负，不再看 vx） --------
            if (dx > 0.f) {
                int rightPix = (int)(hx + hw - 1);
                int tx = rightPix / tw;
                int topRow = (int)hy / th;
                int botRow = (int)(hy + hh - 1) / th;
                for (int ty = topRow; ty <= botRow; ++ty) {
                    if (map->isBlockedAt(tx, ty)) {
                        hx = (float)(tx * tw - hw);   // 卡在阻挡块左侧
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
                        hx = (float)((tx + 1) * tw); // 卡在阻挡块右侧
                        break;
                    }
                }
            }

            // -------- 垂直位移并处理阻挡（改：看 dy 的正负，不再看 vy） --------
            if (dy > 0.f) {
                int bottomPix = (int)(hy + hh - 1);
                int ty = bottomPix / th;
                int leftCol = (int)hx / tw;
                int rightCol = (int)(hx + hw - 1) / tw;
                for (int tx = leftCol; tx <= rightCol; ++tx) {
                    if (map->isBlockedAt(tx, ty)) {
                        hy = (float)(ty * th - hh);   // 卡在阻挡块上方
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
                        hy = (float)((topTile + 1) * th); // 卡在阻挡块下方
                        break;
                    }
                }
            }

            // —— 把“碰撞盒坐标”换回渲染坐标 —— 
            nx = hx - offX;
            ny = hy - offY;

            x = nx;
            y = ny;
        }

        // 5) 动画：移动就播放；静止就停在第0列
        if (len > 0.0f) anim.start(); else anim.stop();
        anim.update(dt);
    }


    // 根据相机绘制到屏幕
    void draw(Window& w, float camX, float camY)
    {
        if (!sheet || !sheet->valid()) return;
        int sx = (int)(x - camX);
        int sy = (int)(y - camY);
        sheet->drawFrame(w, (int)dir, anim.current(), sx, sy);
    }

    // 应用于玩家的击退（dirX,dirY 是方向向量；power=像素/秒；duration=秒）
    void applyKnockback(float dirX, float dirY, float power, float duration)
    {
        if (hitCooldown > 0.f) return;              // 简单防抖：无敌/冷却期内不叠加
        float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len < 1e-6f) return;
        dirX /= len; dirY /= len;
        kx = dirX * power;
        ky = dirY * power;
        kTime = duration;
        hitCooldown = 0.10f;                        // 100ms：避免同一帧多次触发
    }

    // —— 每帧：自动攻击（找最近NPC并请求 NPCSystem 生成子弹）——
    // 说明：不改你原有 update() 签名，攻击独立在此处调用
    void updateAttack(float dt, class EnemyManager& npcs)
    {
        // 冷却计时
        if (shootCD > 0.f) { shootCD -= dt; return; }

        // 取得最近 NPC 的“中心点”；若没有目标，直接返回
        float tx, ty;
        if (!npcs.findNearestAlive(x, y, tx, ty)) return;

        // 以“英雄中心”朝目标中心发射
        float sx = getHitboxX() + getHitboxW() * 0.5f;
        float sy = getHitboxY() + getHitboxH() * 0.5f;
        float dx = tx - sx, dy = ty - sy;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-5f) return;
        dx /= len; dy /= len;

        // 请求 NPCSystem 生成“英雄子弹”
        // 速度可调（px/s），寿命 ttl（秒），这里给 420 px/s，存活 1.2 s
        npcs.spawnHeroBullet(sx, sy, dx, dy, 420.f, 1.2f);

        // 重置冷却
        shootCD = shootInterval;
    }

    void setAOEParams(int N, int dmg, float interval) {
        if (N > 0) aoeN = N;
        if (dmg > 0) aoeDamage = dmg;
        if (interval > 0.f) aoeInterval = interval;
    }

    void updateAOE(float dt, EnemyManager& npcs, Window& input)
    {
        if (aoeCD > 0.f) aoeCD -= dt;

        if (aoeCD <= 0.f && input.keyPressed('J')) {  // ✅ 按 J 触发
            // 英雄中心
            float cx = getHitboxX() + getHitboxW() * 0.5f;
            float cy = getHitboxY() + getHitboxH() * 0.5f;

            // 对“HP 最高的前 N 个”发射紫色 AOE 子弹
            npcs.aoeStrikeTopN(aoeN, aoeDamage, cx, cy);

            aoeCD = aoeInterval; // 重置长冷却
        }
    }
};