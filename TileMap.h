#pragma once
#include "GamesEngineeringBase.h"
#include "gfx_utils.h"
#include "blit.h"
#include <string>
using namespace GamesEngineeringBase;
using namespace std;

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



public:
    // ---- 地形属性（你给的约定：14~22 是水体，英雄不可穿越）----
    bool isBlockedId(int id) const { return id >= 14 && id <= 22; }

    // 按瓦片坐标判断是否阻挡（越界一律视为不阻挡，交给边界夹紧处理）
    bool isBlockedAt(int tx, int ty) const {
        int id = get(tx, ty);
        return (id >= 0) ? isBlockedId(id) : false;
    }

    // 瓦片尺寸访问（用于碰撞计算）
    int getTileW() const { return tileW; }
    int getTileH() const { return tileH; }
    // --- 获取地图瓦片尺寸（列数、行数）---
    int getWidth() const { return width; }   // 地图列数（水平瓦片数）
    int getHeight() const { return height; } // 地图行数（垂直瓦片数）

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