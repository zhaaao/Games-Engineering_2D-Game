#pragma once
#include "GamesEngineeringBase.h"
using namespace GamesEngineeringBase;


// ===== 前向声明：让 friend 能看到函数签名 =====
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

/***************************  NPC 类（单个个体）  ****************************
 * - 仅负责自己的数据、更新与绘制；池管理交给 NPCSystem
 * - 不使用 STL；尺寸为简单矩形（w,h）
 ******************************************************************************/
class NPC
{
public:
    // 0=追踪；1=炮台（静止，后续可扩展发射子弹）
    unsigned char type = 0;
    bool  alive = false;
    // --- Turret (type==1) 专用的简单发射冷却（秒） ---
    float fireCD = 0.0f;   // 倒计时到 0 即可发射；由 NPCSystem 更新与重置

private:
    float x = 0.f, y = 0.f;   // 世界坐标（左上角）
    float vx = 0.f, vy = 0.f; // 朝向单位向量
    float speed = 0.f;        // 像素/秒
    int   w = 24, h = 24;     // 矩形尺寸
    int   hp = 3;             // 预留

    // 工具：单位化
    static void unitVector(float dx, float dy, float& ox, float& oy)
    {
        float len2 = dx * dx + dy * dy;
        if (len2 <= 1e-6f) { ox = 0.f; oy = 0.f; return; }
        float inv = 1.0f / (float)sqrt(len2);
        ox = dx * inv; oy = dy * inv;
    }

public:
    friend class EnemyManager;
    // === 生命周期 ===
    void kill() { alive = false; }
    bool isAlive()const { return alive; }

    // === 只在生成时调用：一次性初始化 ===
    void initSpawn(float sx, float sy, unsigned char _type, float _speed, float faceTx, float faceTy)
    {
        x = sx; y = sy; type = _type; speed = _speed; alive = true; hp = 3;
        unitVector(faceTx - x, faceTy - y, vx, vy); // 初始朝向面对玩家
    }

    // === 帧更新 ===
    // worldW/worldH：像素世界边界，用于夹紧
    void update(float dt, float targetX, float targetY, int worldW, int worldH)
    {
        if (!alive) return;

        // 非炮台（type != 1）都要移动追踪玩家
        if (type != 1) {
            // 按类型给不同的“转向灵敏度”：
            // 轻型(2)更灵活，坦克(3)更迟钝，普通追踪(0)居中
            float steer = (type == 2 ? 0.35f : (type == 3 ? 0.15f : 0.20f));
            float inertia = 1.0f - steer;

            float ux, uy;
            unitVector(targetX - x, targetY - y, ux, uy);
            // 指向目标的平滑转向
            vx = inertia * vx + steer * ux;
            vy = inertia * vy + steer * uy;
            unitVector(vx, vy, vx, vy);

            x += vx * speed * dt;
            y += vy * speed * dt;
        }
        // type == 1 炮台：保持静止，仅在系统里发射子弹

        // 边界夹紧
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (x > worldW - w) x = (float)(worldW - w);
        if (y > worldH - h) y = (float)(worldH - h);

        // 预留：hp<=0 时 kill()
    }

    // === 绘制（相机左上 -> 屏幕坐标） ===
    void draw(Window& win, float camX, float camY)
    {
        if (!alive) return;
        const int W = (int)win.getWidth(), H = (int)win.getHeight();
        int sx = (int)(x - camX);
        int sy = (int)(y - camY);
        if (sx + w < 0 || sy + h < 0 || sx >= W || sy >= H) return;

        unsigned char r = 255, g = 0, b = 0;
        switch (type)
        {
        case 0: // 追踪：红
            r = 255; g = 60;  b = 60;  break;
        case 1: // 炮台：紫
            r = 180; g = 0;   b = 255; break;
        case 2: // 轻型冲锋：青绿
            r = 40;  g = 230; b = 200; break;
        case 3: // 重装坦克：橙
            r = 255; g = 150; b = 40;  break;
        default:
            r = 255; g = 0;   b = 0;   break;
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

    void applyDamage(int dmg) {
        if (!alive || dmg <= 0) return;
        if (hp > 0) {
            hp -= dmg;
            if (hp <= 0) { kill(); }
        }
        else {
            // 若没有使用 hp 的关卡，也保证能被击杀
            kill();
        }
    }

    // === 只读访问（可用于碰撞等扩展） ===
    float getX()const { return x; }
    float getY()const { return y; }
    int   getW()const { return w; }
    int   getH()const { return h; }
    // 命名统一的碰撞盒访问（与 Player 命名对齐）
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }
    // --- 放到 NPC.h, class NPC 的 public 区域 ---
    int   getType() const { return type; }
    void  setType(int t) { type = t; }

    float getFireCD() const { return fireCD; }
    void  setFireCD(float v) { fireCD = v; }
    int   getHP() const { return hp; }
    // ✅ 允许 SaveLoad 模块访问私有字段
    friend bool SaveLoad::SaveToFile(const char*, const Player&, const EnemyManager&, float, int, bool);
    friend bool SaveLoad::LoadFromFile(const char*, Player&, EnemyManager&, float&, int&, bool&);
};
