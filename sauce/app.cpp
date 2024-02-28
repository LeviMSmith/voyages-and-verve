#include "app.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <numeric>
#include <regex>
#include <set>
#include <stdexcept>
#include <unordered_set>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_pixels.h"
#include "SDL_render.h"
#include "SDL_surface.h"
#include "SDL_timer.h"
#include "SDL_video.h"

// Platform specific includes
#ifdef _WIN32
#include <windows.h>
#undef min  // SDL has a macro for this on windows for some reason
#elif defined(__linux__)
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
      600,    // window_width
      400,    // window_height
      true,   // window_start_maximized
      false,  // show_chunk_corners
      "",     // res_dir: Should be set by caller
      "",     // tex_dir: set with res_dir
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

bool Chunk_Coord::operator<(const Chunk_Coord &b) const {
  u64 a_hash =
      (static_cast<u64>(static_cast<u32>(y)) << 32) | static_cast<u32>(x);
  u64 b_hash =
      (static_cast<u64>(static_cast<u32>(b.y)) << 32) | static_cast<u32>(b.x);

  return a_hash < b_hash;
}

bool Chunk_Coord::operator==(const Chunk_Coord &b) const {
  return (x == b.x) && (y == b.y);
}

//////////////////////////////
/// Entity implementations ///
//////////////////////////////

Entity default_entity() {
  Entity return_entity;

  std::memset(&return_entity, 0, sizeof(Entity));
  /*
  return_entity.coord.x = 0;
  return_entity.coord.y = 0;
  return_entity.vx = 0;
  return_entity.vy = 0;
  return_entity.ax = 0;
  return_entity.ay = 0;
  return_entity.status = 0;
  return_entity.camx = 0;
  return_entity.camy = 0;
  return_entity.boundingw = 0;
  return_entity.boundingh = 0;
  return_entity.texture = Texture_Id::NONE;  // Also 0
  return_entity.texture_index = 0;
  */

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

const Cell_Type_Info CELL_TYPE_INFOS[CELL_TYPE_NUM] = {
    {
        255,    // solidity
        0.70f,  // friction
    },          // DIRT
    {
        0,     // solidity
        0.8f,  // friction
    },         // AIR
    {
        50,     // solidity
        0.90f,  // friction
    },          // WATER
};

Cell default_dirt_cell() {
  Cell cell;
  cell.type = Cell_Type::DIRT;
  cell.cr = 99 + std::rand() % 12;
  cell.cg = 80 + std::rand() % 12;
  cell.cb = 79 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

Cell default_air_cell() {
  return {
      Cell_Type::AIR,  // type
      255,             // cr
      255,             // cg
      255,             // cb
      0,               // ca
  };
}

Cell default_water_cell() {
  return {
      Cell_Type::WATER,  // type
      0,                 // cr
      0,                 // cg
      255,               // cb
      200,               // ca
  };
}

Cell default_grass_cell() {
  Cell cell;
  cell.type = Cell_Type::DIRT;
  cell.cr = 8 + std::rand() % 12;
  cell.cg = 94 + std::rand() % 12;
  cell.cb = 11 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

Cell default_sand_cell() {
  Cell cell;
  cell.type = Cell_Type::DIRT;
  cell.cr = 214 + std::rand() % 12;
  cell.cg = 185 + std::rand() % 12;
  cell.cb = 105 + std::rand() % 12;
  cell.ca = 255;

  return cell;
}

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
                          f64 randomness_scale) {
  u16 height = y1 + static_cast<u16>((y2 - y1) * fraction);

  f64 divisor = SURFACE_CELL_RANGE * randomness_scale;
  s16 divisor_as_int = static_cast<s16>(divisor);

  if (divisor_as_int != 0) {
    u16 rand = surface_det_rand(seed);
    s16 nudge = static_cast<s16>(rand) % divisor_as_int;
    height += nudge;
  }

  return std::min(SURFACE_CELL_RANGE, height);
}

u16 surface_height(s64 x, u16 max_depth, u32 world_seed) {
  static std::map<s64, u16> heights;
  static const u64 RANDOMNESS_RANGE = CHUNK_CELL_WIDTH * 64;

  auto height_iter = heights.find(x);
  if (height_iter != heights.end()) {
    return height_iter->second;
  }

  if (x % RANDOMNESS_RANGE == 0) {
    u16 height =
        surface_det_rand(static_cast<u64>(x ^ world_seed)) % SURFACE_CELL_RANGE;
    heights[x] = height;
    return height;
  }

  s64 lower_x = static_cast<s64>(x / RANDOMNESS_RANGE) * RANDOMNESS_RANGE;
  s64 upper_x = lower_x + RANDOMNESS_RANGE;

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
                                    0.5 / std::pow(depth, 2.5));
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
  u16 height = interpolate_and_nudge(lower_height, upper_height, fraction,
                                     static_cast<u64>(x ^ world_seed),
                                     0.5 / std::pow(max_depth, 2.5));
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

/////////////////////////////////
/// Rendering implementations ///
/////////////////////////////////

// Uses global config
Result init_rendering(Render_State &render_state, Config &config) {
  // SDL init

  // I think this was just because I forgot break statements in the switch.
  /*
  // For some reason SDL_QUIT is triggered randomly on my (Levi's) system, so
  // we're not quiting on that. If SDL holds on to the handlers, we can't exit
  // with a break otherwise.
  if (!SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1")) {
    LOG_WARN("SDL didn't relinquish the signal handlers. Good luck quiting.");
  }
  */

  int sdl_init_flags = SDL_INIT_VIDEO;

  SDL_ClearError();
  if (SDL_Init(sdl_init_flags) != 0) {
    LOG_ERROR("Failed to initialize sdl: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("SDL initialized");

  LOG_DEBUG("Config window values: {}, {}", config.window_width,
            config.window_height);

  // Window
  SDL_ClearError();
  int window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (config.window_start_maximized) {
    window_flags = window_flags | SDL_WINDOW_MAXIMIZED;
    LOG_DEBUG("Starting window maximized");
  }

  render_state.window = SDL_CreateWindow(
      "Voyages & Verve", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      config.window_width, config.window_height, window_flags);

  SDL_ClearError();
  if (render_state.window == nullptr) {
    LOG_ERROR("Failed to create sdl window: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("Window created");

  // Renderer
  SDL_ClearError();
  int renderer_flags = SDL_RENDERER_ACCELERATED;
  render_state.renderer =
      SDL_CreateRenderer(render_state.window, -1, renderer_flags);
  if (render_state.renderer == nullptr) {
    LOG_ERROR("Failed to create sdl renderer: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  // Do an initial resize to get all the base info from the screen loading
  // into the state
  Result resize_res = handle_window_resize(render_state);
  if (resize_res != Result::SUCCESS) {
    LOG_WARN("Failed to handle window resize! EC: {}",
             static_cast<s32>(resize_res));
  }

  // Create the world cell texture
  SDL_ClearError();
  render_state.cell_texture = SDL_CreateTexture(
      render_state.renderer, SDL_PIXELFORMAT_RGBA8888,
      SDL_TEXTUREACCESS_STREAMING, SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH,
      SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH);
  if (render_state.cell_texture == nullptr) {
    LOG_ERROR("Failed to create cell texture with SDL: {}", SDL_GetError());
    return Result::SDL_ERROR;
  }

  SDL_SetTextureBlendMode(render_state.cell_texture, SDL_BLENDMODE_BLEND);

  LOG_INFO("Created cell texture");

  // Create the rest of the textures from resources

  Result render_tex_res = init_render_textures(render_state, config);
  if (render_tex_res != Result::SUCCESS) {
    LOG_WARN(
        "Something went wrong while generating textures from resources. "
        "Going to try to continue.");
  } else {
    LOG_INFO("Created {} resource texture(s)", render_state.textures.size());
  }

  // Load fonts
  if (TTF_Init() == -1) {
    LOG_ERROR("Failed to initialize SDL_ttf: {}", TTF_GetError());
    return Result::SDL_ERROR;
  }

  std::filesystem::path main_font_path =
      config.res_dir / "fonts" / "dotty" / "dotty.ttf";
  render_state.main_font = TTF_OpenFont(main_font_path.string().c_str(), 48);
  if (render_state.main_font == nullptr) {
    LOG_ERROR("Failed to load main font: {}", TTF_GetError());
    return Result::SDL_ERROR;
  }

  return Result::SUCCESS;
}

Result render(Render_State &render_state, Update_State &update_state,
              const Config &config) {
  static u64 frame = 0;
  Entity &active_player = *get_active_player(update_state);

  SDL_RenderClear(render_state.renderer);

  // Sky
  SDL_RenderCopy(render_state.renderer,
                 render_state.textures[(u8)Texture_Id::SKY].texture, NULL,
                 NULL);

  // Mountains
  if (update_state.active_dimension == DimensionIndex::OVERWORLD) {
    Res_Texture &mountain_tex =
        render_state.textures[(u8)Texture_Id::MOUNTAINS];
    SDL_Rect dest_rect = {
        static_cast<int>(active_player.coord.x * -0.1) -
            static_cast<int>(
                ((mountain_tex.width * render_state.screen_cell_size) -
                 render_state.window_width) *
                0.5),
        (render_state.window_height -
         (mountain_tex.height * render_state.screen_cell_size) + 128),
        mountain_tex.width * render_state.screen_cell_size,
        mountain_tex.height * render_state.screen_cell_size};

    SDL_RenderCopy(render_state.renderer,
                   render_state.textures[(u8)Texture_Id::MOUNTAINS].texture,
                   NULL, &dest_rect);
  }

  // Cells and entities
  if (update_state.events.contains(Update_Event::PLAYER_MOVED_CHUNK)) {
    gen_world_texture(render_state, update_state, config);
  }

  render_entities(render_state, update_state, INT8_MIN, 0);
  render_entities(render_state, update_state);
  render_cell_texture(render_state, update_state);

  // Debug overlay
  static int w = 0, h = 0;
  if (frame % 20 == 0 && config.debug_overlay) {
    refresh_debug_overlay(render_state, update_state, w, h);
  }

  if (config.debug_overlay && render_state.debug_overlay_texture != nullptr) {
    SDL_Rect dest_rect = {0, 0, w, h};
    SDL_RenderCopy(render_state.renderer, render_state.debug_overlay_texture,
                   NULL, &dest_rect);
  }

  frame++;

  return Result::SUCCESS;
}

void destroy_rendering(Render_State &render_state) {
  if (render_state.main_font != nullptr) {
    TTF_CloseFont(render_state.main_font);
    LOG_INFO("Closed main font");
  }

  TTF_Quit();
  LOG_INFO("Quit SDL_ttf");

  if (render_state.cell_texture != nullptr) {
    SDL_DestroyTexture(render_state.cell_texture);
    LOG_INFO("Destroyed cell texture");
  }

  for (const std::pair<const u8, Res_Texture> &pair : render_state.textures) {
    SDL_DestroyTexture(pair.second.texture);
  }
  LOG_INFO("Destroyed {} resource textures", render_state.textures.size());

  if (render_state.debug_overlay_texture != nullptr) {
    SDL_DestroyTexture(render_state.debug_overlay_texture);
    LOG_INFO("Destroyed debug overlay texture");
  }

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
                config.tex_dir.string());
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
            u8 id = static_cast<u8>(hexValue);

            if (id == 0) {
              LOG_ERROR("Texture {} id can't be 0!", entry.path().string());
              break;
            }

            if (extension == "bmp") {
              SDL_ClearError();
#ifdef __linux__
              SDL_Surface *bmp_surface = SDL_LoadBMP(entry.path().c_str());
#elif defined _WIN32
              SDL_Surface *bmp_surface =
                  SDL_LoadBMP(entry.path().string().c_str());
#endif
              if (bmp_surface == nullptr) {
                LOG_ERROR(
                    "Failed to create surface for bitmap texture {}. SDL "
                    "error: {}",
                    entry.path().string(), SDL_GetError());
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
                    entry.path().string(), SDL_GetError());
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
              filename, config.tex_dir.string());
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
    // We never actually use this surface so if it doesn't work keep going
    LOG_ERROR("Failed to get sdl surface: {}", SDL_GetError());
    // return Result::SDL_ERROR;
  }

  SDL_GetWindowSize(render_state.window, &render_state.window_width,
                    &render_state.window_height);
  LOG_INFO("SDL window resized to {}, {}", render_state.window_width,
           render_state.window_height);

  render_state.screen_cell_size =
      render_state.window_width / (SCREEN_CELL_SIZE_FULL - SCREEN_CELL_PADDING);

  return Result::SUCCESS;
}

Result gen_world_texture(Render_State &render_state, Update_State &update_state,
                         const Config &config) {
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
#ifndef NDEBUG
      if (chunk.coord != ic) {
        LOG_WARN(
            "Mapping of chunks failed! key: {}, {} chunk recieved: {}, "
            "{}",
            ic.x, ic.y, chunk.coord.x, chunk.coord.y);
      }
#endif

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
              (SCREEN_CHUNK_SIZE - 1 - chunk_y) * CHUNK_CELL_WIDTH * PITCH +
              ((CHUNK_CELL_WIDTH - 1 - cell_y) * PITCH) +
              (chunk_x * CHUNK_CELL_WIDTH) + cell_x;

          // Really need to get some unit tests going lol
#ifndef NDEBUG
          if (chunk_y == SCREEN_CHUNK_SIZE - 1 && chunk_x == 0 &&
              cell_y == CHUNK_CELL_WIDTH - 1 && cell_x == 0) {
            assert(buffer_index == 0);
          }

          if (chunk_y == 0 && chunk_x == 0 && cell_y == 0 && cell_x == 0) {
            assert(buffer_index == PITCH * PITCH - PITCH);
          }
#endif

          size_t cell_index = cell_x + cell_y * CHUNK_CELL_WIDTH;

          if (config.debug_overlay) {
            if (cell_y == 0 && cell_x == 0) {
              cr = 255;
              cg = 0;
              cb = 0;
              ca = 255;
            } else {
              cr = chunk.cells[cell_index].cr;
              cg = chunk.cells[cell_index].cg;
              cb = chunk.cells[cell_index].cb;
              ca = chunk.cells[cell_index].ca;
            }
          } else {
            cr = chunk.cells[cell_index].cr;
            cg = chunk.cells[cell_index].cg;
            cb = chunk.cells[cell_index].cb;
            ca = chunk.cells[cell_index].ca;
          }
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

      chunk_x++;
    }
    chunk_x = 0;
    chunk_y++;
  }

  SDL_UnlockTexture(render_state.cell_texture);

  return Result::SUCCESS;
}

Result refresh_debug_overlay(Render_State &render_state,
                             const Update_State &update_state, int &w, int &h) {
  f64 x, y;
  x = update_state.entities[update_state.active_player].coord.x;
  y = update_state.entities[update_state.active_player].coord.y;
  u8 status = update_state.entities[update_state.active_player].status;

  render_state.debug_info = std::format(
      "FPS: {:.1f} | Dimension id: {} Chunks loaded in dim {} | Player pos: "
      "{:.2f}, {:.2f} Status: {} | World seed {:08x}",
      update_state.average_fps, (u8)update_state.active_dimension,
      update_state.dimensions.at(update_state.active_dimension).chunks.size(),
      x, y, status, update_state.world_seed);

  if (render_state.debug_overlay_texture != nullptr) {
    SDL_DestroyTexture(render_state.debug_overlay_texture);
  }

  const SDL_Color do_fcolor = {255, 255, 255, 255};
  SDL_Surface *do_surface = TTF_RenderText_Blended(
      render_state.main_font, render_state.debug_info.c_str(), do_fcolor);
  if (do_surface == nullptr) {
    LOG_WARN("Failed to render debug info to a surface {}", TTF_GetError());
    return Result::SDL_ERROR;
  }

  w = do_surface->w;
  h = do_surface->h;

  SDL_ClearError();
  render_state.debug_overlay_texture =
      SDL_CreateTextureFromSurface(render_state.renderer, do_surface);
  if (render_state.debug_overlay_texture == nullptr) {
    LOG_WARN("Failed to create texture from debug overlay surface: {}",
             SDL_GetError());
    SDL_FreeSurface(do_surface);
    return Result::SDL_ERROR;
  }

  SDL_FreeSurface(do_surface);

  return Result::SUCCESS;
}

/*
Result render_trees(Render_State &rs, Update_State &us) {
  return Result::SUCCESS;
}
*/

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

Result render_entities(Render_State &render_state, Update_State &update_state,
                       Entity_Z z_min, Entity_Z z_thresh) {
  Dimension &active_dimension = *get_active_dimension(update_state);
  static std::set<Texture_Id> suppressed_id_warns;

  u16 screen_cell_size = render_state.screen_cell_size;
  Entity &active_player = *get_active_player(update_state);

  Entity_Coord tl;
  tl.x = active_player.camx + active_player.coord.x;
  tl.x -= (render_state.window_width / 2.0f) / screen_cell_size;

  tl.y = active_player.camy + active_player.coord.y;
  tl.y += (render_state.window_height / 2.0f) / screen_cell_size;

  for (const auto &[z, entity_index] : active_dimension.e_render) {
    if (z > z_thresh || z < z_min) {
      continue;
    }
    Entity &entity = update_state.entities[entity_index];

    auto sdk_texture =
        render_state.textures.find(static_cast<u8>(entity.texture));

    if (sdk_texture == render_state.textures.end()) {
      if (!suppressed_id_warns.contains(entity.texture)) {
        LOG_WARN("Entity wants texture {} which isn't loaded!",
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

      if (entity.status & (u8)Entity_Status::ANIMATED) {
        if (world_offset.x >= -entity.anim_width &&
            world_offset.x <= SCREEN_CELL_SIZE_FULL - SCREEN_CELL_PADDING +
                                  entity.anim_width &&
            world_offset.y >= -texture.height &&
            world_offset.y <= static_cast<s32>(render_state.window_height /
                                               render_state.screen_cell_size) +
                                  texture.height) {
          SDL_Rect src_rect = {
              entity.anim_width * entity.anim_current_frame, 0,
              entity.anim_width * (entity.anim_current_frame + 1),
              texture.height};

          SDL_Rect dest_rect = {
              (int)(world_offset.x * render_state.screen_cell_size),
              (int)(world_offset.y * render_state.screen_cell_size),
              entity.anim_width * screen_cell_size,
              texture.height * screen_cell_size};

          if (entity.flipped) {
            SDL_RenderCopyEx(render_state.renderer, texture.texture, &src_rect,
                             &dest_rect, 0, NULL, SDL_FLIP_HORIZONTAL);
          } else {
            SDL_RenderCopy(render_state.renderer, texture.texture, &src_rect,
                           &dest_rect);
          }
        }

        if (entity.anim_timer > entity.anim_delay) {
          entity.anim_current_frame = (entity.anim_current_frame + 1) %
                                      (texture.width / entity.anim_width);
          entity.anim_timer = 0;
        }
        entity.anim_timer++;
      } else {
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

          if (entity.flipped) {
            SDL_RenderCopyEx(render_state.renderer, texture.texture, NULL,
                             &dest_rect, 0, NULL, SDL_FLIP_HORIZONTAL);
          } else {
            SDL_RenderCopy(render_state.renderer, texture.texture, NULL,
                           &dest_rect);
          }
        }
      }
    }
  }

  return Result::SUCCESS;
}

//////////////////////////////
/// Update implementations ///
//////////////////////////////

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
                     active_player.coord.x, active_player.coord.y,
                     SCREEN_CHUNK_SIZE / 2);

  return Result::SUCCESS;
}

Result update(Update_State &update_state) {
  update_state.events.clear();

  Entity &active_player = *get_active_player(update_state);
  static Chunk_Coord last_player_chunk =
      get_chunk_coord(active_player.coord.x, active_player.coord.y);

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
                       active_player.coord.x, active_player.coord.y,
                       SCREEN_CHUNK_SIZE + 5);
    last_player_chunk = current_player_chunk;
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
                      std::min(fabs(overlap_x), MOV_LIM);  // Move entity left
                } else {
                  entity.coord.x += std::min(fabs(overlap_x),
                                             MOV_LIM);  // Move entity right
                }
              } else {
                if (entity.coord.y > cell_coord.y) {
                  entity.coord.y +=
                      std::min(fabs(overlap_y), MOV_LIM);  // Move entity down
                } else {
                  entity.coord.y -=
                      std::min(fabs(overlap_y), MOV_LIM);  // Move entity up
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

        if (surface_det_rand(static_cast<u64>(abs_x) ^
                             update_state.world_seed) %
                    GEN_TREE_MAX_WIDTH <
                15 &&
            height > chunk_coord.y * CHUNK_CELL_WIDTH &&
            height < (chunk_coord.y + 1) * CHUNK_CELL_WIDTH &&
            height >= SEA_LEVEL_CELL) {
          Entity_ID id;
          create_tree(update_state, update_state.active_dimension, id);

          Entity &tree = update_state.entities[id];
          tree.coord.x = abs_x;
          tree.coord.y = height + 85.0f;
        }

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

Dimension *get_active_dimension(Update_State &update_state) {
#ifndef NDEBUG
  auto dim_iter = update_state.dimensions.find(update_state.active_dimension);
  assert(dim_iter != update_state.dimensions.end());
  return &dim_iter->second;
#else
  return &update_state.dimensions.at(update_state.active_dimension);
#endif
}

Entity *get_active_player(Update_State &update_state) {
  return &update_state.entities[update_state.active_player];
}

/////////////////////////////
/// State implementations ///
/////////////////////////////

Result handle_args(int argv, const char **argc,
                   std::optional<u32> &world_seed) {
  if (argv < 1 || argv > 2) {
    LOG_WARN("Bad number of args {}", argv);
    return Result::BAD_ARGS_ERROR;
  }

  if (argv == 2) {
    try {
      size_t num_chars = 0;
      u32 stoul_res = std::stoul(argc[1], &num_chars, 16);
      LOG_DEBUG("Using argument \"{}\" as seed with {} characters. Hex: {:x}",
                argc[1], num_chars, stoul_res);
      world_seed.emplace(stoul_res);
    } catch (const std::invalid_argument &e) {
      LOG_WARN("Couldn't convert argument {} to an unsigned long: {}", argc[1],
               e.what());
      return Result::BAD_ARGS_ERROR;
    } catch (const std::out_of_range &e) {
      LOG_WARN("Couldn't convert argument {} to an unsigned long: {}", argc[1],
               e.what());
      return Result::BAD_ARGS_ERROR;
    }
  }

  return Result::SUCCESS;
}

Result poll_events(App &app) {
  Render_State &render_state = app.render_state;

  static bool debug_key_pressed = false;
  const auto DEBUG_KEY = SDLK_F3;

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
        break;
      }
      case SDL_QUIT: {
        LOG_DEBUG(
            "Got event SDL_QUIT. Returning "
            "Result::WINDOW_CLOSED");
        return Result::WINDOW_CLOSED;
        break;
      }
      case SDL_KEYDOWN: {
        if (event.key.keysym.sym == DEBUG_KEY) {
          debug_key_pressed = true;
        }
        break;
      }
      case SDL_KEYUP: {
        if (event.key.keysym.sym == DEBUG_KEY && debug_key_pressed) {
          app.config.debug_overlay = !app.config.debug_overlay;
          debug_key_pressed = false;
        }
        break;
      }
    }
  }

  return Result::SUCCESS;
}  // namespace VV

Result init_app(App &app, int argv, const char **argc) {
  spdlog::set_level(spdlog::level::debug);

  app.config = default_config();

  std::filesystem::path res_dir;
  Result res_dir_res = get_resource_dir(app.config.res_dir);
  if (res_dir_res != Result::SUCCESS) {
    LOG_FATAL("Couldn't find resource dir! Exiting...");
    return res_dir_res;
  } else {
    LOG_INFO("Resource dir found at {}", app.config.res_dir.string());
  }

  app.config.tex_dir = app.config.res_dir / "textures";

  std::optional<u32> world_seed;
  Result args_res = handle_args(argv, argc, world_seed);
  if (args_res == Result::BAD_ARGS_ERROR) {
    LOG_FATAL("Argument handling failed. Exiting.");
    return args_res;
  }

  Result update_res = init_updating(app.update_state, world_seed);
  if (update_res != Result::SUCCESS) {
    LOG_FATAL("Failed to initialize updater. Exiting.");
    return update_res;
  }

  Result renderer_res = init_rendering(app.render_state, app.config);
  if (renderer_res != Result::SUCCESS) {
    LOG_FATAL("Failed to initialize renderer. Exiting.");
    return renderer_res;
  }

  LOG_INFO("Using world seed {:08x} (hex)", app.update_state.world_seed);

  return Result::SUCCESS;
}

Result run_app(App &app) {
  std::deque<double> frame_times;
  const size_t max_frame_history = 20;

  while (true) {
    auto frame_start = std::chrono::steady_clock::now();
    // Events
    Result poll_result = poll_events(app);
    if (poll_result == Result::WINDOW_CLOSED) {
      LOG_INFO("Window should close.");
      return Result::SUCCESS;
    }

    // Update
    Result update_res = update(app.update_state);
    if (update_res == Result::WINDOW_CLOSED) {
      LOG_INFO("Window should close.");
      return Result::SUCCESS;
    }

    // Render. This just draws, the flip is after the delay
    render(app.render_state, app.update_state, app.config);

    auto frame_done = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> time_elapsed =
        frame_done - frame_start;
    auto milliseconds_elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(time_elapsed)
            .count();

    if (milliseconds_elapsed < FRAME_TIME_MILLIS) {
      auto delay_time = FRAME_TIME_MILLIS - milliseconds_elapsed;
      SDL_Delay(static_cast<Uint32>(delay_time));
    }

    SDL_RenderPresent(app.render_state.renderer);

    auto all_frame_done = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> all_time_elapsed =
        all_frame_done - frame_start;
    auto all_milliseconds_elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(all_time_elapsed)
            .count();

    frame_times.push_back(all_milliseconds_elapsed);
    if (frame_times.size() > max_frame_history) {
      frame_times.pop_front();
    }

    app.update_state.average_fps =
        1000.0f /
        (std::accumulate(frame_times.begin(), frame_times.end(), 0.0) /
         frame_times.size());
  }

  return Result::SUCCESS;
}

void destroy_app(App &app) {
  destroy_rendering(app.render_state);
}
}  // namespace VV
