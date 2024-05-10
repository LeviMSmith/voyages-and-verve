#include "update/update.h"

#include <fstream>
#include <optional>
#include <regex>

#include "SDL_mouse.h"
#include "rapidjson/document.h"
#include "render/texture.h"
#include "update/ai.h"
#include "update/entity.h"
#include "update/world.h"
#include "utils/config.h"
#include "utils/datastructures.h"
#include "utils/threadpool.h"

namespace rj = rapidjson;

namespace VV {

Cell default_cells[MAX_CELL_TYPES];
Cell_Type_Info cell_type_infos[MAX_CELL_TYPES];

Result init_cell_factory(std::filesystem::path factory_json_path) {
  std::ifstream f_fjson(factory_json_path, std::ifstream::ate);
  if (!f_fjson.is_open() || !f_fjson.good()) {
    return Result::FILESYSTEM_ERROR;
  }

  std::streamsize size = f_fjson.tellg();
  f_fjson.seekg(0, std::ios::beg);
  std::vector<char> json_data(size);

  if (!f_fjson.read(json_data.data(), size)) {
    return Result::FILESYSTEM_ERROR;
  }

  // LOG_DEBUG("Cell factories: {}", json_data.data());

  rj::Document d;
  d.Parse(json_data.data());

  u16 cell_types_processed = 0;
  for (auto &cell_desc : d.GetObject()) {
    cell_types_processed++;
    if (!cell_desc.value.IsObject()) {
      LOG_WARN("Cell descriptor {} is not an object!",
               cell_desc.name.GetString());
      continue;
    }

    const char *cell_type_name = cell_desc.name.GetString();
    Cell_Type this_cell_type = string_to_cell_type(cell_type_name);

    LOG_DEBUG("Initializing cell factory for cell {} (Type: {})",
              cell_type_name, (u16)this_cell_type);

    Cell_Type_Info &cell_info = cell_type_infos[(u16)this_cell_type];

    Cell_Color default_color = {
        0,    // r_base
        12,   // r_variety
        0,    // g_base
        12,   // g_variety
        0,    // b_base
        12,   // b_variety
        255,  // a_base
        0,    // a_variety
        100,  // probability
    };

    // Fill out a default
    cell_info = {
        Cell_State::SOLID,
        0,
        0.0,
        0.0,
        -1.0,
        Cell_Type::NONE,  // sublimation_cell
        0,                // viscosity
        {default_color},  // colors
        1,                // num_colors
    };

    for (auto &cell_item : cell_desc.value.GetObject()) {
      std::string cell_item_name = cell_item.name.GetString();

      if (cell_item_name == "state") {
        const char *cell_state_name = cell_item.value.GetString();
        cell_info.state = string_to_cell_state(cell_state_name);
      } else if (cell_item_name == "solidity") {
        cell_info.solidity = cell_item.value.GetInt();
      } else if (cell_item_name == "friction") {
        cell_info.friction = cell_item.value.GetDouble();
      } else if (cell_item_name == "passive_heat") {
        cell_info.passive_heat = cell_item.value.GetDouble();
      } else if (cell_item_name == "sublimation_point") {
        cell_info.sublimation_point = cell_item.value.GetDouble();
        if (cell_info.sublimation_point > -1.1 &&
            cell_info.sublimation_point < -0.9) {
          cell_info.state = Cell_State::GAS;
        }
      } else if (cell_item_name == "sublimation_cell") {
        const char *o_cell_type = cell_item.value.GetString();
        cell_info.sublimation_cell = string_to_cell_type(o_cell_type);
      } else if (cell_item_name == "viscosity") {
        cell_info.viscosity = cell_item.value.GetInt();
      } else if (cell_item_name == "r_base") {
        cell_info.colors[0].r_base = cell_item.value.GetInt();
      } else if (cell_item_name == "r_variety") {
        cell_info.colors[0].r_variety = cell_item.value.GetInt();
      } else if (cell_item_name == "g_base") {
        cell_info.colors[0].g_base = cell_item.value.GetInt();
      } else if (cell_item_name == "g_variety") {
        cell_info.colors[0].g_variety = cell_item.value.GetInt();
      } else if (cell_item_name == "b_base") {
        cell_info.colors[0].b_base = cell_item.value.GetInt();
      } else if (cell_item_name == "b_variety") {
        cell_info.colors[0].b_variety = cell_item.value.GetInt();
      } else if (cell_item_name == "a_base") {
        cell_info.colors[0].a_base = cell_item.value.GetInt();
      } else if (cell_item_name == "a_variety") {
        cell_info.colors[0].a_variety = cell_item.value.GetInt();
      } else if (cell_item_name == "colors") {
        if (!cell_item.value.IsArray()) {
          LOG_WARN("Cell colors {} is not an array!",
                   cell_item.name.GetString());
          continue;
        }

        u8 i = 0;
        for (auto &cell_color_item : cell_item.value.GetArray()) {
          if (!cell_color_item.IsObject()) {
            LOG_WARN("Cell colors member is not an object!");
            continue;
          }
          if (i >= MAX_CELL_TYPE_COLORS) {
            LOG_WARN("Too many cell colors.");
            continue;
          }
          Cell_Color &color = cell_info.colors[i];

          color = default_color;

          for (auto &cell_color_object : cell_color_item.GetObject()) {
            std::string cell_color_object_desc =
                cell_color_object.name.GetString();

            if (cell_color_object_desc == "r_base") {
              color.r_base = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "r_variety") {
              color.r_variety = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "g_base") {
              color.g_base = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "g_variety") {
              color.g_variety = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "b_base") {
              color.b_base = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "b_variety") {
              color.b_variety = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "a_base") {
              color.a_base = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "a_variety") {
              color.a_variety = cell_color_object.value.GetInt();
            } else if (cell_color_object_desc == "probability") {
              color.probability = cell_color_object.value.GetInt();
            }
          }

          i++;
        }  // Cell color loop
        LOG_DEBUG("Cell has {} colors", i);

        if (i == 0) {
          LOG_WARN("Bad color array from cell descriptor. Using default color");
          i = 1;
        }
        cell_info.num_colors = i;
      }  // Cell else if chain
    }    // Cell item loop
  }      // Cell loop

  LOG_INFO("Parsed {} cell objects from cell factory file",
           cell_types_processed);
  return Result::SUCCESS;
}

Result init_entity_factory(Update_State &us,
                           std::filesystem::path factory_json) {
  std::ifstream f_fjson(factory_json, std::ifstream::ate);
  if (!f_fjson.is_open() || !f_fjson.good()) {
    return Result::FILESYSTEM_ERROR;
  }

  std::streamsize size = f_fjson.tellg();
  f_fjson.seekg(0, std::ios::beg);
  std::vector<char> json_data(size);

  if (!f_fjson.read(json_data.data(), size)) {
    return Result::FILESYSTEM_ERROR;
  }

  rj::Document d;
  d.Parse(json_data.data());

  std::regex bounding_regex("bounding_([\\S]+)");

  for (auto &entity_desc : d.GetObject()) {
    if (!entity_desc.value.IsObject()) {
      LOG_WARN("Entity descriptor {} is not an object!",
               entity_desc.name.GetString());
      continue;
    }

    std::string entity_name = entity_desc.name.GetString();
    Entity_Factory_Type entity_type;
    if (entity_name == "guyplayer") {
      entity_type = Entity_Factory_Type::GUYPLAYER;
    } else if (entity_name == "tree") {
      entity_type = Entity_Factory_Type::TREE;
    } else if (entity_name == "bush") {
      entity_type = Entity_Factory_Type::BUSH;
    } else if (entity_name == "grass") {
      entity_type = Entity_Factory_Type::GRASS;
    } else if (entity_name == "neitzsche") {
      entity_type = Entity_Factory_Type::NIETZSCHE;
    } else {
      LOG_WARN("Unknown entity in descriptor file: {}", entity_name);
      continue;
    }

    // TODO: Warn if we're overwritting an entity of the same name
    Entity_Factory &new_entity_factory = us.entity_factories[entity_type];
    memset(&new_entity_factory, 0, sizeof(Entity_Factory));
    Entity &new_entity = new_entity_factory.e;

    for (auto &entity_item : entity_desc.value.GetObject()) {
      std::string entity_item_name = entity_item.name.GetString();

      std::smatch bounding_matches;
      if (std::regex_match(entity_item_name, bounding_matches,
                           bounding_regex)) {
        if (bounding_matches.size() == 2) {
          std::string bound_name = bounding_matches[1].str();

          f32 x = entity_item.value["x"].GetDouble();
          f32 y = entity_item.value["y"].GetDouble();

          if (bound_name == "head") {
            new_entity.head_boundingw = x;
            new_entity.head_boundingh = y;
          } else if (bound_name == "body" || bound_name == "default") {
            new_entity.boundingw = x;
            new_entity.boundingh = y;
          }
        } else {
          LOG_WARN("Malformed bounding box name {}. Skipping.",
                   entity_item_name);
        }

        continue;
      }

      if (entity_item_name == "texture") {
        new_entity.texture = (Texture_Id)entity_item.value.GetUint();
        new_entity_factory.register_render = true;
      } else if (entity_item_name == "max_health") {
        new_entity.max_health = entity_item.value.GetUint64();
        new_entity_factory.register_health = true;
      } else if (entity_item_name == "starting_health") {
        new_entity.health = entity_item.value.GetUint64();
      } else if (entity_item_name == "zdepth") {
        new_entity.zdepth = entity_item.value.GetInt();
      } else if (entity_item_name == "kinetic") {
        new_entity_factory.register_kinetic = true;
      } else if (entity_item_name == "bouyancy") {
        new_entity.bouyancy = KINETIC_GRAVITY - entity_item.value.GetDouble();
      } else if (entity_item_name == "deathless") {
        new_entity.status |= (u16)Entity_Status::DEATHLESS;
      } else if (entity_item_name == "flipped") {
        new_entity.flipped = true;
      } else if (entity_item_name == "anim_width") {
        new_entity.anim_width = entity_item.value.GetUint();
        new_entity.status |= (u16)Entity_Status::ANIMATED;
      } else if (entity_item_name == "anim_delay") {
        new_entity.anim_delay = entity_item.value.GetUint();
      } else if (entity_item_name == "anim_frames") {
        new_entity.anim_frames = entity_item.value.GetUint();
      } else if (entity_item_name == "ai_id") {
        new_entity.ai_id = static_cast<AI_ID>(entity_item.value.GetUint());
        new_entity_factory.register_ai = true;
      }
    }
  }

  return Result::SUCCESS;
}

Result init_updating(Update_State &update_state, const Config &config,
                     const std::optional<u32> &seed) {
  update_state.thread_pool = new ThreadPool(config.num_threads);

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

  std::filesystem::path res_dir;
  Result res_dir_res = get_resource_dir(res_dir);
  if (res_dir_res != Result::SUCCESS) {
    LOG_ERROR("Updater failed to get resource dir");
    return res_dir_res;
  }
  std::filesystem::path entity_factory_path = res_dir / "entity_factory.json";
  Result entity_factory_res =
      init_entity_factory(update_state, entity_factory_path);
  if (entity_factory_res != Result::SUCCESS) {
    LOG_ERROR("Updater failed to initialize entity factories");
    return res_dir_res;
  }
  std::filesystem::path cell_factory_path = res_dir / "cell_factory.json";
  Result cell_factory_res = init_cell_factory(cell_factory_path);
  if (cell_factory_res != Result::SUCCESS) {
    LOG_ERROR("Updater failed to initialize cell factories");
    return res_dir_res;
  }

  Result ap_res =
      create_entity(update_state, update_state.active_dimension,
                    Entity_Factory_Type::GUYPLAYER, update_state.active_player);

  if (ap_res != Result::SUCCESS) {
    LOG_ERROR("Couldn't create initial player! EC: {:x}", (s32)ap_res);
    return ap_res;
  }

  Entity &active_player = *get_active_player(update_state);

  load_chunks_square(update_state, update_state.active_dimension,
                     active_player.coord.x, active_player.coord.y, 8 / 2);

  return Result::SUCCESS;
}

Result update(Update_State &update_state) {
  Entity &active_player = *get_active_player(update_state);
  static Chunk_Coord last_player_chunk =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

  // active_player.health--;

  update_health(update_state);
  update_mouse(update_state);
  Result res = update_keypresses(update_state);
  if (res == Result::WINDOW_CLOSED) {
    LOG_INFO("Got close from keyboard");
    return res;
  }
  update_ai(update_state);
  update_kinetic(update_state);
  Chunk_Coord current_player_chunk =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

  if (!(last_player_chunk == current_player_chunk)) {
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

void destroy_update(Update_State &update_state) {
  delete update_state.thread_pool;
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

    const u8 CELL_PLACE_RADIUS = 3;
    for (s64 x = c.x - CELL_PLACE_RADIUS; x < c.x + CELL_PLACE_RADIUS; x++) {
      for (s64 y = c.y - CELL_PLACE_RADIUS; y < c.y + CELL_PLACE_RADIUS; y++) {
        Chunk_Coord cc = get_chunk_coord(x, y);

        Chunk &chunk = active_dimension.chunks[cc];
        u16 cx = std::abs((cc.x * CHUNK_CELL_WIDTH) - x);
        u16 cy = y - (cc.y * CHUNK_CELL_WIDTH);
        u32 cell_index = cx + cy * CHUNK_CELL_WIDTH;

        assert(cell_index < CHUNK_CELLS);

        chunk.cells[cell_index] = create_cell(Cell_Type::WATER);
      }
    }

    us.events.emplace(Update_Event::CELL_CHANGE);
  }

  return Result::SUCCESS;
}

Result update_keypresses(Update_State &us) {
  static int num_keys;

  const Uint8 *keys = SDL_GetKeyboardState(&num_keys);

  Entity &active_player = *get_active_player(us);

  static constexpr f32 MOVEMENT_CONSTANT = 0.4f;
  static constexpr f32 AIR_MOV_CONSTANT = 0.15f;
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
      } else if (!(active_player.status & (u8)Entity_Status::ON_GROUND)) {
        active_player.ax -= AIR_MOV_CONSTANT;
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
      } else if (!(active_player.status & (u8)Entity_Status::ON_GROUND)) {
        active_player.ax += AIR_MOV_CONSTANT;
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
      entity.ax *= cell_type_infos[(u8)Cell_Type::WATER].friction;
      entity.ay *= cell_type_infos[(u8)Cell_Type::WATER].friction;
      entity.vx *= cell_type_infos[(u8)Cell_Type::WATER].friction;
      entity.vy *= cell_type_infos[(u8)Cell_Type::WATER].friction;

      entity.vy += entity.bouyancy;
    } else if (entity.status & (u8)Entity_Status::ON_GROUND) {
      entity.ax *= cell_type_infos[(u8)Cell_Type::DIRT].friction;
      entity.ay *= cell_type_infos[(u8)Cell_Type::DIRT].friction;
      entity.vx *= cell_type_infos[(u8)Cell_Type::DIRT].friction;
      entity.vy *= cell_type_infos[(u8)Cell_Type::DIRT].friction;
    } else {
      entity.ax *= cell_type_infos[(u8)Cell_Type::AIR].friction;
      entity.ay *= cell_type_infos[(u8)Cell_Type::AIR].friction;
      entity.vx *= cell_type_infos[(u8)Cell_Type::AIR].friction;
      entity.vy *= cell_type_infos[(u8)Cell_Type::AIR].friction;
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
  // structure (octtree?) to determine if we're colliding with other entities
  for (Entity_ID entity_index : active_dimension.e_kinetic) {
    bool nica_damage = false;
    Entity &entity = update_state.entities[entity_index];
    entity.status = entity.status & ~((u8)Entity_Status::IN_WATER |
                                      (u8)Entity_Status::ON_GROUND);

    Chunk_Coord cc = get_chunk_coord(entity.coord.x, entity.coord.y);

    // TODO: Should also definitly multithread this

    // Right now just checking the chunks below and to the right since that's
    // where the bounding box might spill over. If an entity ever becomes
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
            case Cell_Type::NICARAGUA: {
              if (!nica_damage) {
                entity.health -= 10;
                nica_damage = true;
              }
              [[fallthrough]];
            }
            case Cell_Type::SNOW:
            case Cell_Type::GOLD:
            case Cell_Type::SAND:
            case Cell_Type::GRASS:
            case Cell_Type::DIRT: {
              if (entity.coord.y - entity.boundingh <= cell_coord.y) {
                entity.status |= (u8)Entity_Status::ON_GROUND;
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
            case Cell_Type::STEAM:
            case Cell_Type::NONE:
            case Cell_Type::AIR: {
              break;
            }
            case Cell_Type::LAVA: {
              entity.health -= 1;
              [[fallthrough]];
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

bool process_steam_cell(Dimension &dim, Chunk &chunk, u32 cell_index) {
  s64 cx = chunk.coord.x * CHUNK_CELL_WIDTH +
           static_cast<s64>(cell_index % CHUNK_CELL_WIDTH);
  s64 cy = chunk.coord.y * CHUNK_CELL_WIDTH +
           static_cast<s64>(cell_index / CHUNK_CELL_WIDTH);

  Cell &cell = chunk.cells[cell_index];
  // Start at bottom then sides
  u32 rand_dir = std::rand();
  s8 side_mod = rand_dir % 10;

  // Normally this would just be a for loop going through the
  // directions, but this has to be so wicked fast

  Cell *o_cell = get_cell_at_world_pos(
      dim, cx, cy + (rand_dir % 4 == 0 ? 1 : 0));  // Start with bottom
  if (o_cell != nullptr) {
    // Giving the steam some bonus upward power
    if (cell_type_infos[(u16)o_cell->type].solidity <
        cell_type_infos[(u16)Cell_Type::STEAM].solidity + 30.0f) {
      std::swap(cell, *o_cell);
      return true;
    }
  }

  // Only check one direction and do so randomly
  /*
if (rand_dir % 10 < 5) {  // Some of the time skip moving
break;
}
  */
  if (rand_dir & 1) {
    o_cell = get_cell_at_world_pos(dim, cx - side_mod, cy);  // Left
    if (o_cell != nullptr) {
      if (cell_type_infos[(u16)o_cell->type].solidity <
          cell_type_infos[(u16)Cell_Type::STEAM].solidity) {
        std::swap(cell, *o_cell);
        return true;
      }
    }
  } else {
    o_cell = get_cell_at_world_pos(dim, cx + side_mod, cy);  // Right
    if (o_cell != nullptr) {
      if (cell_type_infos[(u16)o_cell->type].solidity <
          cell_type_infos[(u16)Cell_Type::STEAM].solidity) {
        std::swap(cell, *o_cell);
        return true;
      }
    }
  }

  return false;
}

bool process_fluid_cell(Dimension &dim, Chunk &chunk, u32 cell_index) {
  Cell &cell = chunk.cells[cell_index];
  const Cell_Type_Info &cell_info = *cell.cell_info;

  u32 rand_dir = std::rand();
#ifndef NDEBUG
  static bool suppressed = false;
  if (cell_info.viscosity == 0 && !suppressed) {
    LOG_WARN(
        "process_fluid_cell called on non fluid cell! Cell type {} with 0 "
        "viscosity",
        (int)cell.type);
    suppressed = true;
    return false;
  }
#endif
  s8 side_mod = rand_dir % cell_info.viscosity;

  // if in first row, the next cell we get needs to be from the chunk below us.
  Cell *o_cell = nullptr;
  if (cell_index < CHUNK_CELL_WIDTH) {
    Chunk_Coord o_cc = {chunk.coord.x, chunk.coord.y - 1};
    const auto &o_chunk_iter = dim.chunks.find(o_cc);
    if (o_chunk_iter != dim.chunks.end()) {
      o_cell = &o_chunk_iter->second
                    .cells[CHUNK_CELLS - (CHUNK_CELL_WIDTH - cell_index)];
    }
  } else {
    o_cell = &chunk.cells[cell_index - CHUNK_CELL_WIDTH];
  }
  if (o_cell != nullptr) {
    if (cell_type_infos[(u16)o_cell->type].passive_heat >
        cell_info.sublimation_point) {
      // TODO: Need a map of cell functions that we can call with
      // cell_info.sublimation_cell
      cell = create_cell(Cell_Type::STEAM);
      return true;
    }
    if (cell_type_infos[(u16)o_cell->type].solidity < cell_info.solidity) {
      std::swap(cell, *o_cell);
      return true;
    }
  }

  // Only check one direction and do so randomly
  if (rand_dir & 1) {  // Move left
    Cell *o_cell = nullptr;
    if ((cell_index - side_mod) / CHUNK_CELL_WIDTH !=
            (cell_index) / CHUNK_CELL_WIDTH ||
        cell_index == 0) {  // We're on the leftmost border of the chunk
      Chunk_Coord o_cc = {chunk.coord.x - 1, chunk.coord.y};
      const auto &o_chunk_iter = dim.chunks.find(o_cc);
      if (o_chunk_iter != dim.chunks.end()) {
        u32 o_cell_index = cell_index - side_mod + CHUNK_CELL_WIDTH;
        assert(o_cell_index < CHUNK_CELLS);
        o_cell = &o_chunk_iter->second.cells[o_cell_index];
      }
    } else {
      assert(cell_index - side_mod < CHUNK_CELLS);
      o_cell = &chunk.cells[cell_index - side_mod];
    }

    if (o_cell != nullptr) {
      if (cell_type_infos[(u16)o_cell->type].solidity < cell_info.solidity) {
        std::swap(cell, *o_cell);
        return true;
      }
    }
  } else {  // Move right
    Cell *o_cell = nullptr;
    if ((cell_index + side_mod) / CHUNK_CELL_WIDTH !=
            (cell_index) / CHUNK_CELL_WIDTH ||
        cell_index ==
            CHUNK_CELLS - 1) {  // We're on the rightmost border of the chunk
      Chunk_Coord o_cc = {chunk.coord.x + 1, chunk.coord.y};
      const auto &o_chunk_iter = dim.chunks.find(o_cc);
      if (o_chunk_iter != dim.chunks.end()) {
        u32 o_cell_index = cell_index + side_mod - CHUNK_CELL_WIDTH;
        assert(o_cell_index < CHUNK_CELLS);
        o_cell = &o_chunk_iter->second.cells[o_cell_index];
      }
    } else {
      assert(cell_index + side_mod < CHUNK_CELLS);
      o_cell = &chunk.cells[cell_index + side_mod];
    }

    if (o_cell != nullptr) {
      if (cell_type_infos[(u16)o_cell->type].solidity < cell_info.solidity) {
        std::swap(cell, *o_cell);
        return true;
      }
    }
  }

  return false;
}

bool process_powder_cell(Dimension &dim, Chunk &chunk, u32 cell_index) {
  Cell &cell = chunk.cells[cell_index];
  const Cell_Type_Info &cell_info = *cell.cell_info;

  // if in first row, the next cell we get needs to be from the chunk below us.
  Cell *o_cell = nullptr;
  if (cell_index < CHUNK_CELL_WIDTH) {
    Chunk_Coord o_cc = {chunk.coord.x, chunk.coord.y - 1};
    const auto &o_chunk_iter = dim.chunks.find(o_cc);
    if (o_chunk_iter != dim.chunks.end()) {
      o_cell = &o_chunk_iter->second
                    .cells[CHUNK_CELLS - (CHUNK_CELL_WIDTH - cell_index)];
    }
  } else {
    o_cell = &chunk.cells[cell_index - CHUNK_CELL_WIDTH];
  }
  if (o_cell != nullptr) {
    if (cell_type_infos[(u16)o_cell->type].solidity < cell_info.solidity) {
      std::swap(cell, *o_cell);
      return true;
    }
  }

  // Only check one direction and do so randomly
  u32 rand_dir = std::rand();
  if (rand_dir & 1) {  // Move left
    Cell *o_cell = nullptr;
    if ((cell_index - 1) / CHUNK_CELL_WIDTH !=
            (cell_index) / CHUNK_CELL_WIDTH ||
        cell_index == 0) {  // We're on the leftmost border of the chunk
      Chunk_Coord o_cc = {chunk.coord.x - 1, chunk.coord.y};
      const auto &o_chunk_iter = dim.chunks.find(o_cc);
      if (o_chunk_iter != dim.chunks.end()) {
        u32 o_cell_index = cell_index - 1 + CHUNK_CELL_WIDTH;
        assert(o_cell_index < CHUNK_CELLS);
        o_cell = &o_chunk_iter->second.cells[o_cell_index];
      }
    } else {
      assert(cell_index - 1 < CHUNK_CELLS);
      o_cell = &chunk.cells[cell_index - 1];
    }

    if (o_cell != nullptr) {
      if (cell_type_infos[(u16)o_cell->type].solidity < cell_info.solidity) {
        std::swap(cell, *o_cell);
        return true;
      }
    }
  } else {  // Move right
    Cell *o_cell = nullptr;
    if ((cell_index + 1) / CHUNK_CELL_WIDTH !=
            (cell_index) / CHUNK_CELL_WIDTH ||
        cell_index ==
            CHUNK_CELLS - 1) {  // We're on the rightmost border of the chunk
      Chunk_Coord o_cc = {chunk.coord.x + 1, chunk.coord.y};
      const auto &o_chunk_iter = dim.chunks.find(o_cc);
      if (o_chunk_iter != dim.chunks.end()) {
        u32 o_cell_index = cell_index + 1 - CHUNK_CELL_WIDTH;
        assert(o_cell_index < CHUNK_CELLS);
        o_cell = &o_chunk_iter->second.cells[o_cell_index];
      }
    } else {
      assert(cell_index + 1 < CHUNK_CELLS);
      o_cell = &chunk.cells[cell_index + 1];
    }

    if (o_cell != nullptr) {
      if (cell_type_infos[(u16)o_cell->type].solidity < cell_info.solidity) {
        std::swap(cell, *o_cell);
        return true;
      }
    }
  }

  return false;
}
void update_all_water_chunk(Dimension &dim, Chunk &chunk) {
  // Iterate over the bottom row of cells in the chunk
  bool still_all_water = true;
  for (u32 x = 0; x < CHUNK_CELL_WIDTH; x++) {
    u32 bottom_index = (CHUNK_CELL_WIDTH - 1) * CHUNK_CELL_WIDTH + x;
    if (chunk.cells[bottom_index].type == Cell_Type::WATER) {
      if (process_fluid_cell(dim, chunk, bottom_index)) {
        still_all_water = false;
      }
    }
  }

  // Iterate over the leftmost and rightmost columns (excluding corners already
  // covered)
  for (u32 y = 1; y < CHUNK_CELL_WIDTH - 1; y++) {
    u32 left_index = y * CHUNK_CELL_WIDTH;
    u32 right_index = y * CHUNK_CELL_WIDTH + (CHUNK_CELL_WIDTH - 1);
    if (chunk.cells[left_index].type == Cell_Type::WATER) {
      if (process_fluid_cell(dim, chunk, left_index)) {
        still_all_water = false;
      }
    }
    if (chunk.cells[right_index].type == Cell_Type::WATER) {
      if (process_fluid_cell(dim, chunk, right_index)) {
        still_all_water = false;
      }
    }
  }

  if (!still_all_water) {
    chunk.all_cell = Cell_Type::NONE;
  }
}

void update_cells_chunk(Dimension &dim, Chunk &chunk) {
  switch (chunk.all_cell) {
    case (Cell_Type::WATER): {
      update_all_water_chunk(dim, chunk);
      break;
    }
    default: {
      for (u32 cell_index = 0; cell_index < CHUNK_CELLS; cell_index++) {
        const Cell_Type_Info &cell_info = *chunk.cells[cell_index].cell_info;

        switch (cell_info.state) {
          case Cell_State::POWDER: {  // Basic sand movement
            process_powder_cell(dim, chunk, cell_index);
            break;
          }
          case Cell_State::LIQUID: {
            process_fluid_cell(dim, chunk, cell_index);
            break;
          }
          case Cell_State::GAS: {
            // process_steam_cell(dim, chunk, cell_index);
            break;
          }
          default:
            break;
        }
      }

      break;
    }  // default case
  }    // all_cell switch
}

void update_health(Update_State &us) {
  Dimension &dim = *get_active_dimension(us);

  std::vector<Entity_ID> dead_entities;
  for (Entity_ID id : dim.e_health) {
    Entity &e = us.entities[id];

    // clamp health to max to be sure
    e.health = std::min(e.max_health, e.health);

    if (e.health <= 0) {
      // For now return to respawn point
      if (e.status & (u16)Entity_Status::DEATHLESS) {
        LOG_DEBUG("Entity {} died.", id);
        e.coord = e.respawn_point;
        e.health = e.max_health;
      } else {
        // Doesn't really matter since they'll be deleted
        // e.status |= (u8)Entity_Status::DEAD;

        // Delete entity
        dead_entities.push_back(id);
      }
    }
  }

  // Delete dead but not deathless entities
  for (Entity_ID id : dead_entities) {
    delete_entity(us, dim, id);
  }
}

void update_cells(Update_State &update_state) {
  Entity &active_player = *get_active_player(update_state);
  Dimension &dim = *get_active_dimension(update_state);

  Chunk_Coord player_chunkc =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

  // Calculate bounds based on the player's current chunk coordinates.
  Chunk_Coord bl;
  bl.x = player_chunkc.x - CHUNK_CELL_SIM_RADIUS;
  bl.y = player_chunkc.y - CHUNK_CELL_SIM_RADIUS;

  // Prepare a thread-safe queue or stack to hold all chunks that need
  // processing.
  ThreadSafeProcessingSet chunk_stack;

  // Populate the queue with all chunks in the area.
  for (int x = bl.x; x < bl.x + CHUNK_CELL_SIM_RADIUS * 2; x++) {
    for (int y = bl.y; y < bl.y + CHUNK_CELL_SIM_RADIUS * 2; y++) {
      Chunk_Coord ic = {x, y};
      if (dim.chunks.find(ic) !=
          dim.chunks.end()) {  // Ensure chunk exists before adding
        chunk_stack.push(ic);
      }
    }
  }

  std::vector<std::future<void>> futures;
  for (int i = 0; i < 4; i++) {  // Assuming 4 worker threads
    auto future = update_state.thread_pool->enqueue([&]() {
      Chunk_Coord chunk_coord;
      while (chunk_stack.try_pop(
          chunk_coord)) {  // Attempt to pop a chunk from the stack
        if (!chunk_stack.is_adjacent(chunk_coord)) {
          // Process the chunk.
          auto chunk_iter = dim.chunks.find(chunk_coord);
          if (chunk_iter !=
              dim.chunks.end()) {  // Double-check in case of race conditions
            update_cells_chunk(dim, chunk_iter->second);
          }
          chunk_stack.mark_done(
              chunk_coord);  // Mark this chunk as done processing
        } else {
          // If adjacent to a chunk being processed, push it back for later
          // processing.
          std::this_thread::yield();  // Optionally yield to reduce tight
                                      // looping on this condition
          chunk_stack.push(chunk_coord);
        }
      }
    });

    futures.push_back(std::move(future));
  }

  // Wait for all futures to complete
  for (auto &future : futures) {
    future.wait();
  }
}

void update_ai(Update_State &us) {
  Entity &active_player = *get_active_player(us);
  Dimension &dim = *get_active_dimension(us);

  // Coordinates of the player
  f64 player_x = active_player.coord.x;
  f64 player_y = active_player.coord.y;

  for (Entity_ID e_id : dim.e_ai) {
    Entity &e = us.entities[e_id];

    // Calculate the vector difference between the entity and the player
    f64 dx = e.coord.x - player_x;
    f64 dy = e.coord.y - player_y;
    f64 distance = sqrt(dx * dx + dy * dy);

    if (distance <= AI_CELL_RADIUS &&
        distance > 0) {  // Ensure distance is not zero
      switch (e.ai_id) {
        case AI_ID::FOLLOW_PLAYER_SLOW: {
          // Normalize the direction vector and scale it by a desired
          // acceleration amount
          f64 accel_magnitude = -0.1;  // Set the acceleration magnitude
          f64 ax = (dx / distance) * accel_magnitude;
          f64 ay = (dy / distance) * accel_magnitude;

          e.flipped = ax < 0;

          // Apply acceleration to velocity
          e.vx += ax;
          e.vy += ay;

          // Optionally apply a damping factor to prevent excessive speeds
          f64 damping_factor = 0.95;
          e.vx *= damping_factor;
          e.vy *= damping_factor;

          // Update the position
          e.coord.x += e.vx;
          e.coord.y += e.vy;
        }
      }
    }
  }
}

void gen_ov_forest_ch(Update_State &update_state, Chunk &chunk,
                      const Chunk_Coord &chunk_coord) {
  bool all_water = true;
  bool all_air = true;

  for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
    f64 abs_x = x + chunk_coord.x * CHUNK_CELL_WIDTH;
    u8 grass_depth = 40 + surface_det_rand(static_cast<u64>(abs_x) ^
                                           update_state.world_seed) %
                              25;
    s32 height =
        static_cast<s32>(surface_height(abs_x, 64, update_state.world_seed)) +
        SURFACE_Y_MIN * CHUNK_CELL_WIDTH;
    for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
      u16 cell_index = x + (y * CHUNK_CELL_WIDTH);

      s32 our_height = (y + (chunk_coord.y * CHUNK_CELL_WIDTH));
      if (height < SEA_LEVEL_CELL && our_height <= height) {
        chunk.cells[cell_index] = create_cell(Cell_Type::SAND);
        all_water = false;
        all_air = false;
      } else if (height < SEA_LEVEL_CELL && our_height > height &&
                 our_height < SEA_LEVEL_CELL) {
        chunk.cells[cell_index] = create_cell(Cell_Type::WATER);
        all_air = false;
      } else if (our_height < height && our_height >= height - grass_depth) {
        chunk.cells[cell_index] = create_cell(Cell_Type::GRASS);
        all_water = false;
        all_air = false;
      } else if (our_height < height - grass_depth) {
        chunk.cells[cell_index] = create_cell(Cell_Type::DIRT);
        all_water = false;
        all_air = false;
      } else {
        chunk.cells[cell_index] = create_cell(Cell_Type::AIR);
        all_water = false;
      }
    }

    if (all_water) {
      chunk.all_cell = Cell_Type::WATER;
    } else if (all_air) {
      chunk.all_cell = Cell_Type::AIR;
    }

    // added distance between tree's to prevent overlap
    if (surface_det_rand(static_cast<u64>(abs_x) ^ update_state.world_seed) %
                GEN_TREE_MAX_WIDTH <
            15 &&
        height > chunk_coord.y * CHUNK_CELL_WIDTH &&
        height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
        height >= SEA_LEVEL_CELL) {
      // Check if a tree already exists at this location
      bool locationFree = true;
      for (const auto &entity_id :
           update_state.dimensions[update_state.active_dimension]
               .entity_indicies) {
        const Entity &existingEntity = update_state.entities[entity_id];

        if (existingEntity.texture == Texture_Id::TREE &&
            std::abs(existingEntity.coord.x - abs_x) <
                100) {  // 100 distance between tree's
          locationFree = false;
          break;
        }
      }

      if (locationFree) {
        Entity_ID id;
        create_entity(update_state, update_state.active_dimension,
                      Entity_Factory_Type::TREE, id);

        Entity &tree = update_state.entities[id];
        tree.coord.x = abs_x;
        tree.coord.y =
            height +
            85.0f;  // This assumes tree base height doesn't affect spawn logic
      }
    }

    // Unified spawner for bush and grass
    if (surface_det_rand(static_cast<u64>(abs_x) ^ update_state.world_seed) %
                GEN_TREE_MAX_WIDTH <
            15 &&
        height > chunk_coord.y * CHUNK_CELL_WIDTH &&
        height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
        height >= SEA_LEVEL_CELL) {
      bool locationFreeForBush = true;
      bool locationFreeForGrass = true;

      bool tryBushFirst = (std::rand() % 2) == 0;

      for (const auto &entity_id :
           update_state.dimensions[update_state.active_dimension]
               .entity_indicies) {
        const Entity &existingEntity = update_state.entities[entity_id];

        if (existingEntity.texture == Texture_Id::BUSH &&
            std::abs(existingEntity.coord.x - abs_x) <
                15) {  // Check for existing bush
          locationFreeForBush = false;
        }

        if (existingEntity.texture == Texture_Id::GRASS &&
            std::abs(existingEntity.coord.x - abs_x) <
                10) {  // Check for existing grass
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
          create_entity(update_state, update_state.active_dimension,
                        Entity_Factory_Type::BUSH, id);

          Entity &bush = update_state.entities[id];
          bush.coord.x = abs_x;
          bush.coord.y = height + 20.0f;
        }

        // Spawn grass if location is free
        else if (locationFreeForGrass) {
          Entity_ID id;
          create_entity(update_state, update_state.active_dimension,
                        Entity_Factory_Type::GRASS, id);

          Entity &grass = update_state.entities[id];
          grass.coord.x = abs_x;
          grass.coord.y = height + 10.0f;
        }
      } else {
        // Spawn grass if location is free
        if (locationFreeForGrass) {
          Entity_ID id;
          create_entity(update_state, update_state.active_dimension,
                        Entity_Factory_Type::GRASS, id);

          Entity &grass = update_state.entities[id];
          grass.coord.x = abs_x;
          grass.coord.y = height + 10.0f;
        }

        // Spawn bush if location is free
        else if (locationFreeForBush) {
          Entity_ID id;
          create_entity(update_state, update_state.active_dimension,
                        Entity_Factory_Type::BUSH, id);

          Entity &bush = update_state.entities[id];
          bush.coord.x = abs_x;
          bush.coord.y = height + 20.0f;
        }
      }
    }

    // neitzsche spawner
    if (abs_x == 250 && height > chunk_coord.y * CHUNK_CELL_WIDTH &&
        height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH) {
      Entity_ID id;
      create_entity(update_state, update_state.active_dimension,
                    Entity_Factory_Type::NIETZSCHE, id);

      Entity &neitzsche = update_state.entities[id];
      neitzsche.coord.x = abs_x;
      neitzsche.coord.y = height + 85.0f;
    }
  }
}

void gen_ov_alaska_ch(Update_State &update_state, Chunk &chunk,
                      const Chunk_Coord &chunk_coord) {
  bool all_air = true;
  for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
    f64 abs_x = x + chunk_coord.x * CHUNK_CELL_WIDTH;
    s32 height = static_cast<s32>(surface_height(
                     abs_x, 64, update_state.world_seed, 64 * CHUNK_CELL_WIDTH,
                     CHUNK_CELL_WIDTH * 6)) +
                 SURFACE_Y_MIN * CHUNK_CELL_WIDTH;

    for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
      u16 cell_index = x + (y * CHUNK_CELL_WIDTH);

      s32 our_height = (y + (chunk_coord.y * CHUNK_CELL_WIDTH));
      if (our_height > height) {
        chunk.cells[cell_index] = create_cell(Cell_Type::AIR);
      } else {
        u8 snow_depth = 60 + surface_det_rand(static_cast<u64>(abs_x) ^
                                              update_state.world_seed) %
                                 25;

        if (our_height > height - snow_depth) {
          chunk.cells[cell_index] = create_cell(Cell_Type::SNOW);
        } else {
          chunk.cells[cell_index] = create_cell(Cell_Type::DIRT);
        }
        all_air = false;
      }
    }

    u16 tree_rand =
        surface_det_rand(static_cast<u64>(abs_x) ^ update_state.world_seed);
    if (tree_rand % AK_GEN_TREE_MAX_WIDTH < 15 &&
        height > chunk_coord.y * CHUNK_CELL_WIDTH &&
        height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
        height >= SEA_LEVEL_CELL) {
      Entity_ID new_tree;
      Result en_res = create_entity(update_state, update_state.active_dimension,
                                    Entity_Factory_Type::TREE, new_tree);

      if (en_res == Result::SUCCESS) {
        Entity &tree = update_state.entities[new_tree];

        if (tree_rand & 1) {
          tree.texture = Texture_Id::AKTREE1;
          tree.coord.y = height + 110;
        } else {
          tree.texture = Texture_Id::AKTREE2;
          tree.coord.y = height + 90;
        }

        tree.coord.x = abs_x;
      }
    }
  }

  if (all_air) {
    chunk.all_cell = Cell_Type::AIR;
  }
}

void gen_ov_ocean_chunk(Update_State &update_state, Chunk &chunk,
                        const Chunk_Coord &chunk_coord) {
  // void cast these to promise the compiler we're going to use them in this
  // function eventually
  (void)update_state;

  if (chunk_coord.y < SEA_LEVEL) {
    for (u32 cell_index = 0; cell_index < CHUNK_CELLS; cell_index++) {
      chunk.cells[cell_index] = create_cell(Cell_Type::WATER);
    }

    chunk.all_cell = Cell_Type::WATER;
  } else {
    for (u32 cell_index = 0; cell_index < CHUNK_CELLS; cell_index++) {
      chunk.cells[cell_index] = create_cell(Cell_Type::AIR);
    }

    chunk.all_cell = Cell_Type::AIR;
  }
}

void gen_ov_nicaragua(Update_State &update_state, Chunk &chunk,
                      const Chunk_Coord &chunk_coord) {
  bool all_air = true;
  for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
    f64 abs_x = x + chunk_coord.x * CHUNK_CELL_WIDTH;
    s32 height = static_cast<s32>(surface_height(
                     abs_x, 64, update_state.world_seed, 64 * CHUNK_CELL_WIDTH,
                     CHUNK_CELL_WIDTH * 26)) +
                 SURFACE_Y_MIN * CHUNK_CELL_WIDTH;

    for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
      u16 cell_index = x + (y * CHUNK_CELL_WIDTH);

      s32 our_height = (y + (chunk_coord.y * CHUNK_CELL_WIDTH));
      if (our_height < height) {
        chunk.cells[cell_index] = create_cell(Cell_Type::NICARAGUA);
        all_air = false;
      } else if (our_height < SEA_LEVEL_CELL + (CHUNK_CELL_WIDTH * 2)) {
        chunk.cells[cell_index] = create_cell(Cell_Type::LAVA);
      } else {
        chunk.cells[cell_index] = create_cell(Cell_Type::AIR);
      }
    }
  }

  if (all_air) {
    chunk.all_cell = Cell_Type::AIR;
  }
}

void gen_overworld_chunk(Update_State &update_state, Chunk &chunk,
                         const Chunk_Coord &chunk_coord) {
  if (chunk_coord.x < NICARAGUA_EAST_BORDER_CHUNK) {
    gen_ov_nicaragua(update_state, chunk, chunk_coord);
    return;
  }

  if (chunk_coord.x < FOREST_EAST_BORDER_CHUNK) {
    gen_ov_forest_ch(update_state, chunk, chunk_coord);
    return;
  }

  if (chunk_coord.x < ALASKA_EAST_BORDER_CHUNK) {
    gen_ov_alaska_ch(update_state, chunk, chunk_coord);
    return;
  }

  gen_ov_ocean_chunk(update_state, chunk, chunk_coord);
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
      gen_overworld_chunk(update_state, chunk, chunk_coord);
      break;
    }
    case DimensionIndex::WATERWORLD: {
      const Cell base_air = create_cell(Cell_Type::AIR);
      if (chunk_coord.y > SEA_LEVEL) {
        for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
          for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
            chunk.cells[x + y * CHUNK_CELL_WIDTH] = base_air;
          }
        }
      } else {
        for (u8 x = 0; x < CHUNK_CELL_WIDTH; x++) {
          for (u8 y = 0; y < CHUNK_CELL_WIDTH; y++) {
            chunk.cells[x + y * CHUNK_CELL_WIDTH] =
                create_cell(Cell_Type::WATER);
          }
        }
      }
      chunk.all_cell = Cell_Type::WATER;
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

Cell create_cell(Cell_Type type) {
  const Cell_Type_Info &CELL_INFO = cell_type_infos[static_cast<u16>(type)];

  // Ensure there are color configurations available
  if (CELL_INFO.num_colors == 0) {
    return {type, &CELL_INFO, 0xff, 0, 0xff, 255};  // Default to magenta
  }

  const Cell_Color *selected_color = nullptr;

  // Iterate through each color and select based on probability
  for (u8 i = 0; i < CELL_INFO.num_colors; ++i) {
    const Cell_Color &color = CELL_INFO.colors[i];
    int random_value =
        std::rand() % 100 + 1;  // Generate a number from 1 to 100
    if (random_value <= color.probability) {
      selected_color = &color;
      break;
    }
  }

  // If no color was selected by probability, default to the last color
  if (!selected_color) {
    selected_color = &CELL_INFO.colors[CELL_INFO.num_colors - 1];
  }

  // Compute color components with variation
  u8 r =
      selected_color->r_variety == 0
          ? selected_color->r_base
          : selected_color->r_base + std::rand() % (selected_color->r_variety);
  u8 g =
      selected_color->g_variety == 0
          ? selected_color->g_base
          : selected_color->g_base + std::rand() % (selected_color->g_variety);
  u8 b =
      selected_color->b_variety == 0
          ? selected_color->b_base
          : selected_color->b_base + std::rand() % (selected_color->b_variety);
  u8 a =
      selected_color->a_variety == 0
          ? selected_color->a_base
          : selected_color->a_base + std::rand() % (selected_color->a_variety);

  // Create and return the cell
  Cell ret_cell = {type, &CELL_INFO, r, g, b, a};

  return ret_cell;
}

Result create_entity(Update_State &us, DimensionIndex dim,
                     Entity_Factory_Type type, Entity_ID &id) {
  Result id_res = get_entity_id(us.entity_id_pool, id);
  if (id_res != Result::SUCCESS) {
    id = 0;
    return id_res;
  }

  us.entities[id] = default_entity();

  Entity_Factory &factory = us.entity_factories[type];
  us.entities[id] = factory.e;

  auto dimension_iter = us.dimensions.find(dim);
  if (dimension_iter != us.dimensions.end()) {
    dimension_iter->second.entity_indicies.insert(id);

    if (factory.register_kinetic) {
      dimension_iter->second.e_kinetic.insert(id);
    }
    if (factory.register_health) {
      dimension_iter->second.e_health.insert(id);
    }
    if (factory.register_render) {
      dimension_iter->second.e_render.emplace(us.entities[id].zdepth, id);
    }
    if (factory.register_ai) {
      dimension_iter->second.e_ai.insert(id);
    }
  } else {
    LOG_WARN(
        "Failed to create new entity. Couldn't find dimension specified: {}",
        (u8)dim);
    return Result::VALUE_ERROR;
  }

  return Result::SUCCESS;
}

void delete_entity(Update_State &us, Dimension &dim, Entity_ID id) {
  auto kin_iter = dim.e_kinetic.find(id);
  if (kin_iter != dim.e_kinetic.end()) {
    dim.e_kinetic.erase(id);
  }
  auto health_iter = dim.e_kinetic.find(id);
  if (health_iter != dim.e_health.end()) {
    dim.e_health.erase(id);
  }
  auto range = dim.e_render.equal_range(id);
  // Erase all elements within this range
  if (range.first != range.second) {
    dim.e_render.erase(range.first, range.second);
  }

  auto entity_iter = us.entity_id_pool.find(id);
  if (entity_iter != us.entity_id_pool.end()) {
    us.entity_id_pool.erase(id);
  }
}

}  // namespace VV
