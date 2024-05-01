#include "update/world.h"

namespace VV {
bool Chunk_Coord::operator<(const Chunk_Coord &other) const {
  return x < other.x || (x == other.x && y < other.y);
}

bool Chunk_Coord::operator==(const Chunk_Coord &b) const {
  return (x == b.x) && (y == b.y);
}

// sublimation_points of -1.0f mean it cannot sublimate.
const Cell_Type_Info CELL_TYPE_INFOS[CELL_TYPE_NUM] = {
    {
        255,              // solidity
        0.70f,            // friction
        -20.0f,           // passive_heat
        3000.0f,          // sublimation_point
        Cell_Type::LAVA,  // sublimation_cell
        0,                // viscosity
    },                    // DIRT
    {
        -100,             // solidity
        0.8f,             // friction
        0.0,              // passive_heat
        -1.0,             // sublimation_point
        Cell_Type::NONE,  // sublimation_cell
        7,                // viscosity
    },                    // AIR
    {
        -20,               // solidity
        0.90f,             // friction
        -80.0,             // passive_heat
        100.0,             // sublimation_point
        Cell_Type::STEAM,  // sublimation_cell
        7,                 // viscosity
    },                     // WATER
    {
        255,              // solidity
        0.70f,            // friction
        -10.0,            // passive_heat
        10000.0,          // sublimation_point
        Cell_Type::LAVA,  // sublimation_cell
        0,                // viscosity
    },                    // GOLD
    {
        200,               // solidity
        0.70f,             // friction
        -90.0f,            // passive_heat
        0.0f,              // sublimation_point
        Cell_Type::WATER,  // sublimation_cell
        0,                 // viscosity
    },                     // SNOW
    {
        0, 1.0f,
        0.0f,             // passive_heat
        -1.0f,            // sublimation_point
        Cell_Type::NONE,  // sublimation_cell
        0,                // viscosity
    },                    // NONE
    {
        -50,              // solidity
        0.8f,             // friction
        50.0f,            // passive_heat
        -1.0f,            // sublimation_point
        Cell_Type::NONE,  // sublimation_cell
        4,                // viscosity
    },                    // STEAM
    {
        200,              // solidity
        0.8f,             // friction
        150.0f,           // passive_heat
        10000.0f,         // sublimation_point
        Cell_Type::LAVA,  // sublimation_cell
        0,                // viscosity
    },                    // NICARAGUA
    {
        10,                // solidity
        0.9f,              // friction
        200.0f,            // passive_heat
        1000.0f,           // sublimation_point
        Cell_Type::STEAM,  // sublimation_cell
        2,                 // viscosity
    }                      // LAVA
};

u16 surface_det_rand(u64 seed) {
  seed = (~seed) + (seed << 21);  // input = (input << 21) - input - 1;
  seed = seed ^ (seed >> 24);
  seed = (seed + (seed << 3)) + (seed << 8);  // seed * 265
  seed = seed ^ (seed >> 14);
  seed = (seed + (seed << 2)) + (seed << 4);  // seed * 21
  seed = seed ^ (seed >> 28);
  seed = seed + (seed << 31);

  return static_cast<u16>((seed >> 16) ^ (seed & 0xFFFF));
}

u16 interpolate_and_nudge(u16 y1, u16 y2, f64 fraction, u64 seed,
                          f64 randomness_scale, u16 cell_range) {
  u16 height = y1 + static_cast<u16>((y2 - y1) * fraction);

  f64 divisor = cell_range * randomness_scale;
  s16 divisor_as_int = static_cast<s16>(divisor);

  if (divisor_as_int != 0) {
    u16 rand = surface_det_rand(seed);
    s16 nudge = static_cast<s16>(rand) % divisor_as_int;
    height += nudge;
  }

  return std::min(cell_range, height);
}

u16 surface_height(s64 x, u16 max_depth, u32 world_seed, u64 randomness_range,
                   u16 cell_range) {
  static std::map<s64, u16> heights;

  auto height_iter = heights.find(x);
  if (height_iter != heights.end()) {
    return height_iter->second;
  }

  if (x % randomness_range == 0) {
    u16 height =
        surface_det_rand(static_cast<u64>(x ^ world_seed)) % cell_range;
    heights[x] = height;
    return height;
  }

  s64 lower_x = static_cast<s64>(x / randomness_range) * randomness_range;
  s64 upper_x = lower_x + randomness_range;

  u16 lower_height = surface_height(lower_x, 1, world_seed);
  u16 upper_height = surface_height(upper_x, 1, world_seed);

  for (u16 depth = 0; depth < max_depth; depth++) {
    s64 x_mid = static_cast<s64>(lower_x + upper_x) / 2;

    u16 y_mid;
    auto height_iter = heights.find(x_mid);
    if (height_iter != heights.end()) {
      y_mid = height_iter->second;
    } else {
      y_mid = interpolate_and_nudge(lower_height, upper_height, 0.5,
                                    static_cast<u64>(x ^ world_seed),
                                    0.5 / std::pow(depth, 2.5), cell_range);
      heights[x_mid] = y_mid;
    }

    if (x == x_mid) {
      return y_mid;
    } else if (x < x_mid) {
      upper_x = x_mid;
      upper_height = y_mid;
    } else {
      lower_x = x_mid;
      lower_height = y_mid;
    }
  }

  f64 fraction = static_cast<f64>(x - lower_x) / (upper_x - lower_x);
  u16 height = interpolate_and_nudge(
      lower_height, upper_height, fraction, static_cast<u64>(x ^ world_seed),
      0.5 / std::pow(max_depth, 2.5), cell_range);
  heights[x] = height;
  return height;
}

Entity_Coord get_world_pos_from_chunk(Chunk_Coord coord) {
  Entity_Coord ret_entity_coord;

  ret_entity_coord.x = coord.x * CHUNK_CELL_WIDTH;
  ret_entity_coord.y = coord.y * CHUNK_CELL_WIDTH;

  return ret_entity_coord;
}

Chunk_Coord get_chunk_coord(f64 x, f64 y) {
  Chunk_Coord return_chunk_coord;

  x += 0.02;
  y += 0.02;

  return_chunk_coord.x = static_cast<s32>(x / CHUNK_CELL_WIDTH);
  return_chunk_coord.y = static_cast<s32>(y / CHUNK_CELL_WIDTH);

  if (x < 0) {
    return_chunk_coord.x -= 1;
  }

  if (y < 0) {
    return_chunk_coord.y -= 1;
  }

  return return_chunk_coord;
}

Cell *get_cell_at_world_pos(Dimension &dim, s64 x, s64 y) {
  Chunk_Coord cc = get_chunk_coord(x, y);

  u32 cell_x = ((x % CHUNK_CELL_WIDTH) + CHUNK_CELL_WIDTH) % CHUNK_CELL_WIDTH;
  u32 cell_y = ((y % CHUNK_CELL_WIDTH) + CHUNK_CELL_WIDTH) % CHUNK_CELL_WIDTH;

  u32 cell_index = cell_x + cell_y * CHUNK_CELL_WIDTH;

  return &(dim.chunks[cc].cells[cell_index]);
}
}  // namespace VV
