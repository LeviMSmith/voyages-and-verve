#pragma once

#include "core.h"
#include "utils/math/vec2.h"

#include <map>

namespace YC {
enum class Cell_Type : u8 { DIRT, AIR, WATER };

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
};

// All cell interactions are done in chunks. This is how they're simulated,
// loaded, and generated.
constexpr u16 CHUNK_CELL_WIDTH = 16;
constexpr u16 CHUNK_CELLS = CHUNK_CELL_WIDTH * CHUNK_CELL_WIDTH; // 256

// All a chunk ever should be is a list of cells, but it's easier to struct
// then type that all out
struct Chunk {
  Cell cells[CHUNK_CELLS];
};

Result gen_chunk(Chunk &chunk);

// Entities will also go here in their own map
struct Dimension {
  std::map<Chunk_Coord, Chunk> chunks;
};

} // namespace YC
