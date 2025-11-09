#pragma once
#include "GamesEngineeringBase.h"

/*
 * Utility: solid rectangle blit for tile/GUI primitives.
 * - Arguments are screen-space pixels.
 * - Clamps to the window bounds; no alpha blending.
 * - Intended for quick debug visualisation and simple HUD panels.
 */
void fillRect(GamesEngineeringBase::Window& canvas,
    int x0, int y0, int w, int h,
    unsigned char r, unsigned char g, unsigned char b);
void idToColor(int id, unsigned char& r, unsigned char& g, unsigned char& b);
void putPix(GamesEngineeringBase::Window& w, int x, int y,
    unsigned char r, unsigned char g, unsigned char b);
void fillRectUI(GamesEngineeringBase::Window& w, int x0, int y0, int wdt, int hgt,
    unsigned char r, unsigned char g, unsigned char b);
// 5x7 glyph: 7 rows, low 5 bits per row are pixels (MSB-left ordering below).
struct Glyph5x7 { unsigned char row[7]; };
// Minimal ASCII set used by HUD.
const Glyph5x7* getGlyph5x7(char c);
void drawChar5x7(GamesEngineeringBase::Window& w, int x, int y,
    char c, unsigned char r, unsigned char g, unsigned char b, int scale);
void drawText5x7(GamesEngineeringBase::Window& w, int x, int y,
    const char* s, unsigned char r, unsigned char g, unsigned char b, int scale, int spacing);
void drawNumber(GamesEngineeringBase::Window& w, int x, int y, int v,
    unsigned char r, unsigned char g, unsigned char b, int scale);
void fillRectStipple(GamesEngineeringBase::Window& w, int x0, int y0, int wdt, int hgt,
    unsigned char r, unsigned char g, unsigned char b, int pattern);
void drawHUD(GamesEngineeringBase::Window& w,
    int remainSec, int fpsInt, int kills);
void showGameOverScreen(GamesEngineeringBase::Window& w,
    int kills, int fpsInt);