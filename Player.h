#pragma once
#include "GamesEngineeringBase.h"
#include "TileMap.h"
#include "SpriteSheet.h"
#include "Animator.h"
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

private:
    float x = 0.f, y = 0.f;     // 世界坐标（以像素为单位）
    float speed = 150.f;        // 像素/秒，按需要调
    Dir   dir = Down;           // 朝向（决定使用哪一行）
    SpriteSheet* sheet = nullptr;
    Animator anim;              // 列序动画器（0..3）
    TileMap* map = nullptr;


public:
    void attachSprite(SpriteSheet* s) { sheet = s; }
    void setPosition(float px, float py) { x = px; y = py; }
    void setSpeed(float s) { speed = s; }
    float getX() const { return x; }
    float getY() const { return y; }
    int getW() const { return sheet ? sheet->getFrameW() : 0; }
    int getH() const { return sheet ? sheet->getFrameH() : 0; }
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

        // 4) 移动
        if (!map) {
            // 兜底：没有地图就直接位移
            x += vx * speed * dt;
            y += vy * speed * dt;
        }
        else {
            const int w = getW(), h = getH();
            const int tw = map->getTileW(), th = map->getTileH();

            // -------- 水平位移并处理阻挡 --------
            float nx = x + vx * speed * dt;
            if (vx > 0.f) {
                // 取移动后右侧像素所在的瓦片列
                int right = (int)(nx + w - 1);
                int tx = right / tw;
                int top = (int)(y) / th;
                int bot = (int)(y + h - 1) / th;
                for (int ty = top; ty <= bot; ++ty) {
                    if (map->isBlockedAt(tx, ty)) {
                        nx = (float)(tx * tw - w);   // 卡在阻挡块左侧
                        break;
                    }
                }
            }
            else if (vx < 0.f) {
                int left = (int)nx;
                int tx = left / tw;
                int top = (int)(y) / th;
                int bot = (int)(y + h - 1) / th;
                for (int ty = top; ty <= bot; ++ty) {
                    if (map->isBlockedAt(tx, ty)) {
                        nx = (float)((tx + 1) * tw); // 卡在阻挡块右侧
                        break;
                    }
                }
            }
            x = nx;

            // -------- 垂直位移并处理阻挡 --------
            float ny = y + vy * speed * dt;
            if (vy > 0.f) {
                int bottom = (int)(ny + h - 1);
                int ty = bottom / th;
                int left = (int)x / tw;
                int rightC = (int)(x + w - 1) / tw;
                for (int tx = left; tx <= rightC; ++tx) {
                    if (map->isBlockedAt(tx, ty)) {
                        ny = (float)(ty * th - h);   // 卡在阻挡块上方
                        break;
                    }
                }
            }
            else if (vy < 0.f) {
                int top = (int)ny / th;
                int left = (int)x / tw;
                int rightC = (int)(x + w - 1) / tw;
                for (int tx = left; tx <= rightC; ++tx) {
                    if (map->isBlockedAt(tx, top)) {
                        ny = (float)((top + 1) * th); // 卡在阻挡块下方
                        break;
                    }
                }
            }
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
};