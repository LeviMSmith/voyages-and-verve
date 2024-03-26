#pragma once

#include <map>

#include "core.h"
#include "update/entity.h"

namespace VV {
/// Chunk_Coord ///
struct Chunk_Coord {
  s32 x, y;

  bool operator<(const Chunk_Coord &b) const;
  bool operator==(const Chunk_Coord &b) const;
};

/// Cell ///
enum class Cell_Type : u8 { DIRT, AIR, WATER, GOLD, SNOW };
#define CELL_TYPE_NUM 5

struct Cell_Type_Info {
  u8 solidity;   // Used for collisions and cellular automata
  f32 friction;  // Used for slowing down an entity as it moves through or on
                 // that cell
};

extern const Cell_Type_Info CELL_TYPE_INFOS[CELL_TYPE_NUM];

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
  u8 cr, cg, cb, ca;  // Color rgba8
};

// Factory cell functions
inline Cell default_dirt_cell() {
  Cell cell;
  cell.type = Cell_Type::DIRT;
  cell.cr = 99 + std::rand() % 12;
  cell.cg = 80 + std::rand() % 12;
  cell.cb = 79 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

inline Cell default_air_cell() {
  return {
      Cell_Type::AIR,  // type
      255,             // cr
      255,             // cg
      255,             // cb
      0,               // ca
  };
}

inline Cell default_water_cell() {
  return {
      Cell_Type::WATER,  // type
      0,                 // cr
      0,                 // cg
      255,               // cb
      200,               // ca
  };
}

inline Cell default_gold_cell() {
  Cell cell;
  cell.type = Cell_Type::GOLD;
  cell.cr = 237 + std::rand() % 18;
  cell.cg = 220 + std::rand() % 20;
  cell.cb = 43 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

inline Cell default_snow_cell() {
  Cell cell;
  cell.type = Cell_Type::SNOW;
  cell.cr = 230 + std::rand() % 12;
  cell.cg = 230 + std::rand() % 12;
  cell.cb = 230 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

inline Cell default_grass_cell() {
  Cell cell;
  cell.type = Cell_Type::DIRT;
  cell.cr = 8 + std::rand() % 12;
  cell.cg = 94 + std::rand() % 12;
  cell.cb = 11 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

inline Cell default_sand_cell() {
  Cell cell;
  cell.type = Cell_Type::DIRT;
  cell.cr = 214 + std::rand() % 12;
  cell.cg = 185 + std::rand() % 12;
  cell.cb = 105 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

/*
inline Cell default_snowy_air_cell() {
  Cell cell;
  cell.type = Cell_Type::AIR;
  cell.cr = 255;
  cell.cg = 255;
  cell.cb = 255;
  cell.ca = 100;

  return cell;
}
*/

/// Chunk ///
// All cell interactions are done in chunks. This is how they're simulated,
// loaded, and generated.
constexpr u16 CHUNK_CELL_WIDTH = 64;
constexpr u16 CHUNK_CELLS = CHUNK_CELL_WIDTH * CHUNK_CELL_WIDTH;  // 4096

struct Chunk {
  Chunk_Coord coord;
  Cell cells[CHUNK_CELLS];
};

/// Surface generation ///
constexpr s32 SURFACE_Y_MAX = 7;
constexpr s32 SURFACE_Y_MIN = -5;
constexpr u16 FOREST_CELL_RANGE =
    SURFACE_Y_MAX * CHUNK_CELL_WIDTH - SURFACE_Y_MIN * CHUNK_CELL_WIDTH;

constexpr s32 SEA_WEST = -16;
constexpr s32 SEA_LEVEL = 0;
constexpr f64 SEA_LEVEL_CELL = SEA_LEVEL * CHUNK_CELL_WIDTH;

constexpr u32 GEN_TREE_MAX_WIDTH = 1500;

u16 surface_det_rand(u64 seed);
u16 interpolate_and_nudge(u16 y1, u16 y2, f64 fraction, u64 seed,
                          f64 randomness_scale, u16 cell_range);
u16 surface_height(s64 x, u16 max_depth, u32 world_seed,
                   u64 randomness_range = CHUNK_CELL_WIDTH * 64,
                   u16 cell_range = FOREST_CELL_RANGE);

// For finding out where a chunk bottom right corner is
Entity_Coord get_world_pos_from_chunk(Chunk_Coord coord);
Chunk_Coord get_chunk_coord(f64 x, f64 y);

/// Dimensions ///
enum class DimensionIndex : u8 {
  OVERWORLD,
  WATERWORLD,
};

struct Dimension {
  std::map<Chunk_Coord, Chunk> chunks;

  // Entities are all stored in Update_State, but for existance based
  // processing, we keep an index here
  std::vector<Entity_ID> entity_indicies;

  std::multimap<Entity_Z, Entity_ID> e_render;  // Entites with a texture
  std::vector<Entity_ID>
      e_kinetic;  // Entities that should be updated in the kinetic step
};

Cell *get_cell_at_world_pos(Dimension &dim, s64 x, s64 y);

}  // namespace VV
