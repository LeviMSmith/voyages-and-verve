#pragma once

#include <set>
#include <unordered_set>

#include "SDL_events.h"
#include "core.h"
#include "update/entity.h"
#include "update/world.h"

namespace VV {
enum Update_Event : u8 {
  PLAYER_MOVED_CHUNK,
  CELL_CHANGE,
};

struct Update_State {
  std::vector<SDL_Event> pending_events;

  std::map<DimensionIndex, Dimension> dimensions;

  std::unordered_set<Entity_ID> entity_id_pool;
  Entity entities[MAX_TOTAL_ENTITIES];

  DimensionIndex active_dimension;  // Key of active dimension
  Entity_ID active_player;          // Index into entities

  std::set<Update_Event> events;

  u32 world_seed;

  // Render. These are duplicates so that we can do update things based on
  // render without including render headers here
  u16 screen_cell_size;
  int window_width, window_height;

  // Debug
  f32 average_fps;
};

Result init_updating(Update_State &update_state,
                     const std::optional<u32> &seed);
Result update(Update_State &update_state);

Result update_mouse(Update_State &us);
Result update_keypresses(Update_State &us);

constexpr f32 KINETIC_FRICTION = 0.8f;
constexpr f32 KINETIC_GRAVITY = 0.43f;
constexpr f32 KINETIC_TERMINAL_VELOCITY = -300.0f;
void update_kinetic(Update_State &update_state);

constexpr u8 CHUNK_CELL_SIM_RADIUS = (8 / 2) + 2;
void update_cells(Update_State &update_state);

Result gen_chunk(Update_State &update_state, DimensionIndex dim, Chunk &chunk,
                 const Chunk_Coord &chunk_coord);
Result load_chunk(Update_State &update_state, DimensionIndex dimid,
                  const Chunk_Coord &coord);
Result load_chunks_square(Update_State &update_state, DimensionIndex dimid,
                          f64 x, f64 y, u8 radius);

// This can fail! Check the result.
Result get_entity_id(Entity_ID &id);

// Factory functions. These should be used over default_entity.
Result create_entity(Update_State &us, DimensionIndex dim,
                     Entity_ID &id);  // Creates default entity and returns
                                      // index in update_state.entities

Result create_player(Update_State &us, DimensionIndex dim,
                     Entity_ID &id);  // Creates a player entity

Result create_tree(Update_State &us, DimensionIndex dim,
                   Entity_ID &id);  // This is background tree that just is
                                    // there for a position an sprite

Result create_bush(Update_State &us, DimensionIndex dim,
                   Entity_ID &id);  // This is background grass_tall that just is
                                    // there for a position an sprite

Result create_grass(Update_State &us, DimensionIndex dim,
                   Entity_ID &id);  // This is background grass_short that just is
                                    // there for a position an sprite

Result create_neitzsche(Update_State &us, DimensionIndex dim, Entity_ID &id);

// Don't hold on to these pointers too long. Additions to the vectors could
// invalidate them
Dimension *get_active_dimension(Update_State &update_state);
Entity *get_active_player(Update_State &update_state);

}  // namespace VV
