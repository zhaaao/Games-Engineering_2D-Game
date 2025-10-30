#pragma once
#include "GamesEngineeringBase.h"
#include "blit.h"
using namespace GamesEngineeringBase;

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
