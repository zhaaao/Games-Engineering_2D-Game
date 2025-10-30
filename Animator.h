#pragma once
/***************************  动画器：推进列序（0..3）  **************************
  只处理“第几列”；行由“方向”决定。
  frameCount=4（四列），frameTime 控制播放速度（秒/帧）。
*******************************************************************************/
class Animator
{
private:
    int frameCount = 4;     // 四列
    float frameTime = 0.12f;// 每帧 0.12s，可按手感微调
    float acc = 0.f;
    int cur = 0;
    bool playing = false;

public:
    void setFrameTime(float t) { frameTime = (t > 0.f ? t : 0.1f); }
    void start() { playing = true; }
    void stop() { playing = false; cur = 0; } // 停止时回到第0帧（常用作站立）
    void update(float dt)
    {
        if (!playing) return;
        acc += dt;
        while (acc >= frameTime)
        {
            acc -= frameTime;
            cur = (cur + 1) % frameCount;
        }
    }
    int current() const { return cur; }
};