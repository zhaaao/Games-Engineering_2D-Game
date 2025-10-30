#include "gfx_utils.h"
using namespace GamesEngineeringBase;

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