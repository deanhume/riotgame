#pragma once

#include "Levels.h"
#include "PixelArt.h"

namespace riot
{
// Placeholder pixel art for the building the crowd is marching on. It fills the right-hand end of the street
// (x 1070..1272, y 180..686) and keeps its three windows level with the three lanes, so the goal line never moves.
void drawTarget(PixelArt& art, const TargetLook& look, float time);
}
