#include "update/update.h"

#include "SDL_mouse.h"
#include "update/world.h"

namespace VV {
Result init_updating(Update_State &update_state,
                     const std::optional<u32> &seed) {
  const DimensionIndex starting_dim = DimensionIndex::OVERWORLD;
  // const DimensionIndex starting_dim = DimensionIndex::WATERWORLD;
  update_state.dimensions.emplace(starting_dim, Dimension());
  update_state.active_dimension = starting_dim;

  if (!seed.has_value()) {
    std::srand(std::time(NULL));

    update_state.world_seed = std::rand();
  } else {
    update_state.world_seed = seed.value();
  }

  Result ap_res = create_player(update_state, update_state.active_dimension,
                                update_state.active_player);

  if (ap_res != Result::SUCCESS) {
    LOG_ERROR("Couldn't create initial player! EC: {:x}", (s32)ap_res);
    return ap_res;
  }

  Entity &active_player = *get_active_player(update_state);

  Dimension &active_dimension = *get_active_dimension(update_state);
  active_dimension.entity_indicies.push_back(update_state.active_player);

  load_chunks_square(update_state, update_state.active_dimension,
                     active_player.coord.x, active_player.coord.y, 8 / 2);

  return Result::SUCCESS;
}

Result update(Update_State &update_state) {
  Entity &active_player = *get_active_player(update_state);
  static Chunk_Coord last_player_chunk =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

  update_mouse(update_state);
  Result res = update_keypresses(update_state);
  if (res == Result::WINDOW_CLOSED) {
    LOG_INFO("Got close from keyboard");
    return res;
  }
  update_kinetic(update_state);
  Chunk_Coord current_player_chunk =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

  if (last_player_chunk != current_player_chunk) {
    /*
    LOG_DEBUG("Player moved to cell chunk {} {}", current_player_chunk.x,
              current_player_chunk.y);
    */
    update_state.events.insert(Update_Event::PLAYER_MOVED_CHUNK);
    load_chunks_square(update_state, update_state.active_dimension,
                       active_player.coord.x, active_player.coord.y, 8 + 5);
    last_player_chunk = current_player_chunk;
  }

  update_cells(update_state);

  return Result::SUCCESS;
}

Result update_mouse(Update_State &us) {
  int mouse_x, mouse_y;
  Uint32 button_state = SDL_GetMouseState(&mouse_x, &mouse_y);

  u16 screen_cell_size = us.screen_cell_size;
  Entity &active_player = *get_active_player(us);
  Dimension &active_dimension = *get_active_dimension(us);

  Entity_Coord tl;
  tl.x = active_player.camx + active_player.coord.x;
  tl.x -= (us.window_width / 2.0f) / screen_cell_size;

  tl.y = active_player.camy + active_player.coord.y;
  tl.y += (us.window_height / 2.0f) / screen_cell_size;

  if (SDL_BUTTON(button_state) == SDL_BUTTON_LEFT) {
    Entity_Coord c;
    c.x = static_cast<s32>(mouse_x / us.screen_cell_size) + tl.x;
    c.y = tl.y - static_cast<s32>(mouse_y / us.screen_cell_size);

    Chunk_Coord cc = get_chunk_coord(c.x, c.y);

    Chunk &chunk = active_dimension.chunks[cc];
    u16 cx = std::abs((cc.x * CHUNK_CELL_WIDTH) - c.x);
    u16 cy = c.y - (cc.y * CHUNK_CELL_WIDTH);
    u32 cell_index = cx + cy * CHUNK_CELL_WIDTH;

    assert(cell_index < CHUNK_CELLS);

    chunk.cells[cell_index] = default_gold_cell();

    us.events.emplace(Update_Event::CELL_CHANGE);
  }

  return Result::SUCCESS;
}

Result update_keypresses(Update_State &us) {
  static int num_keys;

  const Uint8 *keys = SDL_GetKeyboardState(&num_keys);

  Entity &active_player = *get_active_player(us);

  static constexpr f32 MOVEMENT_CONSTANT = 0.4f;
  static constexpr f32 SWIM_CONSTANT = 0.025f;
  static constexpr f32 MOVEMENT_JUMP_ACC = 4.5f;
  static constexpr f32 MOVEMENT_JUMP_VEL = -1.0f * (KINETIC_GRAVITY + 1.0f);
  static constexpr f32 MOVEMENT_ACC_LIMIT = 1.0f;
  static constexpr f32 MOVEMENT_ACC_LIMIT_NEG = MOVEMENT_ACC_LIMIT * -1.0;
  // static constexpr f32 MOVEMENT_ON_GROUND_MULT = 4.0f;

  // Movement
  if (keys[SDL_SCANCODE_W] == 1 || keys[SDL_SCANCODE_UP] == 1) {
    if (active_player.ay < MOVEMENT_ACC_LIMIT + KINETIC_GRAVITY &&
        active_player.status & (u8)Entity_Status::ON_GROUND &&
        !(active_player.status & (u8)Entity_Status::IN_WATER)) {
      active_player.ay += MOVEMENT_JUMP_ACC;
      active_player.vy += MOVEMENT_JUMP_VEL;
    }
    if (active_player.ay < MOVEMENT_ACC_LIMIT &&
        active_player.status & (u8)Entity_Status::IN_WATER) {
      active_player.ay += SWIM_CONSTANT;
    }
    // active_player.status = active_player.status &
    // ~(u8)Entity_Status::ON_GROUND;
  }
  if (keys[SDL_SCANCODE_A] == 1 || keys[SDL_SCANCODE_LEFT] == 1) {
    if (active_player.ax > MOVEMENT_ACC_LIMIT_NEG) {
      if (active_player.status & (u8)Entity_Status::IN_WATER) {
        active_player.ax -= SWIM_CONSTANT;
      } else {
        active_player.ax -= MOVEMENT_CONSTANT;
      }
      // if (active_player.on_ground) {
      //   active_player.ax *= MOVEMENT_ON_GROUND_MULT;
      // }
    }
    active_player.flipped = true;
  }
  if (keys[SDL_SCANCODE_S] == 1 || keys[SDL_SCANCODE_DOWN] == 1) {
    if (active_player.ay > MOVEMENT_ACC_LIMIT_NEG - KINETIC_GRAVITY) {
      if (active_player.status & (u8)Entity_Status::IN_WATER) {
        active_player.ay -= SWIM_CONSTANT;
      } else {
        active_player.ay -= MOVEMENT_CONSTANT;
      }
    }
  }
  if (keys[SDL_SCANCODE_D] == 1 || keys[SDL_SCANCODE_RIGHT] == 1) {
    if (active_player.ax < MOVEMENT_ACC_LIMIT) {
      if (active_player.status & (u8)Entity_Status::IN_WATER) {
        active_player.ax += SWIM_CONSTANT;
      } else {
        active_player.ax += MOVEMENT_CONSTANT;
      }
      // if (active_player.on_ground) {
      //   active_player.ax *= MOVEMENT_ON_GROUND_MULT;
      // }
    }
    active_player.flipped = false;
  }

  // Quit
  if (keys[SDL_SCANCODE_Q] == 1) {
    return Result::WINDOW_CLOSED;
  }

  return Result::SUCCESS;
}

void update_kinetic(Update_State &update_state) {
  Dimension &active_dimension = *get_active_dimension(update_state);

  // TODO: Multithread

  // Start by updating kinetics values: acc, vel, pos
  for (Entity_ID entity_index : active_dimension.e_kinetic) {
    // Have to trust that entity_indicies is correct at them moment.
    Entity &entity = update_state.entities[entity_index];

    if (entity.status & (u8)Entity_Status::IN_WATER) {
      entity.ax *= CELL_TYPE_INFOS[(u8)Cell_Type::WATER].friction;
      entity.ay *= CELL_TYPE_INFOS[(u8)Cell_Type::WATER].friction;
      entity.vx *= CELL_TYPE_INFOS[(u8)Cell_Type::WATER].friction;
      entity.vy *= CELL_TYPE_INFOS[(u8)Cell_Type::WATER].friction;

      entity.vy += entity.bouyancy;
    } else if (entity.status & (u8)Entity_Status::ON_GROUND) {
      entity.ax *= CELL_TYPE_INFOS[(u8)Cell_Type::DIRT].friction;
      entity.ay *= CELL_TYPE_INFOS[(u8)Cell_Type::DIRT].friction;
      entity.vx *= CELL_TYPE_INFOS[(u8)Cell_Type::DIRT].friction;
      entity.vy *= CELL_TYPE_INFOS[(u8)Cell_Type::DIRT].friction;
    } else {
      entity.ax *= CELL_TYPE_INFOS[(u8)Cell_Type::AIR].friction;
      entity.ay *= CELL_TYPE_INFOS[(u8)Cell_Type::AIR].friction;
      entity.vx *= CELL_TYPE_INFOS[(u8)Cell_Type::AIR].friction;
      entity.vy *= CELL_TYPE_INFOS[(u8)Cell_Type::AIR].friction;
    }

    entity.vx += entity.ax;
    entity.vy += entity.ay;
    if (entity.vy > KINETIC_TERMINAL_VELOCITY) {
      entity.vy -= KINETIC_GRAVITY;
    }
    entity.coord.x += entity.vx;
    entity.coord.y += entity.vy;
  }

  // Now resolve colisions

  // TODO: this just does cell collisions. We need some kind of spacial data
  // structure to determine if we're colliding with other entities
  for (Entity_ID entity_index : active_dimension.e_kinetic) {
    Entity &entity = update_state.entities[entity_index];
    entity.status = 0;

    Chunk_Coord cc = get_chunk_coord(entity.coord.x, entity.coord.y);

    // TODO: Should also definitly multithread this

    // Right now just checking the chunks below and to the right since that's
    // where the bounding box might spill over. If an entity every becomes
    // larger than a chunk, we'll have to do this range based on that

    // This is bad. It's going to take forever. Definitly have to only do this
    // on entities that absolutly need it.
    Chunk_Coord ic = cc;
    for (ic.x = cc.x; ic.x < cc.x + 2; ic.x++) {
      for (ic.y = cc.y; ic.y > cc.y - 2; ic.y--) {
        Chunk &chunk = active_dimension.chunks[ic];

        for (u32 cell = 0; cell < CHUNK_CELLS; cell++) {
          // Bounding box colision between the entity and the cell

          Entity_Coord cell_coord;
          cell_coord.x = ic.x * CHUNK_CELL_WIDTH;
          cell_coord.y = ic.y * CHUNK_CELL_WIDTH;

          u8 x = cell % CHUNK_CELL_WIDTH;
          cell_coord.x += x;
          cell_coord.y += static_cast<s32>(
              (cell - x) / CHUNK_CELL_WIDTH);  // -x is probably unecessary.

          if (entity.coord.x + entity.boundingw < cell_coord.x ||
              cell_coord.x + 1 < entity.coord.x) {
            continue;
          }

          if (entity.coord.y - entity.boundingh > cell_coord.y ||
              cell_coord.y > entity.coord.y) {
            continue;
          }

          // If neither, we are coliding, and resolve based on cell
          switch (chunk.cells[cell].type) {
            case Cell_Type::GOLD:
            case Cell_Type::DIRT: {
              if (entity.coord.y - entity.boundingh <= cell_coord.y) {
                entity.status = entity.status | (u8)Entity_Status::ON_GROUND;
              }

              // With solid cells, we also don't allow the entity to intersect
              // the cell
              f32 overlap_x, overlap_y;

              // For X axis
              if (entity.coord.x < cell_coord.x) {
                overlap_x = (entity.coord.x + entity.boundingw) - cell_coord.x;
              } else {
                overlap_x = (cell_coord.x + 1) - entity.coord.x;
              }

              // For Y axis
              if (entity.coord.y > cell_coord.y) {
                overlap_y = cell_coord.y - (entity.coord.y - entity.boundingh);
              } else {
                overlap_y = (cell_coord.y + 1) - entity.coord.y;
              }

              // Determine the smallest overlap to resolve the collision with
              static constexpr f64 MOV_LIM = 0.95;
              if (fabs(overlap_x) < fabs(overlap_y)) {
                if (entity.coord.x < cell_coord.x) {
                  entity.coord.x -=
                      std::min(static_cast<double>(fabs(overlap_x)), MOV_LIM);
                  // Move entity left
                } else {
                  entity.coord.x +=
                      std::min(static_cast<double>(fabs(overlap_x)),
                               MOV_LIM);  // Move entity right
                }
              } else {
                if (entity.coord.y > cell_coord.y) {
                  entity.coord.y +=
                      std::min(static_cast<double>(fabs(overlap_y)),
                               MOV_LIM);  // Move entity down
                } else {
                  entity.coord.y -=
                      std::min(static_cast<double>(fabs(overlap_y)),
                               MOV_LIM);  // Move entity up
                }
              }
              break;
            }
            case Cell_Type::AIR: {
              break;
            }
            case Cell_Type::WATER: {
              entity.status = entity.status | (u8)Entity_Status::IN_WATER;
              break;
            }
          }

          // entity.ax *= 0.1f;
          // entity.ay *= 0.1f;
          // entity.vx *= 0.5f;
          // entity.vy *= 0.5f;
        }
      }
    }
  }
}

void update_cells(Update_State &update_state) {
  Entity &active_player = *get_active_player(update_state);
  Dimension &dim = *get_active_dimension(update_state);

  Chunk_Coord player_chunkc =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

  Chunk_Coord bl;
  bl.x = player_chunkc.x - CHUNK_CELL_SIM_RADIUS;
  bl.y = player_chunkc.y - CHUNK_CELL_SIM_RADIUS;

  Chunk_Coord ic;

  // Not existance based processing. Probably won't ever be.
  for (ic.x = bl.x; ic.x < bl.x + CHUNK_CELL_SIM_RADIUS * 2; ic.x++) {
    for (ic.y = bl.y; ic.y < bl.y + CHUNK_CELL_SIM_RADIUS * 2; ic.y++) {
      Chunk &chunk = dim.chunks[ic];

      for (u32 cell_index = 0; cell_index < CHUNK_CELLS; cell_index++) {
        switch (chunk.cells[cell_index].type) {
          case Cell_Type::GOLD: {
            s64 cx = chunk.coord.x * CHUNK_CELL_WIDTH +
                     static_cast<s64>(cell_index % CHUNK_CELL_WIDTH);
            s64 cy = chunk.coord.y * CHUNK_CELL_WIDTH +
                     static_cast<s64>(cell_index / CHUNK_CELL_WIDTH);

            Cell *below = get_cell_at_world_pos(dim, cx, cy - 1);

            if (CELL_TYPE_INFOS[(u8)below->type].solidity < 200) {
              std::swap(chunk.cells[cell_index], *below);
              break;
            }

            // Which side to try first
            s8 dir = std::rand() % 2;
            if (dir == 0) {
              dir = -1;
            }

            Cell *side_cell = get_cell_at_world_pos(dim, cx + dir, cy - 1);
            if (CELL_TYPE_INFOS[(u8)side_cell->type].solidity < 200) {
              std::swap(chunk.cells[cell_index], *side_cell);
              break;
            }

            if (dir == 1) {
              dir = -1;
            }
            if (dir == -1) {
              dir = 1;
            }

            side_cell = get_cell_at_world_pos(dim, cx + dir, cy - 1);
            if (CELL_TYPE_INFOS[(u8)side_cell->type].solidity < 200) {
              std::swap(chunk.cells[cell_index], *side_cell);
              break;
            }

            break;
          }
          default:
            break;
        }
      }
    }
  }
}

Result gen_chunk(Update_State &update_state, DimensionIndex dim, Chunk &chunk,
                 const Chunk_Coord &chunk_coord) {
  // This works by zones. Every zone that a chunk is part of generates based
  // on that chunk and then is overwritten by the next zone a chunk is part of

  // Biomes will be dynamically generated zones. Likely when implemented there
  // will be a get biome function which will then do the base generation of a
  // chunk

  // There can also be predefined structures that will be the last zone so
  // that if the developer say there's a house here, there will be a house
  // there.

  /// Zones ///

  // Biomes by explicit positioning
  switch (dim) {
    case DimensionIndex::OVERWORLD: {
      for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
        f64 abs_x = x + chunk_coord.x * CHUNK_CELL_WIDTH;
        u8 grass_depth = 40 + surface_det_rand(static_cast<u64>(abs_x) ^
                                               update_state.world_seed) %
                                  25;
        s32 height = static_cast<s32>(
                         surface_height(x + chunk_coord.x * CHUNK_CELL_WIDTH,
                                        64, update_state.world_seed)) +
                     SURFACE_Y_MIN * CHUNK_CELL_WIDTH;
        for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
          u16 cell_index = x + (y * CHUNK_CELL_WIDTH);

          s32 our_height = (y + (chunk_coord.y * CHUNK_CELL_WIDTH));
          if (height < SEA_LEVEL_CELL && our_height <= height) {
            chunk.cells[cell_index] = default_sand_cell();
          } else if (height < SEA_LEVEL_CELL && our_height > height &&
                     our_height < SEA_LEVEL_CELL) {
            chunk.cells[cell_index] = default_water_cell();
          } else if (our_height < height &&
                     our_height >= height - grass_depth) {
            chunk.cells[cell_index] = default_grass_cell();
          } else if (our_height < height - grass_depth) {
            chunk.cells[cell_index] = default_dirt_cell();
          } else {
            chunk.cells[cell_index] = default_air_cell();
          }
        }


        //added distance between tree's to prevent overlap
        if (surface_det_rand(static_cast<u64>(abs_x) ^ update_state.world_seed) % GEN_TREE_MAX_WIDTH < 15 &&
            height > chunk_coord.y * CHUNK_CELL_WIDTH &&
            height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
            height >= SEA_LEVEL_CELL) {
            // Check if a tree already exists at this location
            bool locationFree = true;
            for (const auto& entity_id : update_state.dimensions[update_state.active_dimension].entity_indicies) {
                const Entity& existingEntity = update_state.entities[entity_id];

                if (existingEntity.texture == Texture_Id::TREE &&
                    std::abs(existingEntity.coord.x - abs_x) <100) {  // 100 distance between tree's
                    locationFree = false;
                    break;
                }
            }

            if (locationFree) {
                Entity_ID id;
                create_tree(update_state, update_state.active_dimension, id);

                Entity &tree = update_state.entities[id];
                tree.coord.x = abs_x;
                tree.coord.y = height + 85.0f;  // This assumes tree base height doesn't affect spawn logic
            }
        }

        // Unified spawner for bush and grass
        if (surface_det_rand(static_cast<u64>(abs_x) ^ update_state.world_seed) % GEN_TREE_MAX_WIDTH < 15 &&
            height > chunk_coord.y * CHUNK_CELL_WIDTH &&
            height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
            height >= SEA_LEVEL_CELL) {
            
            bool locationFreeForBush = true;
            bool locationFreeForGrass = true;

            bool tryBushFirst = (std::rand() % 2) == 0;

            for (const auto& entity_id : update_state.dimensions[update_state.active_dimension].entity_indicies) {
                const Entity& existingEntity = update_state.entities[entity_id];

                if (existingEntity.texture == Texture_Id::BUSH &&
                    std::abs(existingEntity.coord.x - abs_x) < 15) {  // Check for existing bush
                    locationFreeForBush = false;
                }

                if (existingEntity.texture == Texture_Id::GRASS &&
                    std::abs(existingEntity.coord.x - abs_x) < 10) {  // Check for existing grass
                    locationFreeForGrass = false;
                }

                // If location is not free for either, stop checking
                if (!locationFreeForBush && !locationFreeForGrass) {
                    break;
                }
            }

            if (tryBushFirst) {
                if (locationFreeForBush) {
                Entity_ID id;
                create_bush(update_state, update_state.active_dimension, id);

                Entity &bush = update_state.entities[id];
                bush.coord.x = abs_x;
                bush.coord.y = height + 20.0f;
                }

                // Spawn grass if location is free
                else if (locationFreeForGrass) {
                    Entity_ID id;
                    create_grass(update_state, update_state.active_dimension, id);

                    Entity &grass = update_state.entities[id];
                    grass.coord.x = abs_x;
                    grass.coord.y = height + 10.0f;
                }
            } else {
                // Spawn grass if location is free
                if (locationFreeForGrass) {
                    Entity_ID id;
                    create_grass(update_state, update_state.active_dimension, id);

                    Entity &grass = update_state.entities[id];
                    grass.coord.x = abs_x;
                    grass.coord.y = height + 10.0f;
                }

                // Spawn bush if location is free
                else if (locationFreeForBush) {
                    Entity_ID id;
                    create_bush(update_state, update_state.active_dimension, id);

                    Entity &bush = update_state.entities[id];
                    bush.coord.x = abs_x;
                    bush.coord.y = height + 20.0f;
                }
            }
        }


        //neitzsche spawner
        if (abs_x == 250 && height > chunk_coord.y * CHUNK_CELL_WIDTH &&
            height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
            height >= SEA_LEVEL_CELL) {
          Entity_ID id;
          create_neitzsche(update_state, update_state.active_dimension, id);

          Entity &neitzsche = update_state.entities[id];
          neitzsche.coord.x = abs_x;
          neitzsche.coord.y = height + 85.0f;
        }
      }

      break;
    }
    case DimensionIndex::WATERWORLD: {
      const Cell base_air = default_air_cell();
      if (chunk_coord.y > SEA_LEVEL) {
        for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
          for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
            chunk.cells[x + y * CHUNK_CELL_WIDTH] = base_air;
          }
        }
      } else {
        for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
          for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
            chunk.cells[x + y * CHUNK_CELL_WIDTH] = default_water_cell();
          }
        }
      }
      break;
    }
  }

  chunk.coord = chunk_coord;

  return Result::SUCCESS;
}

Result load_chunk(Update_State &update_state, DimensionIndex dimid,
                  const Chunk_Coord &coord) {
  Dimension &dim = update_state.dimensions[dimid];
  if (dim.chunks.find(coord) == dim.chunks.end()) {
    gen_chunk(update_state, dimid, dim.chunks[coord], coord);
  }
  // Eventually we'll also load from disk

  return Result::SUCCESS;
}

Result load_chunks_square(Update_State &update_state, DimensionIndex dimid,
                          f64 x, f64 y, u8 radius) {
  Chunk_Coord origin = get_chunk_coord(x, y);

  Chunk_Coord icc;
  for (icc.x = origin.x - radius; icc.x < origin.x + radius; icc.x++) {
    for (icc.y = origin.y - radius; icc.y < origin.y + radius; icc.y++) {
      load_chunk(update_state, dimid, icc);
    }
  }

  return Result::SUCCESS;
}

Result get_entity_id(std::unordered_set<Entity_ID> &entity_id_pool,
                     Entity_ID &id) {
  static Entity_ID current_entity = 1;
  static std::pair<std::unordered_set<Entity_ID>::iterator, bool> result;

  Entity_ID start_entity = current_entity;
  do {
    result = entity_id_pool.insert(current_entity);
    if (result.second) {
      id = current_entity;
      return Result::SUCCESS;
    }

    current_entity = (current_entity + 1) % MAX_TOTAL_ENTITIES;
    if (current_entity == 0) {
      ++current_entity;
    }  // Skip 0
  } while (current_entity != start_entity);

  LOG_WARN("Failed to get entity id. Pool full.");
  return Result::ENTITY_POOL_FULL;
}

Result create_entity(Update_State &us, DimensionIndex dim, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    return id_res;
  }

  us.entities[id] = default_entity();
  us.dimensions[dim].entity_indicies.push_back(id);

  return Result::SUCCESS;
}

Result create_player(Update_State &us, DimensionIndex dim, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    return id_res;
  }

  Entity &player = us.entities[id];
  player = default_entity();

  player.texture = Texture_Id::PLAYER;
  player.zdepth = 9;
  player.bouyancy = KINETIC_GRAVITY - 0.01f;

  player.coord.y = CHUNK_CELL_WIDTH * SURFACE_Y_MAX;
  player.coord.x = 15;
  player.camy -= 20;
  player.vy = -1.1;

  // TODO: Should be in a resource description file. This will be different
  // than texture width and height. Possibly with offsets. Maybe multiple
  // bounding boxes
  player.boundingw = 11;
  player.boundingh = 29;

  us.dimensions[dim].entity_indicies.push_back(id);
  us.dimensions[dim].e_render.emplace(player.zdepth, id);
  us.dimensions[dim].e_kinetic.push_back(id);

  return Result::SUCCESS;
}

Result create_tree(Update_State &us, DimensionIndex dim, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    return id_res;
  }

  Entity &tree = us.entities[id];

  tree.texture = Texture_Id::TREE;
  tree.zdepth = -10;

  us.dimensions[dim].entity_indicies.push_back(id);
  us.dimensions[dim].e_render.emplace(tree.zdepth, id);

  return Result::SUCCESS;
}

Result create_bush(Update_State &us, DimensionIndex dim, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    return id_res;
  }

  Entity &bush = us.entities[id];

  bush.texture = Texture_Id::BUSH;
  bush.zdepth = -10;

  us.dimensions[dim].entity_indicies.push_back(id);
  us.dimensions[dim].e_render.emplace(bush.zdepth, id);

  return Result::SUCCESS;
}

Result create_grass(Update_State &us, DimensionIndex dim, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    return id_res;
  }

  Entity &grass = us.entities[id];

  grass.texture = Texture_Id::GRASS;
  grass.zdepth = -10;

  us.dimensions[dim].entity_indicies.push_back(id);
  us.dimensions[dim].e_render.emplace(grass.zdepth, id);

  return Result::SUCCESS;
}

Result create_neitzsche(Update_State &us, DimensionIndex dim, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    return id_res;
  }

  Entity &entity = us.entities[id];

  entity.texture = Texture_Id::NEITZSCHE;
  entity.flipped = true;
  entity.zdepth = 10;

  entity.anim_width = 25;
  entity.anim_frames = 2;
  entity.anim_delay = 20;  // 20 frames
  entity.status = entity.status | (u8)Entity_Status::ANIMATED;

  us.dimensions[dim].entity_indicies.push_back(id);
  us.dimensions[dim].e_render.emplace(entity.zdepth, id);

  return Result::SUCCESS;
}

inline Dimension *get_active_dimension(Update_State &update_state) {
#ifndef NDEBUG
  auto dim_iter = update_state.dimensions.find(update_state.active_dimension);
  assert(dim_iter != update_state.dimensions.end());
  return &dim_iter->second;
#else
  return &update_state.dimensions.at(update_state.active_dimension);
#endif
}

inline Entity *get_active_player(Update_State &update_state) {
  return &update_state.entities[update_state.active_player];
}
}  // namespace VV
