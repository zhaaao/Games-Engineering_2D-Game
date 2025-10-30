#pragma once
#include "GamesEngineeringBase.h"
void blitImage(GamesEngineeringBase::Window& window, const GamesEngineeringBase::Image& img, int dstX, int dstY);
void blitSubImage(GamesEngineeringBase::Window& window,
    const GamesEngineeringBase::Image& img,
    int srcX, int srcY, int subW, int subH,
    int dstX, int dstY);
