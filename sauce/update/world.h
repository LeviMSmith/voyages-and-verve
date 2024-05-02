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
#define MAX_CELL_TYPES 1000
enum class Cell_Type : u16 {
  DIRT,
  AIR,
  WATER,
  GOLD,
  SNOW,
  NONE,
  STEAM,
  NICARAGUA,
  LAVA,
  SAND,
  GRASS
};

struct Cell_Type_Info {
  s16 solidity;  // Used for collisions and cellular automata
  f32 friction;  // Used for slowing down an entity as it moves through or on
                 // that cell
  f32 passive_heat;
  f32 sublimation_point;
  Cell_Type sublimation_cell;
  u8 viscosity;
};

extern Cell_Type_Info cell_type_infos[MAX_CELL_TYPES];

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
  u8 cr, cg, cb, ca;  // Color rgba8
};

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
