#include "gfx_utils.h"
using namespace GamesEngineeringBase;

/*
 * fillRect()
 * ------------------------------------------------------------------------
 * Draws a solid-colored rectangle directly into the window back buffer.
 * Used mainly for tiles, UI panels, and simple debug overlays.
 *
 * Parameters:
 *   canvas - target rendering window (provides width, height, draw()).
 *   x0, y0 - top-left corner in screen pixels.
 *   w, h   - width and height of the rectangle in pixels.
 *   r,g,b  - fill color (8-bit each).
 *
 * Implementation notes:
 * - Performs manual clipping to avoid out-of-bounds writes.
 * - Writes pixel-by-pixel using the linear framebuffer (y * W + x index).
 * - No transparency or blending — this is a fast, opaque fill.
 */
void fillRect(Window& canvas, int x0, int y0, int w, int h,
    unsigned char r, unsigned char g, unsigned char b)
{
    // Clamp to window boundaries before writing to buffer
    int W = (int)canvas.getWidth();
    int H = (int)canvas.getHeight();
    int x1 = x0 + w;
    int y1 = y0 + h;

    if (x0 >= W || y0 >= H || x1 <= 0 || y1 <= 0) return; // fully off-screen
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W;
    if (y1 > H) y1 = H;

    // Write RGB values directly into the back buffer
    for (int y = y0; y < y1; ++y)
    {
        int base = y * W;
        for (int x = x0; x < x1; ++x)
        {
            canvas.draw(base + x, r, g, b);
        }
    }
}
// Small helper: map a tile id to a fallback color when no texture is present.
// Keeps the world readable even if an image fails to load.
void idToColor(int id, unsigned char& r, unsigned char& g, unsigned char& b)
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

// ============================== HUD primitives ===============================
// 5x7 bitmap font, stippled panels, and minimal pixel writers.
// Avoid STL and keep drawing branch-light for per-frame HUD rendering.
// ============================================================================

void putPix(GamesEngineeringBase::Window& w, int x, int y,
    unsigned char r, unsigned char g, unsigned char b)
{
    const int W = (int)w.getWidth(), H = (int)w.getHeight();
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
    w.draw(y * W + x, r, g, b);
}

void fillRectUI(GamesEngineeringBase::Window& w, int x0, int y0, int wdt, int hgt,
    unsigned char r, unsigned char g, unsigned char b)
{
    const int W = (int)w.getWidth(), H = (int)w.getHeight();
    int x1 = x0 + wdt, y1 = y0 + hgt;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; ++y) {
        int base = y * W;
        for (int x = x0; x < x1; ++x) {
            w.draw(base + x, r, g, b);
        }
    }
}


// Minimal ASCII set used by HUD.
const Glyph5x7* getGlyph5x7(char c)
{
    // digits 0–9
    static const Glyph5x7 G0 = { {0x1E,0x11,0x13,0x15,0x19,0x11,0x1E} };
    static const Glyph5x7 G1 = { {0x04,0x0C,0x14,0x04,0x04,0x04,0x1F} };
    static const Glyph5x7 G2 = { {0x1E,0x01,0x01,0x1E,0x10,0x10,0x1F} };
    static const Glyph5x7 G3 = { {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E} };
    static const Glyph5x7 G4 = { {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02} };
    static const Glyph5x7 G5 = { {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E} };
    static const Glyph5x7 G6 = { {0x0E,0x10,0x10,0x1E,0x11,0x11,0x1E} };
    static const Glyph5x7 G7 = { {0x1F,0x01,0x02,0x04,0x08,0x08,0x08} };
    static const Glyph5x7 G8 = { {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E} };
    static const Glyph5x7 G9 = { {0x1E,0x11,0x11,0x1E,0x01,0x01,0x0E} };

    // letters we actually print: TIME, FPS, KILLS, etc.
    static const Glyph5x7 GT = { {0x1F,0x04,0x04,0x04,0x04,0x04,0x04} };
    static const Glyph5x7 GI = { {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F} };
    static const Glyph5x7 GM = { {0x11,0x1B,0x15,0x11,0x11,0x11,0x11} };
    static const Glyph5x7 GE = { {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F} };
    static const Glyph5x7 GF = { {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10} };
    static const Glyph5x7 GP = { {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10} };
    static const Glyph5x7 GS = { {0x0F,0x10,0x10,0x1E,0x01,0x01,0x1E} };
    static const Glyph5x7 GK = { {0x11,0x12,0x14,0x18,0x14,0x12,0x11} };
    static const Glyph5x7 GL = { {0x10,0x10,0x10,0x10,0x10,0x10,0x1F} };
    static const Glyph5x7 GO = { {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E} };
    static const Glyph5x7 GR = { {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11} };

    static const Glyph5x7 GCOL = { {0x00,0x04,0x00,0x00,0x00,0x04,0x00} }; // ':'

    if (c >= '0' && c <= '9') {
        static const Glyph5x7* DIG[10] = { &G0,&G1,&G2,&G3,&G4,&G5,&G6,&G7,&G8,&G9 };
        return DIG[c - '0'];
    }
    switch (c) {
    case 'T': return &GT; case 'I': return &GI; case 'M': return &GM; case 'E': return &GE;
    case 'F': return &GF; case 'P': return &GP; case 'S': return &GS;
    case 'K': return &GK; case 'L': return &GL; case 'O': return &GO; case 'R': return &GR;
    case ':': return &GCOL;
    }
    return nullptr;
}

void drawChar5x7(GamesEngineeringBase::Window& w, int x, int y,
    char c, unsigned char r, unsigned char g, unsigned char b, int scale = 2)
{
    const Glyph5x7* gph = getGlyph5x7(c);
    if (!gph) return;
    for (int ry = 0; ry < 7; ++ry) {
        unsigned char mask = gph->row[ry];
        for (int rx = 0; rx < 5; ++rx) {
            if (mask & (1 << (4 - rx))) {
                // scale each lit pixel into a scale×scale block
                for (int yy = 0; yy < scale; ++yy)
                    for (int xx = 0; xx < scale; ++xx)
                        putPix(w, x + rx * scale + xx, y + ry * scale + yy, r, g, b);
            }
        }
    }
}

void drawText5x7(GamesEngineeringBase::Window& w, int x, int y,
    const char* s, unsigned char r, unsigned char g, unsigned char b, int scale = 2, int spacing = 1)
{
    int cx = x;
    for (int i = 0; s[i]; ++i) {
        drawChar5x7(w, cx, y, s[i], r, g, b, scale);
        cx += 5 * scale + spacing;
    }
}

// Integer-only number drawing (no std::string). Writes '-' first, then digits.
void drawNumber(GamesEngineeringBase::Window& w, int x, int y, int v,
    unsigned char r, unsigned char g, unsigned char b, int scale = 2)
{
    if (v == 0) { drawChar5x7(w, x, y, '0', r, g, b, scale); return; }
    int buf[12]; int n = 0;
    int t = (v < 0 ? -v : v);
    while (t > 0 && n < 12) { buf[n++] = t % 10; t /= 10; }
    if (v < 0) { drawChar5x7(w, x, y, '-', r, g, b, scale); x += 5 * scale + 1; }
    for (int i = n - 1; i >= 0; --i) {
        drawChar5x7(w, x, y, (char)('0' + buf[i]), r, g, b, scale);
        x += 5 * scale + 1;
    }
}

// Stippled fill to fake translucency over the back buffer.
// pattern=0: checker at 1px; pattern=1: sparser 2×2 grid.
void fillRectStipple(GamesEngineeringBase::Window& w, int x0, int y0, int wdt, int hgt,
    unsigned char r, unsigned char g, unsigned char b, int pattern = 0)
{
    const int W = (int)w.getWidth(), H = (int)w.getHeight();
    int x1 = x0 + wdt, y1 = y0 + hgt;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x0 >= x1 || y0 >= y1) return;

    for (int y = y0; y < y1; ++y) {
        int base = y * W;
        for (int x = x0; x < x1; ++x) {
            bool paint = (pattern == 0) ? (((x ^ y) & 1) == 0)
                : (((x & 1) == 0) && ((y & 1) == 0));
            if (paint) w.draw(base + x, r, g, b);
        }
    }
}

// Compact HUD: TIME, FPS, KILLS; uses the 5×7 font above.
void drawHUD(GamesEngineeringBase::Window& w,
    int remainSec, int fpsInt, int kills)
{
    const int panelX = 10, panelY = 10, panelW = 220, panelH = 80;
    fillRectStipple(w, panelX, panelY, panelW, panelH, 18, 18, 18, /*pattern=*/0);

    drawText5x7(w, panelX + 8, panelY + 8, "TIME:", 255, 240, 180, 2);
    drawNumber(w, panelX + 92, panelY + 8, remainSec, 255, 240, 180, 2);

    drawText5x7(w, panelX + 8, panelY + 32, "FPS:", 180, 220, 255, 2);
    drawNumber(w, panelX + 72, panelY + 32, fpsInt, 180, 220, 255, 2);

    drawText5x7(w, panelX + 8, panelY + 56, "KILLS:", 200, 255, 200, 2);
    drawNumber(w, panelX + 96, panelY + 56, kills, 200, 255, 200, 2);
}


// ============================== Game Over panel ===============================
// Block input loop that overlays stats and waits for ENTER/Q/ESC to exit.
// Keeps the last frame’s back buffer visible underneath a stippled mask.
// =============================================================================
void showGameOverScreen(GamesEngineeringBase::Window& w,
    int kills, int fpsInt)
{
    const int W = (int)w.getWidth();
    const int H = (int)w.getHeight();

    // background mask
    fillRectStipple(w, 0, 0, W, H, 0, 0, 0, 1);

    // centered panel
    const int panelW = 360, panelH = 200;
    const int panelX = (W - panelW) / 2;
    const int panelY = (H - panelH) / 2;
    fillRectUI(w, panelX, panelY, panelW, panelH, 24, 24, 24);

    // title + stats
    drawText5x7(w, panelX + 24, panelY + 20, "== GAME OVER ==", 255, 220, 180, 3);
    drawText5x7(w, panelX + 24, panelY + 80, "KILLS:", 200, 255, 200, 2);
    drawNumber(w, panelX + 130, panelY + 80, kills, 200, 255, 200, 2);
    drawText5x7(w, panelX + 24, panelY + 110, "FPS:", 180, 220, 255, 2);
    drawNumber(w, panelX + 100, panelY + 110, fpsInt, 180, 220, 255, 2);

    drawText5x7(w, panelX + 24, panelY + 150, "Press ENTER / Q / ESC to quit",
        255, 240, 180, 2);

    // push once, then idle in a light input loop
    w.present();

    for (;;)
    {
        w.checkInput();
        if (w.keyPressed(VK_RETURN) || w.keyPressed(VK_ESCAPE) || w.keyPressed('Q'))
            break;
        Sleep(10);
        // Could animate title here by redrawing; static panel is simpler for coursework.
    }
}