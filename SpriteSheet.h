#pragma once
#include "GamesEngineeringBase.h"
#include "blit.h"
using namespace GamesEngineeringBase;

/***************************  SpriteSheet: extract frame (row, col)  ***************************
 * Handles sub-image selection within a sprite sheet.
 * Convention:
 *   row 0 = Down, 1 = Right, 2 = Left, 3 = Up
 *   col 0..3 = animation columns (per direction)
 *
 * Designed to work with any number of rows (≥4) and a fixed 4-column layout,
 * but row/column indices are generic for future flexibility.
 ***********************************************************************************************/
class SpriteSheet
{
private:
    GamesEngineeringBase::Image image; // Full source sheet
    int frameW = 0, frameH = 0;        // Dimensions of a single frame
    int rows = 0, cols = 0;            // Total rows and columns (cols usually = 4)

public:
    // Loads the entire sheet and stores its grid metadata.
    // fw, fh define the dimensions of each frame.
    // Returns false if file loading fails or any parameter is invalid.
    bool load(const char* filename, int fw, int fh, int _rows, int _cols)
    {
        if (!image.load(std::string(filename))) return false;
        frameW = fw; frameH = fh; rows = _rows; cols = _cols;
        // Basic sanity checks to prevent division or overflow issues
        if (frameW <= 0 || frameH <= 0) return false;
        if (rows <= 0 || cols <= 0) return false;
        return true;
    }

    // Renders a single frame (row, col) to the screen at (dstX, dstY).
    // The coordinates specify screen-space position (top-left of the frame).
    void drawFrame(Window& w, int row, int col, int dstX, int dstY) const
    {
        // Validate bounds before drawing
        if (row < 0 || row >= rows) return;
        if (col < 0 || col >= cols) return;

        // Compute source rectangle inside the sprite sheet
        int sx = col * frameW;
        int sy = row * frameH;

        // Copy sub-image into window buffer
        blitSubImage(w, image, sx, sy, frameW, frameH, dstX, dstY);
    }

    // Accessors for frame size
    int getFrameW() const { return frameW; }
    int getFrameH() const { return frameH; }

    // Checks whether the sheet has valid pixel data
    bool valid() const { return image.data != nullptr; }
};
