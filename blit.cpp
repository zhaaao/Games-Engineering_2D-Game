#include "blit.h"
using namespace GamesEngineeringBase;

/*
 * blitImage()
 * ------------------------------------------------------------------------
 * Draws an entire image onto the window’s back buffer at the given position.
 *
 * Features:
 * - Performs clipping to ensure drawing remains within window bounds.
 * - Per-pixel copy (no scaling or interpolation).
 * - Includes a simple alpha test: pixels with very low alpha (<16) are skipped.
 *
 * Parameters:
 *   window : target window to draw to.
 *   img    : source image (e.g., a 32×32 tile or sprite).
 *   dstX,Y : destination top-left position in screen coordinates.
 *
 * Implementation notes:
 * - Uses the window’s linear framebuffer indexing (y * width + x).
 * - Designed for efficiency in software rendering.
 */
void blitImage(Window& window, const GamesEngineeringBase::Image& img, int dstX, int dstY)
{
    const int W = (int)window.getWidth();
    const int H = (int)window.getHeight();

    // Compute visible rectangle and clip against window bounds
    int x0 = dstX, y0 = dstY;
    int x1 = dstX + (int)img.width;
    int y1 = dstY + (int)img.height;
    if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;

    // Copy pixels row by row
    for (int y = y0; y < y1; ++y)
    {
        int srcY = y - dstY;
        int base = y * W;
        for (int x = x0; x < x1; ++x)
        {
            int srcX = x - dstX;
            unsigned char a = img.alphaAtUnchecked(srcX, srcY);
            if (a < 16) continue; // Skip nearly transparent pixels

            unsigned char* p = img.atUnchecked(srcX, srcY); // RGB pointer
            window.draw(base + x, p[0], p[1], p[2]);        // Write to framebuffer
        }
    }
}


/*
 * blitSubImage()
 * ------------------------------------------------------------------------
 * Renders a cropped region of an image into the window.
 * Commonly used for drawing individual frames from a sprite sheet or
 * tile atlas.
 *
 * Features:
 * - Clips both source and destination regions to prevent out-of-bounds access.
 * - Copies pixels directly with a simple alpha cutoff (<16 ignored).
 * - No blending or scaling; exact 1:1 copy.
 *
 * Parameters:
 *   window : target window.
 *   img    : source image.
 *   srcX,Y : top-left corner of the source sub-rectangle.
 *   subW,H : width and height of the region to draw.
 *   dstX,Y : destination top-left corner on screen.
 *
 * Implementation notes:
 * - The function manually clamps coordinates on both sides.
 * - Works efficiently in software pixel loops.
 */
void blitSubImage(Window& window,
    const Image& img,
    int srcX, int srcY, int subW, int subH,
    int dstX, int dstY)
{
    const int W = (int)window.getWidth();
    const int H = (int)window.getHeight();

    // Clip source region inside image bounds
    if (srcX < 0) { int d = -srcX; srcX = 0; subW -= d; dstX += d; }
    if (srcY < 0) { int d = -srcY; srcY = 0; subH -= d; dstY += d; }
    if (srcX + subW > (int)img.width)  subW = (int)img.width - srcX;
    if (srcY + subH > (int)img.height) subH = (int)img.height - srcY;
    if (subW <= 0 || subH <= 0) return;

    // Clip destination region inside window bounds
    int x0 = dstX, y0 = dstY;
    int x1 = dstX + subW;
    int y1 = dstY + subH;
    if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return;
    if (x0 < 0) { int d = -x0; x0 = 0; srcX += d; }
    if (y0 < 0) { int d = -y0; y0 = 0; srcY += d; }
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;

    int drawW = x1 - x0;
    int drawH = y1 - y0;

    // Copy each visible pixel
    for (int y = 0; y < drawH; ++y)
    {
        int sy = srcY + y;
        int dy = y0 + y;
        int baseDest = dy * W;

        for (int x = 0; x < drawW; ++x)
        {
            int sx = srcX + x;
            int dx = x0 + x;

            unsigned char a = img.alphaAtUnchecked((unsigned int)sx, (unsigned int)sy);
            if (a < 16) continue; // Skip transparent pixels

            unsigned char* sp = img.atUnchecked((unsigned int)sx, (unsigned int)sy);
            window.draw(baseDest + dx, sp[0], sp[1], sp[2]);
        }
    }
}
