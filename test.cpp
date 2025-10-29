#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include "GamesEngineeringBase.h"
using namespace GamesEngineeringBase;

static const int TILE = 32;

// ✅ 填充矩形函数（保持不变）
static void fillRect(Window& win, int x0, int y0, int w, int h,
    unsigned char r, unsigned char g, unsigned char b)
{
    int W = (int)win.getWidth(), H = (int)win.getHeight();
    int x1 = x0 + w, y1 = y0 + h;
    if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    for (int y = y0; y < y1; ++y) {
        int base = y * W;
        for (int x = x0; x < x1; ++x) {
            win.draw(base + x, r, g, b);
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

public:
    // 构造 & 析构
    TileMap() = default;
    ~TileMap() { delete[] data; }

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
    void draw(Window& window, float camX, float camY) const {
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

                unsigned char r, g, b;
                idToColor(id, r, g, b);
                fillRect(window, sx, sy, tileW, tileH, r, g, b);
            }
        }
    }

    // 获取地图像素尺寸
    int getPixelWidth()  const { return width * tileW; }
    int getPixelHeight() const { return height * tileH; }
};
