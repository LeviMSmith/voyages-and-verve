#pragma once

#include "SDL_render.h"
#include "core.h"

namespace VV {
struct Res_Texture {
  s32 width, height;
  SDL_Texture *texture;
};

enum class Texture_Id : u8 {
  NONE = 0,
  PLAYER = 1,
  SKY = 2,
  TREE = 3,
  MOUNTAINS = 4,
  NEITZSCHE = 5,
  BUSH = 6,
  GRASS = 7,
  ALASKA_BG = 8,
  AKTREE1 = 9,
  AKTREE2 = 0x0a,
};
}  // namespace VV
