#include "app.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <regex>
#include <set>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_pixels.h"
#include "SDL_render.h"
#include "SDL_surface.h"
#include "SDL_timer.h"
#include "SDL_video.h"

// Platform specific includes
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#error "Unsupported platform"
#endif

namespace VV {
/////////////////////////////////
/// Utilities implementations ///
/////////////////////////////////

/// Config implementations ///

Config g_config;

Config default_config() {
  return {
      600,   // window_width
      400,   // window_height
      true,  // window_start_maximized
      "",    // res_dir: Should be set by caller
      "",    // tex_dir: set with res_dir
  };
}

/// Resource implementations ///

// NOTE (Levi): I don't know the other platforms, so ChatGPT did them. Platform
// specific includes for this are at the top of the file.
Result get_resource_dir(std::filesystem::path &res_dir) {
#ifdef _WIN32
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  res_dir = std::filesystem::path(path).parent_path() / "res";
  return Result::SUCCESS;
#elif defined(__linux__)
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX - 1);
  if (count > 0) {
    path[count] = '\0';
    res_dir = std::filesystem::path(path).parent_path() / "res";
    return Result::SUCCESS;
  } else {
    LOG_ERROR("Failed to find resource dir on linux!");
    return Result::FILESYSTEM_ERROR;
  }
#elif defined(__APPLE__)
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    path[size] =
        '\0';  // Ensure null-termination, might need adjustment for actual size
    res_dir = std::filesystem::path(path).parent_path() / "res";
    return Result::SUCCESS;
  } else {
    LOG_ERROR("Failed to find resource dir on Apple!");
    return Result::FILESYSTEM_ERROR;
  }
#endif
}

////////////////////////////
/// Math implementations ///
////////////////////////////

u64 mod_cantor(s32 a, s32 b) {
  u64 ua, ub;
  if (a < 0) {
    ua = static_cast<u64>(std::abs(a) * 2);
  } else {
    ua = static_cast<u64>(a * 2 - 1);
  }

  if (b < 0) {
    ub = static_cast<u64>(std::abs(b) * 2);
  } else {
    ub = static_cast<u64>(b * 2 - 1);
  }

  u64 result = ((ua + ub) * (ua + ub + 1)) / 2 + ub;

  return result;
}

bool Chunk_Coord::operator<(const Chunk_Coord &b) const {
  u64 a_cantor = mod_cantor(x, y);
  u64 b_cantor = mod_cantor(b.x, b.y);

  return a_cantor < b_cantor;
}

bool Chunk_Coord::operator==(const Chunk_Coord &b) const {
  return (x == b.x) && (y == b.y);
}

//////////////////////////////
/// Entity implementations ///
//////////////////////////////

Entity default_entity() {
  Entity return_entity;

  // std::memset(&return_entity, 0, sizeof(Entity));
  return_entity.coord.x = 0;
  return_entity.coord.y = 0;
  return_entity.vx = 0;
  return_entity.vy = 0;
  return_entity.ax = 0;
  return_entity.ay = 0;
  return_entity.camx = 0;
  return_entity.camy = 0;
  return_entity.boundingw = 0;
  return_entity.boundingh = 0;
  return_entity.texture = Texture_Id::NONE;  // Also 0
  return_entity.texture_index = 0;

  return return_entity;
}

Entity_Coord get_cam_coord(const Entity &e) {
  return {
      e.coord.x + e.camx,  // x
      e.coord.y + e.camy   // y
  };
}

/////////////////////////////
/// World implementations ///
/////////////////////////////

Entity_Coord get_world_pos_from_chunk(Chunk_Coord coord) {
  Entity_Coord ret_entity_coord;

  ret_entity_coord.x = coord.x * CHUNK_CELL_WIDTH;
  ret_entity_coord.y = coord.y * CHUNK_CELL_WIDTH;

  return ret_entity_coord;
}

Chunk_Coord get_chunk_coord(f64 x, f64 y) {
  Chunk_Coord return_chunk_coord;

  // floor to ensure it goes toward bottom left
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

Result gen_chunk(Chunk &chunk, const Chunk_Coord &chunk_coord) {
  if (chunk_coord.y < 0) {
    for (Cell &cell : chunk.cells) {
      cell.type = Cell_Type::DIRT;
      cell.cr = 255;
      cell.cg = 255;
      cell.cb = 0;
      cell.ca = 255;
    }
  } else {
    for (Cell &cell : chunk.cells) {
      cell.type = Cell_Type::AIR;
      cell.cr = 255;
      cell.cg = 255;
      cell.cb = 255;
      cell.ca = 255;
    }
  }

  chunk.coord = chunk_coord;

  return Result::SUCCESS;
}

Result load_chunk(Dimension &dim, const Chunk_Coord &coord) {
  // Eventually we'll also load from disk
  gen_chunk(dim.chunks[coord], coord);

  return Result::SUCCESS;
}

Result load_chunks_square(Dimension &dim, f64 x, f64 y, u8 radius) {
  Chunk_Coord origin = get_chunk_coord(x, y);

  Chunk_Coord icc;
  for (icc.x = origin.x - radius; icc.x < origin.x + radius; icc.x++) {
    for (icc.y = origin.y - radius; icc.y < origin.y + radius; icc.y++) {
      load_chunk(dim, icc);
    }
  }

  return Result::SUCCESS;
}

/////////////////////////////////
/// Rendering implementations ///
/////////////////////////////////

// Uses global config
Result init_rendering(App &app) {
  // SDL init

  // For some reason SDL_QUIT is triggered randomly on my (Levi's) system, so
  // we're not quiting on that. If SDL holds on to the handlers, we can't exit
  // with a break otherwise.
  if (!SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1")) {
    LOG_WARN("SDL didn't relinquish the signal handlers. Good luck quiting.");
  }

  int sdl_init_flags = SDL_INIT_VIDEO;

  SDL_ClearError();
  if (SDL_Init(sdl_init_flags) != 0) {
    LOG_ERROR("Failed to initialize sdl: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("SDL initialized");

  LOG_DEBUG("Config window values: {}, {}", app.config.window_width,
            app.config.window_height);

  // Window
  SDL_ClearError();
  int window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (app.config.window_start_maximized) {
    window_flags = window_flags | SDL_WINDOW_MAXIMIZED;
    LOG_DEBUG("Starting window maximized");
  }

  app.render_state.window = SDL_CreateWindow(
      "Voyages & Verve", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      app.config.window_width, app.config.window_height, window_flags);

  SDL_ClearError();
  if (app.render_state.window == nullptr) {
    LOG_ERROR("Failed to create sdl window: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("Window created");

  // Renderer
  SDL_ClearError();
  app.render_state.renderer =
      SDL_CreateRenderer(app.render_state.window, -1, 0);
  if (app.render_state.renderer == nullptr) {
    LOG_ERROR("Failed to create sdl renderer: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  // Do an initial resize to get all the base info from the screen loading
  // into the state
  Result resize_res = handle_window_resize(app.render_state);
  if (resize_res != Result::SUCCESS) {
    LOG_WARN("Failed to handle window resize! EC: {}",
             static_cast<s32>(resize_res));
  }

  // Create the world cell texture
  SDL_ClearError();
  app.render_state.cell_texture = SDL_CreateTexture(
      app.render_state.renderer, SDL_PIXELFORMAT_RGBA8888,
      SDL_TEXTUREACCESS_STREAMING, SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH,
      SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH);
  if (app.render_state.cell_texture == nullptr) {
    LOG_ERROR("Failed to create cell texture with SDL: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("Created cell texture");

  // Create the rest of the textures from resources

  Result render_tex_res = init_render_textures(app.render_state, app.config);
  if (render_tex_res != Result::SUCCESS) {
    LOG_WARN(
        "Something went wrong while generating textures from resources. "
        "Going to try to continue.");
  } else {
    LOG_INFO("Created {} resource texture(s)",
             app.render_state.textures.size());
  }

  return Result::SUCCESS;
}

Result render(Render_State &render_state, Update_State &update_state) {
  SDL_RenderClear(render_state.renderer);

  // TODO: Might want to only call this when necessary. Maybe have an event for
  // the player moving chunks
  gen_world_texture(render_state, update_state);

  render_cell_texture(render_state, update_state);
  render_entities(render_state, update_state);

  return Result::SUCCESS;
}

void destroy_rendering(Render_State &render_state) {
  if (render_state.cell_texture != nullptr) {
    SDL_DestroyTexture(render_state.cell_texture);
    LOG_INFO("Destroyed cell texture");
  }

  for (const std::pair<u8, Res_Texture> &pair : render_state.textures) {
    SDL_DestroyTexture(pair.second.texture);
  }
  LOG_INFO("Destroyed {} resource textures", render_state.textures.size());

  if (render_state.window != nullptr) {
    SDL_DestroyWindow(render_state.window);
    LOG_INFO("Destroyed SDL window");
  }

  SDL_Quit();
  LOG_INFO("Quit SDL");
}

Result init_render_textures(Render_State &render_state, const Config &config) {
  try {
    if (!std::filesystem::is_directory(config.tex_dir)) {
      LOG_ERROR("Can't initialize textures. {} is not a directory!",
                config.tex_dir.c_str());
      return Result::NONEXIST;
    }

    for (const auto &entry :
         std::filesystem::directory_iterator(config.tex_dir)) {
      if (entry.is_regular_file()) {
        std::regex pattern(
            "^([a-zA-Z0-9]+)-([0-9A-Fa-f]{2})\\.([a-zA-Z0-9]+)$");
        std::smatch matches;

        std::string filename = entry.path().filename().string();

        if (std::regex_match(filename, matches, pattern)) {
          if (matches.size() == 4) {  // matches[0] is the entire string, 1-3
                                      // are the capture groups
            std::string name = matches[1].str();
            std::string hexStr = matches[2].str();
            std::string extension = matches[3].str();

            unsigned int hexValue;
            std::stringstream ss;
            ss << std::hex << hexStr;
            ss >> hexValue;
            u8 id = static_cast<unsigned char>(hexValue);

            if (id == 0) {
              LOG_ERROR("Texture {} id can't be 0!", entry.path().c_str());
              break;
            }

            if (extension == "bmp") {
              SDL_ClearError();
              SDL_Surface *bmp_surface = SDL_LoadBMP(entry.path().c_str());
              if (bmp_surface == nullptr) {
                LOG_ERROR(
                    "Failed to create surface for bitmap texture {}. SDL "
                    "error: {}",
                    entry.path().c_str(), SDL_GetError());
                break;
              }

              // Assume no conflicting texture ids, but check after with the
              // emplacement

              Res_Texture new_tex;

              SDL_ClearError();
              new_tex.texture = SDL_CreateTextureFromSurface(
                  render_state.renderer, bmp_surface);
              SDL_FreeSurface(
                  bmp_surface);  // Shouldn't overwrite any potential
                                 // errors from texture creation
              if (new_tex.texture == nullptr) {
                LOG_ERROR(
                    "Failed to create texture for bitmap texture {}. SDL "
                    "error: {}",
                    entry.path().c_str(), SDL_GetError());
                break;
              }

              SDL_ClearError();
              if (SDL_QueryTexture(new_tex.texture, NULL, NULL, &new_tex.width,
                                   &new_tex.height) != 0) {
                LOG_ERROR(
                    "Failed to get width and height for texture {}. Guessing. "
                    "SDL error: {}",
                    id, SDL_GetError());
                break;
              }

              auto emplace_res = render_state.textures.emplace(id, new_tex);
              if (!emplace_res.second) {
                LOG_ERROR("Couldn't create texture of id {}. Already exists",
                          id);
                SDL_DestroyTexture(new_tex.texture);
                break;
              }
            }
          }
        } else {
          LOG_WARN(
              "File {} in {} doesn't match the texture format. Skipping. "
              "Should be "
              "name-XX.ext",
              filename, config.tex_dir.c_str());
        }
      }
    }
  } catch (const std::filesystem::filesystem_error &e) {
    LOG_ERROR(
        "Something went wrong on the filesystem side while creating "
        "textures: {}",
        e.what());
    return Result::FILESYSTEM_ERROR;
  } catch (const std::exception &e) {
    LOG_ERROR("General standard library error while creating textures: {}",
              e.what());
    return Result::GENERAL_ERROR;
  }

  return Result::SUCCESS;
}

Result handle_window_resize(Render_State &render_state) {
  SDL_ClearError();
  render_state.surface = SDL_GetWindowSurface(render_state.window);
  if (render_state.surface == nullptr) {
    LOG_ERROR("Failed to get sdl surface: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  SDL_GetWindowSize(render_state.window, &render_state.window_width,
                    &render_state.window_height);
  LOG_INFO("SDL window resized to {}, {}", render_state.window_width,
           render_state.window_height);

  render_state.screen_cell_size =
      render_state.window_width / (SCREEN_CELL_SIZE_FULL - SCREEN_CELL_PADDING);

  return Result::SUCCESS;
}

Result gen_world_texture(Render_State &render_state,
                         Update_State &update_state) {
  // TODO: Might be good to have this ask the world for chunks it needs if they
  // aren't loaded

  // How to generate:
  // First need to find which chunks are centered around active_player cam.
  // Then essentially write the colors of each cell to the texture. Should
  // probably multithread.
  // TODO: multithread

  Entity &active_player = *get_active_player(update_state);

  // Remember, Entity::cam is relative to the entity's position
  f64 camx, camy;
  camx = active_player.camx + active_player.coord.x;
  camy = active_player.camy + active_player.coord.y;

  Chunk_Coord center = get_chunk_coord(camx, camy);
  if (center.x < 0) {
    center.x++;
  }
  if (center.y < 0) {
    center.y++;
  }
  Chunk_Coord ic;
  Chunk_Coord ic_max;
  u8 radius = SCREEN_CHUNK_SIZE / 2;

  // NOTE (Levi): There's no overflow checking on this right now.
  // Just don't go to the edge of the world I guess.
  ic.x = center.x - radius;
  ic.y = center.y - radius;

  ic_max.x = ic.x + SCREEN_CHUNK_SIZE;
  ic_max.y = ic.y + SCREEN_CHUNK_SIZE;

  // Update the top left chunk of the texture
  render_state.tl_tex_chunk = {ic.x, ic_max.y};

  u8 chunk_x = 0;
  u8 chunk_y = 0;

  Dimension &active_dimension = *get_active_dimension(update_state);

  // LOG_DEBUG("Generating world texture");
  // For each chunk in the texture...

  u32 *pixels;
  int pitch;
  SDL_ClearError();
  if (SDL_LockTexture(render_state.cell_texture, NULL, (void **)&pixels,
                      &pitch) != 0) {
    LOG_WARN("Failed to lock cell texture for updating: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  assert(pitch == CHUNK_CELL_WIDTH * SCREEN_CHUNK_SIZE * sizeof(u32));
  constexpr u16 PITCH = CHUNK_CELL_WIDTH * SCREEN_CHUNK_SIZE;

  // For each chunk in the texture...
  u8 cr, cg, cb, ca;
  for (ic.y = center.y - radius; ic.y < ic_max.y; ic.y++) {
    for (ic.x = center.x - radius; ic.x < ic_max.x; ic.x++) {
      Chunk &chunk = active_dimension.chunks[ic];
      // if (chunk.coord != ic) {
      // LOG_WARN("Cantor mapping of chunks failed! %d, %d: %d, %d", ic.x,
      // ic.y,
      //          chunk.coord.x, chunk.coord.y);
      // }

      // assert(chunk.coord == ic);
      // if (chunk.cells[0].type == Cell_Type::AIR) {
      //   LOG_DEBUG("Chunk %d, %d is a air chunk", ic.x, ic.y);
      // } else {
      //   LOG_DEBUG("Chunk %d, %d is a dirt chunk", ic.x, ic.y);
      // }

      for (u16 cell_y = 0; cell_y < CHUNK_CELL_WIDTH; cell_y++) {
        for (u16 cell_x = 0; cell_x < CHUNK_CELL_WIDTH; cell_x++) {
          // NOTE (Levi): Everything seems to be rotated 90 clockwise when
          // entered normally into this texture, so we swap x and y and subtract
          // x from it's max to mirror
          size_t buffer_index =
              (cell_x + chunk_y * CHUNK_CELL_WIDTH) +
              ((CHUNK_CELL_WIDTH - 1) * PITCH - (cell_y * PITCH)) +
              ((SCREEN_CHUNK_SIZE - 1) * CHUNK_CELL_WIDTH * PITCH -
               (chunk_x * CHUNK_CELL_WIDTH * PITCH));
          size_t chunk_index = chunk_y + chunk_x * CHUNK_CELL_WIDTH;

#ifndef NDEBUG
          if (cell_y == 0 && cell_x == 0) {
            cr = 255;
            cg = 0;
            cb = 0;
            ca = 255;
          } else {
            cr = chunk.cells[chunk_index].cr;
            cg = chunk.cells[chunk_index].cg;
            cb = chunk.cells[chunk_index].cb;
            ca = chunk.cells[chunk_index].ca;
          }
#else
          cr = chunk.cells[chunk_index].cr;
          cg = chunk.cells[chunk_index].cg;
          cb = chunk.cells[chunk_index].cb;
          ca = chunk.cells[chunk_index].ca;
#endif
          pixels[buffer_index] = (cr << 24) | (cg << 16) | (cb << 8) | ca;
          if (buffer_index >
              SCREEN_CHUNK_SIZE * SCREEN_CHUNK_SIZE * CHUNK_CELLS - 1) {
            LOG_ERROR(
                "Somehow surpassed the texture size while generating: {} "
                "cell texture chunk_x: {}, chunk_y: {}, cell_x: {}, cell_y: "
                "{}",
                buffer_index, chunk_x, chunk_y, cell_x, cell_y);
          }
          assert(buffer_index <=
                 SCREEN_CHUNK_SIZE * SCREEN_CHUNK_SIZE * CHUNK_CELLS - 1);
        }
      }

      chunk_y++;
    }
    chunk_y = 0;
    chunk_x++;
  }

  SDL_UnlockTexture(render_state.cell_texture);

  return Result::SUCCESS;
}

Result render_cell_texture(Render_State &render_state,
                           Update_State &update_state) {
  Entity &active_player = *get_active_player(update_state);

  u16 screen_cell_size = render_state.screen_cell_size;

  Entity_Coord tl_chunk = get_world_pos_from_chunk(render_state.tl_tex_chunk);
  tl_chunk.y--;  // This is what makes it TOP left instead of bottom left

  // This is where the top left of the screen should be in world coordinates
  Entity_Coord good_tl_chunk;
  good_tl_chunk.x = active_player.camx + active_player.coord.x;
  good_tl_chunk.x -= (render_state.window_width / 2.0f) / screen_cell_size;

  good_tl_chunk.y = active_player.camy + active_player.coord.y;
  good_tl_chunk.y += (render_state.window_height / 2.0f) / screen_cell_size;

  s32 offset_x = (good_tl_chunk.x - tl_chunk.x) * screen_cell_size * -1;
  s32 offset_y = (tl_chunk.y - good_tl_chunk.y) * screen_cell_size * -1;

  /*
#ifndef NDEBUG
  if (offset_y > 0 || offset_x > 0) {
    LOG_WARN("Texture appears to be copied incorrectly. One of the offsets are "
             "above 0: x:{}, y:{}",
             offset_x, offset_y);
  }
#endif
*/

  s32 width = screen_cell_size * SCREEN_CELL_SIZE_FULL;
  s32 height = screen_cell_size * SCREEN_CELL_SIZE_FULL;

  SDL_Rect destRect = {offset_x, offset_y, width, height};
  // SDL_Rect destRect = {10 * screen_cell_size, 10 * screen_cell_size,
  //                      SCREEN_CELL_SIZE_FULL * screen_cell_size,
  //                      SCREEN_CELL_SIZE_FULL * screen_cell_size};

  SDL_RenderCopy(render_state.renderer, render_state.cell_texture, NULL,
                 &destRect);

  return Result::SUCCESS;
}

Result render_entities(Render_State &render_state, Update_State &update_state) {
  Dimension &active_dimension = *get_active_dimension(update_state);
  static std::set<Texture_Id> suppressed_id_warns;

  u16 screen_cell_size = render_state.screen_cell_size;
  Entity &active_player = *get_active_player(update_state);

  Entity_Coord tl;
  tl.x = active_player.camx + active_player.coord.x;
  tl.x -= (render_state.window_width / 2.0f) / screen_cell_size;

  tl.y = active_player.camy + active_player.coord.y;
  tl.y += (render_state.window_height / 2.0f) / screen_cell_size;

  for (size_t entity_index : active_dimension.entity_indicies) {
    Entity &entity = update_state.entities[entity_index];

    if (entity.texture != Texture_Id::NONE) {
      auto sdk_texture =
          render_state.textures.find(static_cast<u8>(entity.texture));

      if (sdk_texture == render_state.textures.end()) {
        if (!suppressed_id_warns.contains(entity.texture)) {
          LOG_WARN("Entity want's texture {} which isn't loaded!",
                   (u8)entity.texture);
          suppressed_id_warns.insert(entity.texture);
        }
      } else {
        // Now it's time to render!

        Res_Texture &texture = sdk_texture->second;

        // LOG_DEBUG("entity coord: {} {}", entity.coord.x, entity.coord.y);
        // LOG_DEBUG("tl: {} {}", tl.x, tl.y);

        Entity_Coord world_offset;
        world_offset.x = entity.coord.x - tl.x;
        world_offset.y = tl.y - entity.coord.y;

        // LOG_DEBUG("World offset: {} {}", world_offset.x, world_offset.y);

        // If visable
        if (world_offset.x >= -texture.width &&
            world_offset.x <=
                SCREEN_CELL_SIZE_FULL - SCREEN_CELL_PADDING + texture.width &&
            world_offset.y >= -texture.height &&
            world_offset.y <= static_cast<s32>(render_state.window_height /
                                               render_state.screen_cell_size) +
                                  texture.height) {
          SDL_Rect dest_rect = {
              (int)(world_offset.x * render_state.screen_cell_size),
              (int)(world_offset.y * render_state.screen_cell_size),
              texture.width * screen_cell_size,
              texture.height * screen_cell_size};

          SDL_RenderCopy(render_state.renderer, texture.texture, NULL,
                         &dest_rect);
        }
      }
    }
  }

  return Result::SUCCESS;
}

//////////////////////////////
/// Update implementations ///
//////////////////////////////

Result init_updating(Update_State &update_state) {
  update_state.dimensions.emplace(DimensionIndex::OVERWORLD, Dimension());
  update_state.active_dimension = DimensionIndex::OVERWORLD;

  update_state.active_player =
      create_player(update_state, update_state.active_dimension);

  Entity &active_player = *get_active_player(update_state);

  Dimension &active_dimension = *get_active_dimension(update_state);
  active_dimension.entity_indicies.push_back(update_state.active_player);

  load_chunks_square(*get_active_dimension(update_state), active_player.coord.x,
                     active_player.coord.y, SCREEN_CHUNK_SIZE / 2);

  return Result::SUCCESS;
}

Result update(Update_State &update_state) {
  Entity &active_player = *get_active_player(update_state);
  Dimension &active_dimension = *get_active_dimension(update_state);

  update_kinetic(update_state);

  return Result::SUCCESS;
}

void update_kinetic(Update_State &update_state) {
  Dimension &active_dimension = *get_active_dimension(update_state);

  // TODO: Multithread

  // Start by updating kinetics values: acc, vel, pos
  for (size_t entity_index : active_dimension.entity_indicies) {
    // Have to trust that entity_indicies is correct at them moment.
    Entity &entity = update_state.entities[entity_index];

    // If not 0, move toward 0. Only other forces increase acceleration
    // No epsilon since it will be explicitly set to zero
    if (entity.ax != 0.0f) {
      if (entity.ax > 0) {
        entity.ax -= KINETIC_FRICTION;

        if (entity.ax <= 0.0f) {  // If we flipped signs, set to 0
          entity.ax = 0.0f;
        }
      } else {
        entity.ax += KINETIC_FRICTION;

        if (entity.ax > 0.0f) {
          entity.ax = 0.0f;
        }
      }
    }

    if (entity.ay != 0.0f) {
      if (entity.ay > 0) {
        entity.ay -= KINETIC_FRICTION;

        if (entity.ay <= 0.0f) {  // If we flipped signs, set to 0
          entity.ay = 0.0f;
        }
      } else {
        entity.ay += KINETIC_FRICTION;

        if (entity.ay > 0.0f) {
          entity.ay = 0.0f;
        }
      }
    }

    // Velocity should also tend toward 0
    entity.vx += entity.ax;
    entity.vy += entity.ay;
    entity.coord.x += entity.vx;
    entity.coord.y += entity.vy;
  }

  // Now resolve colisions

  // TODO: this just does cell collisions. We need some kind of spacial data
  // sctructure to determine if we're colliding with other entities
  for (size_t entity_index : active_dimension.entity_indicies) {
    Entity &entity = update_state.entities[entity_index];

    Chunk_Coord cc = get_chunk_coord(entity.coord.x, entity.coord.y);
    static Chunk_Coord last_chunk = cc;

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
          if (chunk.cells[cell].type == Cell_Type::DIRT) {
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

            // If neither, we are colliding. Lets resolve it in a really basic
            // way here for now.

            // This will cause it to bounce back the way it came but oh well
            entity.ax = entity.ax * -0.25;  // Decrease and go back
            entity.ay = entity.ay * -0.25;
            entity.vx = entity.vx * -0.25;  // Decrease and go back
            entity.vy = entity.vy * -0.25;
            entity.coord.x += entity.vx;
            entity.coord.y += entity.vy;
          }
        }
      }
    }
  }
}

u32 create_entity(Update_State &us, DimensionIndex dim) {
  Entity e = default_entity();

  us.entities.push_back(e);

  u32 entity_index = us.entities.size() - 1;
  us.dimensions[dim].entity_indicies.push_back(entity_index);

  return entity_index;
}

u32 create_player(Update_State &us, DimensionIndex dim) {
  Entity player = default_entity();

  player.texture = Texture_Id::PLAYER;
  player.coord.y = 65;
  player.coord.x = 15;
  player.camy -= 20;
  player.ay = -1.03;

  // TODO: Should be in a resource description file. This will be different than
  // texture width and height. Possibly with offsets. Maybe multiple bounding
  // boxes
  player.boundingw = 11;
  player.boundingh = 29;

  us.entities.push_back(player);
  u32 entity_index = us.entities.size() - 1;
  us.dimensions[dim].entity_indicies.push_back(entity_index);

  return entity_index;
}

Dimension *get_active_dimension(Update_State &update_state) {
  return &update_state.dimensions.at(update_state.active_dimension);
}

Entity *get_active_player(Update_State &update_state) {
  return &update_state.entities[update_state.active_player];
}

/////////////////////////////
/// State implementations ///
/////////////////////////////

Result poll_events(App &app) {
  Render_State &render_state = app.render_state;

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_WINDOWEVENT: {
        if (event.window.windowID == SDL_GetWindowID(render_state.window)) {
          switch (event.window.event) {
            case SDL_WINDOWEVENT_CLOSE: {
              return Result::WINDOW_CLOSED;
            }
            case SDL_WINDOWEVENT_RESIZED: {
              handle_window_resize(app.render_state);
            }
          }
        }
      }
      case SDL_QUIT: {
        // LOG_DEBUG("Got event SDL_QUIT. Returning Result::WINDOW_CLOSED");
        // return Result::WINDOW_CLOSED;
        LOG_DEBUG("Got event SDL_QUIT but don't trust it. Continuing.");
      }
    }
  }

  return Result::SUCCESS;
}

Result init_app(App &app) {
  spdlog::set_level(spdlog::level::debug);

  app.config = default_config();

  std::filesystem::path res_dir;
  Result res_dir_res = get_resource_dir(app.config.res_dir);
  if (res_dir_res != Result::SUCCESS) {
    LOG_FATAL("Couldn't find resource dir! Exiting...");
    return res_dir_res;
  } else {
    LOG_INFO("Resource dir found at {}", app.config.res_dir.c_str());
  }

  app.config.tex_dir = app.config.res_dir / "textures";

  init_updating(app.update_state);
  Result renderer_res = init_rendering(app);
  if (renderer_res != Result::SUCCESS) {
    LOG_FATAL("Failed to initialize renderer. Exiting.");
    return renderer_res;
  }

  return Result::SUCCESS;
}

Result run_app(App &app) {
  while (true) {
    // Events
    Result poll_result = poll_events(app);
    if (poll_result == Result::WINDOW_CLOSED) {
      LOG_INFO("Window should close.");
      return Result::SUCCESS;
    }

    // Update
    update(app.update_state);

    // Render. This just draws, the flip is after the delay
    render(app.render_state, app.update_state);
    // TODO: This should delay the remaining time to make this frame 1/60 of a
    // second
    SDL_Delay(1);

    SDL_RenderPresent(app.render_state.renderer);
  }

  return Result::SUCCESS;
}

void destroy_app(App &app) {
  destroy_rendering(app.render_state);
}
}  // namespace VV
