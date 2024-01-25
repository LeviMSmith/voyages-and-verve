#pragma once

#include "core.h"

namespace YC {
struct Chunk_Coord {
  u32 x, y;

  // Cantor 0.5 * ()
  bool operator<(const Chunk_Coord &b);
};
} // namespace YC
