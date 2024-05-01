#pragma once

#include <map>
#include <set>

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
enum class Cell_Type : u8 {
  DIRT,
  AIR,
  WATER,
  GOLD,
  SNOW,
  NONE,
  STEAM,
  NICARAGUA,
  LAVA
};
#define CELL_TYPE_NUM 9

struct Cell_Type_Info {
  s16 solidity;  // Used for collisions and cellular automata
  f32 friction;  // Used for slowing down an entity as it moves through or on
                 // that cell
  f32 passive_heat;
  f32 sublimation_point;
};

extern const Cell_Type_Info CELL_TYPE_INFOS[CELL_TYPE_NUM];

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
  u8 cr, cg, cb, ca;  // Color rgba8
  s16 density;        // Used in fluid dynamics for fluids
  // s16 vx, vy;
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
      0,               // density
  };
}

inline Cell default_water_cell() {
  return {
      Cell_Type::WATER,  // type
      0x0e,              // cr
      0x0b,              // cg
      0x4c,              // cb
      200,               // ca
      10                 // density
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

inline Cell default_steam_cell() {
  Cell cell;
  cell.type = Cell_Type::STEAM;
  cell.cr = 0xaf + std::rand() % 12;
  cell.cg = 0xaf + std::rand() % 12;
  cell.cb = 0xaf + std::rand() % 12;
  cell.ca = 0x33 + std::rand() % 12;

  return cell;
}

// 540f0f
inline Cell default_nicaragua_cell(int y, int max_y) {
  Cell cell;
  cell.type = Cell_Type::NICARAGUA;

  // Base color adjusted by position to create a vertical gradient
  float factor = static_cast<float>(y) / max_y;
  int base_red =
      0x54 + static_cast<int>(11 * factor);  // Gradient effect on red

  // Random variation around the base color
  cell.cr =
      base_red + std::rand() % 6 - 3;  // Random perturbation around base_red
  cell.cg = 0x0f + std::rand() % 12;   // Green stays fully random
  cell.cb = 0x0f + std::rand() % 12;   // Blue stays fully random
  cell.ca = 255;

  return cell;
}

inline Cell default_lava_cell() {
  Cell cell;
  cell.type = Cell_Type::LAVA;

  int chance = std::rand() % 100;

  if (chance < 30) {
    // Darker lava (more black)
    cell.cr = std::rand() % 80;  // 0 to 79
    cell.cg = std::rand() % 40;  // 0 to 39
    cell.cb = std::rand() % 40;  // 0 to 39
  } else {
    // Brighter lava (red-orange)
    cell.cr = 0xc0 + std::rand() % 64;  // 192 to 255
    cell.cg = 0x40 + std::rand() % 64;  // 64 to 127
    cell.cb = std::rand() % 40;         // 0 to 39
  }

  cell.ca = 255;  // Fully opaque

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
                                                                  //
struct Chunk {
  Chunk_Coord coord;
  Cell cells[CHUNK_CELLS];
  Cell_Type all_cell;
};

enum class Biome : u8 { FOREST, ALASKA, OCEAN, NICARAGUA, DEEP_OCEAN };

/// Surface generation ///
constexpr s32 SURFACE_Y_MAX = 7;
constexpr s32 SURFACE_Y_MIN = -5;
constexpr u16 FOREST_CELL_RANGE =
    SURFACE_Y_MAX * CHUNK_CELL_WIDTH - SURFACE_Y_MIN * CHUNK_CELL_WIDTH;

constexpr s32 SEA_WEST = -16;
constexpr s32 SEA_LEVEL = 0;
constexpr f64 SEA_LEVEL_CELL = SEA_LEVEL * CHUNK_CELL_WIDTH;
constexpr s32 DEEP_SEA_LEVEL = -5;
constexpr s64 DEEP_SEA_LEVEL_CELL = DEEP_SEA_LEVEL * CHUNK_CELL_WIDTH;

constexpr u32 GEN_TREE_MAX_WIDTH = 1500;
constexpr u32 AK_GEN_TREE_MAX_WIDTH = 450;

constexpr s64 NICARAGUA_EAST_BORDER_CHUNK = -25;
constexpr s64 FOREST_EAST_BORDER_CHUNK = 25;
constexpr s64 ALASKA_EAST_BORDER_CHUNK = 50;

u16 surface_det_rand(u64 seed);
u16 interpolate_and_nudge(u16 y1, u16 y2, f64 fraction, u64 seed,
                          f64 randomness_scale, u16 cell_range);
u16 surface_height(s64 x, u16 max_depth, u32 world_seed,
                   u64 randomness_range = CHUNK_CELL_WIDTH * 64,
                   u16 cell_range = FOREST_CELL_RANGE);

// For finding out where a chunk bottom left corner is
Entity_Coord get_world_pos_from_chunk(Chunk_Coord coord);
Chunk_Coord get_chunk_coord(f64 x, f64 y);

/// Dimensions ///
enum class DimensionIndex : u8 {
  OVERWORLD,
  WATERWORLD,
};

struct Dimension {
  std::map<Chunk_Coord, Chunk> chunks;
  std::set<Entity_ID>
      entity_indicies;  // General collection of all entities in the dimension

  // Entities are all stored in Update_State, but for existance based
  // processing, we keep an index here
  std::multimap<Entity_Z, Entity_ID> e_render;  // Entites with a texture
  std::set<Entity_ID>
      e_kinetic;  // Entities that should be updated in the kinetic step
  std::set<Entity_ID>
      e_health;  // Entites that need to have their health checked
};

Cell *get_cell_at_world_pos(Dimension &dim, s64 x, s64 y);

}  // namespace VV
