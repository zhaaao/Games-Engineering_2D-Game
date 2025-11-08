#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"   // 需要地图尺寸与瓦片尺寸
#include "NPC.h"
using namespace GamesEngineeringBase;

class Player;

/*********************  敌方子弹（固定容量池，不用 STL）  *********************/
struct EnemyProjectile
{
    bool  alive = false;
    float x = 0.f, y = 0.f;     // 世界坐标（左上角）
    float vx = 0.f, vy = 0.f;   // 速度（px/s）
    float life = 0.f;           // 剩余寿命（秒），<=0 则回收
    int   w = 6, h = 6;         // 简单矩形尺寸（像素）

    void spawn(float sx, float sy, float dirx, float diry, float speed, float ttl)
    {
        alive = true;
        x = sx; y = sy;
        // 归一化方向
        float len = std::sqrt(dirx * dirx + diry * diry);
        if (len < 1e-6f) { dirx = 1.f; diry = 0.f; len = 1.f; } // 兜底
        dirx /= len; diry /= len;
        vx = dirx * speed;
        vy = diry * speed;
        life = ttl;
    }

    // AABB 碰撞盒（与 Player / NPC 的 getHitbox* 命名保持一致）
    float getHitboxX() const { return x; }
    float getHitboxY() const { return y; }
    int   getHitboxW() const { return w; }
    int   getHitboxH() const { return h; }
};

/*********************  英雄子弹（固定容量池，不用 STL）  *********************/
struct PlayerProjectile
{
    bool  alive = false;
    float x = 0.f, y = 0.f;     // 世界坐标（左上/中心影响不大，统一即可）
    float vx = 0.f, vy = 0.f;   // 速度（px/s）
    float life = 0.f;           // 剩余寿命（秒）
    int   w = 6, h = 6;         // 简单矩形尺寸（像素）

    unsigned char r = 40, g = 200, b = 255; // 默认青色（普通直线子弹）
    int  damage = 1;                         // 默认1点伤害
    bool isAOE = false;                      // AOE 专用标记（仅用于渲染/调试）
    void spawn(float sx, float sy, float dirx, float diry, float speed, float ttl);
    
    void update(float dt);
};


/***************************  NPCSystem：池与生成器  **************************
 * - 固定容量数组（MAX=128），不使用 STL
 * - 生成：相机外环带；频率随时间线性加快（封顶）
 * - 更新：追踪玩家；NPC 可穿越水（保持你现有差异）
 ******************************************************************************/
class EnemyManager
{
public:
    static const int MAX = 128;
    static const int BULLET_MAX = 256;

    bool isInfiniteWorld = false;
private:
    NPC enemies[MAX];
    TileMap* tileMap = nullptr;
    float elapsedSeconds = 0.f;      // 累计时间
    float spawnAccumulator = 0.f;   // 生成计时
    // ----------------- 子弹池 -----------------
    EnemyProjectile enemyProjectiles [BULLET_MAX];
    static const int kPlayerProjectileCapacity = 256;
    PlayerProjectile playerProjectiles [kPlayerProjectileCapacity];
    // 世界尺寸缓存（像素）
    int worldWidthPx = 0, worldHeightPx = 0;
    // 频率参数（与你现有一致）
    static constexpr float kSpawnBaseInterval = 1.6f;
    static constexpr float kSpawnMinInterval = 0.35f;
    static constexpr float kSpawnAccelPerSec = 0.02f;

    // 简单双精随机与夹紧
    static float frand01() { return (float)rand() / (float)RAND_MAX; }
    static float fclamp(float v, float a, float b) { return (v < a) ? a : ((v > b) ? b : v); }

    // 找空位；没空返回 -1
    int allocIndex();

    // 世界像素尺寸
    void mapPixelSize(int& outW, int& outH);

    // 实际生成 1 个
    void spawnOne(float camX, float camY, int viewW, int viewH, float px, float py);

    // 取一个空闲子弹槽
    int allocBullet();

    // 工具：从 NPC 中心到目标（玩家中心）的方向
    static inline void dirFromAToB(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh,
        float& outx, float& outy)
    {
        float acx = ax + aw * 0.5f, acy = ay + ah * 0.5f;
        float bcx = bx + bw * 0.5f, bcy = by + bh * 0.5f;
        outx = (bcx - acx); outy = (bcy - acy);
    }

    // AABB 判交（不分离，仅检测）
    static inline bool aabbOverlap(float ax, float ay, int aw, int ah,
        float bx, float by, int bw, int bh)
    {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

public:
    void init(TileMap* m);

    // ---- AABB 矩形相交 ----
    static inline bool aabbIntersect(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh)
    {
        return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
    }

    // 每帧：尝试生成（频率随时间加快）
    void trySpawn(float dt, float camX, float camY, int viewW, int viewH, float px, float py);

    // 更新全体（传玩家坐标）——含：追踪AI、炮台冷却与发射子弹
    void updateAll(float dt, float px, float py);


    // 绘制全体（传相机左上）
    void drawAll(Window& win, float camX, float camY);

    // --- 玩家与所有存活 NPC 的碰撞检查（含最小分离）---
    void checkPlayerCollision(Player& hero);

    // 若你后续做碰撞/查询，可提供一个只读指针
    const NPC* getArray() const { return enemies; }
    NPC* getArray() { return enemies; }

    // —— O(N) 线性近邻：找到最近的存活 NPC，返回其“中心点” ——
// 输入 (px,py) 通常是英雄中心；输出 (tx,ty) 是 NPC 中心
    bool findNearestAlive(float px, float py, float& tx, float& ty) const;


    // —— 敌方子弹更新（越界或寿命到期即回收）——
    void updateBullets(float dt);

    // —— 敌方子弹命中英雄：击退 + 销毁 —— 
    void checkHeroHit(Player& hero);

    // —— 敌方子弹绘制 —— 
    void drawBullets(Window& win, float camX, float camY);

    // 生成一发英雄子弹
    void spawnHeroBullet(float sx, float sy, float dirx, float diry, float speed, float ttl);

    // 更新英雄子弹
    void updateHeroBullets(float dt);

    // 绘制英雄子弹（与你敌弹的 drawBullets 风格一致，避免越界）
    void drawHeroBullets(Window& win, float camX, float camY);

    // 命中检测：英雄子弹 vs NPC（命中即扣血/击杀）；返回“击杀数”
    // 若你已有 NPC 的 hp/kill() 接口，直接调用；否则可先做“命中即移除”
    int checkNPCHit();

    int  aoeStrikeTopN(int N, int damage, float heroCx, float heroCy);
    void spawnAoeBullet(float sx, float sy, float tx, float ty, float speed, float ttl, int dmg);

    void setInfinite(bool v) { isInfiniteWorld = v; }

    

};
