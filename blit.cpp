#include "blit.h"
using namespace GamesEngineeringBase;

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



/**********************  通用：图像裁剪绘制（带Alpha）  *************************
  注意：不使用 STL；与 TileMap 的 blit 思路一致，但这是“子图裁剪”版本，
  以后敌人、道具、子弹都能用。
*******************************************************************************/
void blitSubImage(Window& window,
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