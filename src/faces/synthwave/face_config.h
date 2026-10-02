#pragma once
// Geometria tarczy synthwave (480x320)

#include "config.h"

constexpr int DATE_TOP    = 0;     // pas daty: y 0-40
constexpr int DATE_BOTTOM = 40;
constexpr int HORIZON_Y   = 220;   // horyzont, srodek slonca
constexpr int GRID_TOP    = HORIZON_Y;
constexpr int GRID_H      = SCREEN_H - GRID_TOP;  // siatka y 220-320
constexpr int SIDE_W      = 75;    // boki na animacje (poza pasem cyfr)
