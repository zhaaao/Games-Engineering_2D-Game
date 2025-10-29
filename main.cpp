#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include "GamesEngineeringBase.h"
#include <iostream>
using namespace GamesEngineeringBase;
using namespace std;


// 简单工具：画实心矩形（用于瓦片）
void fillRect(Window& canvas, int x0, int y0, int w, int h,
    unsigned char r, unsigned char g, unsigned char b)
{
    // 边界裁剪（避免越界写 back buffer）
    int W = (int)canvas.getWidth();
    int H = (int)canvas.getHeight();
    int x1 = x0 + w;
    int y1 = y0 + h;
    if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;

    for (int y = y0; y < y1; ++y)
    {
        int base = y * W;
        for (int x = x0; x < x1; ++x)
        {
            canvas.draw(base + x, r, g, b); // Window::draw(pixelIndex, r,g,b) :contentReference[oaicite:2]{index=2}
        }
    }
}

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
// ✅ TileMap 类
class TileMap {
private:
    int width = 0, height = 0;
    int tileW = 32, tileH = 32;
    int* data = nullptr;
    // --- 贴图缓存（不用 STL）：固定上限，按需加载 ---
    static const int MAX_TILE_ID = 1024;   // 够用即可，可根据需要调大
    GamesEngineeringBase::Image* tileImg[MAX_TILE_ID];
    char folder[260]; // 资源目录（例如 "./tiles/"）

    // 初始化缓存为空
    void initCache() {
        for (int i = 0; i < MAX_TILE_ID; ++i) tileImg[i] = nullptr;
        folder[0] = '\0';
    }

    // 解析一行里的所有整数
    static int parseInts(const std::string& line, int* dst, int maxWrite) {
        int written = 0;
        int i = 0, n = (int)line.size();
        while (i < n && written < maxWrite) {
            while (i < n && !(line[i] == '-' || (line[i] >= '0' && line[i] <= '9'))) ++i;
            if (i >= n) break;
            int sign = 1;
            if (line[i] == '-') { sign = -1; ++i; }
            long val = 0;
            bool hasDigit = false;
            while (i < n && (line[i] >= '0' && line[i] <= '9')) {
                val = val * 10 + (line[i] - '0');
                ++i; hasDigit = true;
            }
            if (hasDigit) {
                dst[written++] = (int)(sign * val);
            }
            while (i < n && (line[i] == ',' || line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        }
        return written;
    }

    // 懒加载：第一次用到某个 id 时，尝试加载 "<folder><id>.png"
    Image* getTileImage(int id) {
        if (id < 0 || id >= MAX_TILE_ID) return nullptr;
        if (tileImg[id]) return tileImg[id];

        // 组文件名
        char filename[320];
        int n = 0;
        // 拷贝目录
        for (; folder[n] && n < 259; ++n) filename[n] = folder[n];
        // 追加数字
        int start = n;
        if (id == 0) { filename[n++] = '0'; }
        else {
            // 把 id 转成字符串（不使用 sprintf，为了少依赖）
            int tmp = id, digits[10]; int k = 0;
            while (tmp > 0 && k < 10) { digits[k++] = tmp % 10; tmp /= 10; }
            for (int j = k - 1; j >= 0; --j) filename[n++] = char('0' + digits[j]);
        }
        // 追加 .png
        filename[n++] = '.'; filename[n++] = 'p'; filename[n++] = 'n'; filename[n++] = 'g';
        filename[n] = '\0';

        // 加载
        Image* img = new Image();
        if (!img->load(string(filename))) { // Image::load 使用 WIC 读取 PNG/JPG 等 :contentReference[oaicite:2]{index=2}:contentReference[oaicite:3]{index=3}
            delete img;
            tileImg[id] = nullptr; // 记失败，避免反复尝试
            return nullptr;
        }
        tileImg[id] = img;
        return img;
    }

    // 将 img(一般32x32) 拷贝到窗口 back buffer 上的 (dstX, dstY)，带裁剪与简单透明测试
    void blitImage(Window& window, const GamesEngineeringBase::Image& img, int dstX, int dstY) {
        const int W = (int)window.getWidth();
        const int H = (int)window.getHeight();

        // 裁剪：计算可见矩形
        int x0 = dstX, y0 = dstY;
        int x1 = dstX + (int)img.width;
        int y1 = dstY + (int)img.height;
        if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > W) x1 = W; if (y1 > H) y1 = H;

        // 逐像素拷贝
        for (int y = y0; y < y1; ++y) {
            int srcY = y - dstY;
            int base = y * W; // window backbuffer 索引基址（像素）:contentReference[oaicite:6]{index=6}
            for (int x = x0; x < x1; ++x) {
                int srcX = x - dstX;
                unsigned char a = img.alphaAtUnchecked(srcX, srcY); // 直接访问，性能好，前面已裁剪 :contentReference[oaicite:7]{index=7}
                if (a < 16) continue; // 近似“透明”就跳过；需要混合再扩展

                unsigned char* p = img.atUnchecked(srcX, srcY);     // 取源 RGB :contentReference[oaicite:8]{index=8}
                window.draw(base + x, p[0], p[1], p[2]);            // 写目标像素（索引接口）:contentReference[oaicite:9]{index=9}
            }
        }
    }

public:
    // 构造 & 析构
    TileMap() { initCache(); }
    ~TileMap() {
        delete[] data;
        // 释放图片
        for (int i = 0; i < MAX_TILE_ID; ++i) { if (tileImg[i]) { tileImg[i]->~Image(); delete tileImg[i]; tileImg[i] = nullptr; } }
    }
    // 设置图片所在文件夹（以斜杠结尾更方便，例如 "./tiles/"）
    void setImageFolder(const char* path) {
        // 简单拷贝（不做复杂健壮性处理）
        int i = 0;
        for (; path[i] && i < 259; ++i) folder[i] = path[i];
        folder[i] = '\0';
    }

    // 读取 tiles.txt
    bool load(const char* path) {
        std::ifstream f(path);
        if (!f) return false;

        std::string line;
        bool haveW = false, haveH = false, haveTW = false, haveTH = false;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string key; ss >> key;
            if (key == "tileswide") { ss >> width;  haveW = (width > 0); }
            else if (key == "tileshigh") { ss >> height; haveH = (height > 0); }
            else if (key == "tilewidth") { ss >> tileW;  haveTW = (tileW > 0); }
            else if (key == "tileheight") { ss >> tileH;  haveTH = (tileH > 0); }
            else if (key == "layer") {
                break; // 到 layer 就停
            }
        }
        if (!(haveW && haveH && haveTW && haveTH)) return false;

        delete[] data;
        data = new int[width * height];
        int idx = 0;

        while (idx < width * height && std::getline(f, line)) {
            if (line.empty()) continue;
            idx += parseInts(line, data + idx, width * height - idx);
        }
        return (idx == width * height);
    }

    // 获取 tile id
    int get(int x, int y) const {
        if (!data) return -1;
        if (x < 0 || y < 0 || x >= width || y >= height) return -1;
        return data[y * width + x];
    }

    // 绘制地图
    void draw(Window& window, float camX, float camY) {
        if (!data) return;
        int W = (int)window.getWidth(), H = (int)window.getHeight();

        int startTileX = (int)std::floor(camX / tileW) - 1;
        int startTileY = (int)std::floor(camY / tileH) - 1;
        int tilesX = W / tileW + 3;
        int tilesY = H / tileH + 3;

        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                int gx = startTileX + tx;
                int gy = startTileY + ty;
                int id = get(gx, gy);
                if (id < 0) continue;

                int sx = gx * tileW - (int)camX;
                int sy = gy * tileH - (int)camY;

                Image* img = getTileImage(id);
                if (img && (int)img->width == tileW && (int)img->height == tileH) {
                    blitImage(window, *img, sx, sy);
                }
                else {
                    // 找不到贴图或尺寸不匹配时：回退为纯色，防止“黑格子”
                    unsigned char r, g, b;
                    // 也可以把原来的 idToColor 保留为 fallback
                    // 这里简写个暗灰
                    r = g = b = 40;
                    fillRect(window, sx, sy, tileW, tileH, r, g, b);
                }

            }
        }
    }

    // 获取地图像素尺寸
    int getPixelWidth()  const { return width * tileW; }
    int getPixelHeight() const { return height * tileH; }
};

/**********************  通用：图像裁剪绘制（带Alpha）  *************************
  注意：不使用 STL；与 TileMap 的 blit 思路一致，但这是“子图裁剪”版本，
  以后敌人、道具、子弹都能用。
*******************************************************************************/
static void blitSubImage(Window& window,
    const Image& img,
    int srcX, int srcY, int subW, int subH,
    int dstX, int dstY)
{
    const int W = (int)window.getWidth();
    const int H = (int)window.getHeight();

    // 源区域裁剪到图像范围内（防止越界读）
    if (srcX < 0) { int d = -srcX; srcX = 0; subW -= d; dstX += d; }
    if (srcY < 0) { int d = -srcY; srcY = 0; subH -= d; dstY += d; }
    if (srcX + subW > (int)img.width)  subW = (int)img.width - srcX;
    if (srcY + subH > (int)img.height) subH = (int)img.height - srcY;

    if (subW <= 0 || subH <= 0) return;

    // 目标区域裁剪到屏幕范围内（防止越界写）
    int x0 = dstX, y0 = dstY;
    int x1 = dstX + subW;
    int y1 = dstY + subH;
    if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) { int d = -x0; x0 = 0; srcX += d; }
    if (y0 < 0) { int d = -y0; y0 = 0; srcY += d; }
    if (x1 > W) { int d = x1 - W; x1 = W; /* srcX 不变，宽自然减小 */ }
    if (y1 > H) { int d = y1 - H; y1 = H; /* 同上 */ }

    int drawW = x1 - x0;
    int drawH = y1 - y0;

    // 逐像素拷贝（带简单 Alpha 测试；如需混合，可以在日后加入混合公式）
    for (int y = 0; y < drawH; ++y)
    {
        int sy = srcY + y;
        int dy = y0 + y;
        int baseDest = dy * W; // Window backbuffer 的像素基址
        for (int x = 0; x < drawW; ++x)
        {
            int sx = srcX + x;
            int dx = x0 + x;

            unsigned char a = img.alphaAtUnchecked((unsigned int)sx, (unsigned int)sy);
            if (a < 16) continue; // 近似全透明：跳过

            unsigned char* sp = img.atUnchecked((unsigned int)sx, (unsigned int)sy); // RGB(A)
            window.draw(baseDest + dx, sp[0], sp[1], sp[2]);
        }
    }
}

/***************************  精灵表：裁剪第(row,col)帧  *************************
  约定：行 0=Down, 1=Right, 2=Left, 3=Up；列 0..3
  支持任意行数（>=4）与固定的 4 列；行列从 0 开始计数。
*******************************************************************************/
class SpriteSheet
{
private:
    GamesEngineeringBase::Image image; // 原始整张精灵表
    int frameW = 0, frameH = 0;        // 单帧尺寸
    int rows = 0, cols = 0;            // 行数、列数（当前列固定用4；保留通用性）

public:
    bool load(const char* filename, int fw, int fh, int _rows, int _cols)
    {
        if (!image.load(std::string(filename))) return false;
        frameW = fw; frameH = fh; rows = _rows; cols = _cols;
        // 基本健壮性检查
        if (frameW <= 0 || frameH <= 0) return false;
        if (rows <= 0 || cols <= 0) return false;
        return true;
    }

    // 画出第 row 行、第 col 列的帧到屏幕（dstX,dstY 是屏幕坐标）
    void drawFrame(Window& w, int row, int col, int dstX, int dstY) const
    {
        if (row < 0 || row >= rows) return;
        if (col < 0 || col >= cols) return;
        int sx = col * frameW;
        int sy = row * frameH;
        blitSubImage(w, image, sx, sy, frameW, frameH, dstX, dstY);
    }

    int getFrameW() const { return frameW; }
    int getFrameH() const { return frameH; }
    bool valid() const { return image.data != nullptr; }
};

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

private:
    float x = 0.f, y = 0.f;     // 世界坐标（以像素为单位）
    float speed = 150.f;        // 像素/秒，按需要调
    Dir   dir = Down;           // 朝向（决定使用哪一行）
    SpriteSheet* sheet = nullptr;
    Animator anim;              // 列序动画器（0..3）

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
        x += vx * speed * dt;
        y += vy * speed * dt;

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
    hero.setSpeed(150.f);
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
        hero.draw(canvas, camX, camY);         // 玩家立刻叠在上面
        // 显示到屏幕（把 back buffer 推给 GPU 并 Present）
        canvas.present(); // UpdateSubresource + Draw + SwapChain::Present 已封装好 :contentReference[oaicite:9]{index=9}
    }

    return 0;
}
