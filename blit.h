#pragma once
#include "GamesEngineeringBase.h"

/*
 * Basic blitting utilities for 2D image rendering.
 * ------------------------------------------------
 * These functions copy raw pixel data from an Image object
 * into a Window's back buffer, without scaling or filtering.
 *
 * 1) blitImage()
 *    - Draws the entire source image at the given screen position.
 *    - (dstX, dstY) specify the top-left corner on the target window.
 *
 * 2) blitSubImage()
 *    - Draws a rectangular region (subW × subH) from the source image.
 *    - (srcX, srcY) define the region’s top-left corner inside the image.
 *    - (dstX, dstY) define where it will appear on screen.
 *
 * Notes:
 * - Coordinates are in pixels.
 * - These are direct copies (no alpha blending, no scaling).
 * - Used for sprite rendering, tile drawing, and HUD elements.
 */
void blitImage(GamesEngineeringBase::Window& window,
    const GamesEngineeringBase::Image& img,
    int dstX, int dstY);

void blitSubImage(GamesEngineeringBase::Window& window,
    const GamesEngineeringBase::Image& img,
    int srcX, int srcY, int subW, int subH,
    int dstX, int dstY);
