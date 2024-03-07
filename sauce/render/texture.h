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
  NEITZSCHE = 5
};
}  // namespace VV
