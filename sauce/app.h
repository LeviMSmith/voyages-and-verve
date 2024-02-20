#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <unordered_set>
#include <vector>

#include "SDL_events.h"
#include "SDL_render.h"
#include "SDL_surface.h"
#include "SDL_ttf.h"
#include "SDL_video.h"
#include "spdlog/spdlog.h"

namespace VV {
/////////////////////////////
/// Utilities definitions ///
/////////////////////////////

/// Basic Type definitions ///

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef bool b8;

/// Result definitions ///

enum class Result {
  SUCCESS,
  SDL_ERROR,
  WINDOW_CLOSED,
  NONEXIST,
  FILESYSTEM_ERROR,
  GENERAL_ERROR,
  ENTITY_POOL_FULL,
};

/// Log definitions ///

#ifndef NDEBUG
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...) (void)0
#endif
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_FATAL(...) spdlog::critical(__VA_ARGS__)

/// Config definitions ///

struct Config {
  int window_width, window_height;  // using int since that's what sdl takes
  bool window_start_maximized;

  bool debug_overlay;

  std::filesystem::path res_dir;
  std::filesystem::path tex_dir;
};

Config default_config();

/// Resource definitions ///

// Resource definitions on the disk

struct Res_Texture {
  s32 width, height;
  SDL_Texture *texture;
};

Result get_resource_dir(std::filesystem::path &res_dir);

////////////////////////
/// Math definitions ///
////////////////////////

/// Chunk_Coord ///
struct Chunk_Coord {
  s32 x, y;

  bool operator<(const Chunk_Coord &b) const;
  bool operator==(const Chunk_Coord &b) const;
};

//////////////////////////
/// Entity definitions ///
//////////////////////////

// Monolithic Entity struct. Every entity possess every possible
// attribute to simplify the data. I'm thinking we'll probably
// never have more than at max hundreds of thousands of these
// so even at hundreds of bytes this struct should be totally fine
// to fit into memory. (Like maybe 100MB max)
// AOS, so a little OOP, but I think this is very flexible way
// to hold all the data. Plus no mapping like a SOA would require.

constexpr u32 MAX_TOTAL_ENTITIES = 100000;

struct Entity_Coord {
  f64 x, y;
};

enum class Texture_Id : u8 { NONE = 0, PLAYER = 1, SKY = 2, TREE = 3 };

typedef u32 Entity_ID;
typedef s8 Entity_Z;

// If you add something to this, make sure it works with default_entity!
struct Entity {
  Entity_Coord coord;  // For bounding box and rendering, this is top left
  f32 vx, vy;
  f32 ax, ay;
  bool on_ground;
  f32 camx, camy;  // This is relative to coord

  // The physics bounding box starting from coord as top left
  f32 boundingw, boundingh;

  // These are what will be manipulated by the animation system
  Texture_Id texture;  // a number that is mapped to a file in resources.json
  u8 texture_index;    // an index into a texture atlas
  Entity_Z zdepth;     // Only applies to entity rendering
  bool flipped;
};

Entity default_entity();
inline Entity_Coord get_cam_coord(const Entity &e);

/////////////////////////
/// World definitions ///
/////////////////////////

/// Cell ///
enum class Cell_Type : u8 { DIRT, AIR, WATER };

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
  u8 cr, cg, cb, ca;  // Color rgba8
};

// Factory functions
Cell default_dirt_cell();
Cell default_air_cell();
Cell default_water_cell();

Cell default_grass_cell();

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
constexpr u16 SURFACE_CELL_RANGE =
    SURFACE_Y_MAX * CHUNK_CELL_WIDTH - SURFACE_Y_MIN * CHUNK_CELL_WIDTH;
constexpr s32 SEA_LEVEL = 0;
constexpr u32 GEN_TREE_MIN_WIDTH = 1500;

u16 surface_det_rand(u64 seed);
u16 interpolate_and_nudge(u16 y1, u16 y2, f64 fraction, u64 seed,
                          f64 randomness_scale);
u16 surface_height(s64 x, u16 max_depth);

// For finding out where a chunk bottom right corner is
Entity_Coord get_world_pos_from_chunk(Chunk_Coord coord);
Chunk_Coord get_chunk_coord(f64 x, f64 y);

/// Dimensions ///
enum class DimensionIndex : u8 {
  OVERWORLD,
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

//////////////////////////
/// Update definitions ///
//////////////////////////

enum Update_Event : u8 {
  PLAYER_MOVED_CHUNK,
};

struct Update_State {
  std::vector<SDL_Event> pending_events;

  std::map<DimensionIndex, Dimension> dimensions;

  std::unordered_set<Entity_ID> entity_id_pool;
  Entity entities[MAX_TOTAL_ENTITIES];

  DimensionIndex active_dimension;  // Key of active dimension
  Entity_ID active_player;          // Index into entities

  std::set<Update_Event> events;

  // Debug
  f32 average_fps;
};

Result init_updating(Update_State &update_state);
Result update(Update_State &update_state);

Result update_keypresses(Update_State &us);

constexpr f32 KINETIC_FRICTION = 0.8f;
constexpr f32 KINETIC_GRAVITY = 0.43f;
constexpr f32 KINETIC_TERMINAL_VELOCITY = -300.0f;
void update_kinetic(Update_State &update_state);

Result gen_chunk(Update_State &update_state, Chunk &chunk,
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

// Don't hold on to these pointers too long. Additions to the vectors could
// invalidate them
inline Dimension *get_active_dimension(Update_State &update_state);
inline Entity *get_active_player(Update_State &update_state);

/////////////////////////////
/// Rendering definitions ///
/////////////////////////////

constexpr u8 SCREEN_CHUNK_SIZE =
    8;  // 64 * 6 = 384; 384 * 384 = 147456 pixels in texture

// This is the part of the texture that will not be shown
constexpr u8 SCREEN_CELL_PADDING = 160;  // Makes screen width 224 cells
constexpr u16 SCREEN_CELL_SIZE_FULL = SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH;

struct Render_State {
  int window_width, window_height;

  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Surface *surface;

  std::string debug_info;
  TTF_Font *main_font;

  SDL_Texture *cell_texture;
  std::map<u8, Res_Texture>
      textures;  // This mapping should be the same as in resources.json
  SDL_Texture *debug_overlay_texture;

  std::vector<SDL_Event> pending_events;

  Chunk_Coord tl_tex_chunk;
  u16 screen_cell_size;
};

// Uses global config
Result init_rendering(Render_State &render_state, Config &config);
Result render(Render_State &render_state, Update_State &update_state,
              const Config &config);
void destroy_rendering(Render_State &render_state);

// No need to stream textures in, so we'll just create them all up front and
// then use them as needed.
Result init_render_textures(Render_State &render_state, const Config &config);

Result handle_window_resize(Render_State &render_state);

Result gen_world_texture(Render_State &render_state, Update_State &update_state,
                         const Config &config);
Result refresh_debug_overlay(Render_State &render_state,
                             const Update_State &update_state, int &w, int &h);

Result render_trees(Render_State &rs, Update_State &us);
Result render_cell_texture(Render_State &render_state,
                           Update_State &update_state);
Result render_entities(Render_State &render_state, Update_State &update_state,
                       Entity_Z z_min = 1, Entity_Z z_thresh = INT8_MAX);

/////////////////////////
/// State definitions ///
/////////////////////////

constexpr u32 FPS = 60;
constexpr f32 FRAME_TIME_MILLIS = (1.0f / FPS) * 1000;

struct App {
  Update_State update_state;
  Render_State render_state;
  Config config;
};

Result poll_events(App &app);
Result init_app(App &app);
Result run_app(App &app);
void destroy_app(App &app);
}  // namespace VV
