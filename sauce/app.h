#pragma once

#include "SDL_events.h"
#include "SDL_render.h"
#include "SDL_surface.h"
#include "SDL_video.h"
#include <cstdint>
#include <map>
#include <vector>

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
};

/// Log definitions ///

enum Log_Level {
  DEBUG = 0,
  INFO,
  WARN,
  ERROR,
  FATAL,
};

void app_log(Log_Level level, const char *format, ...);

#define LOG_DEBUG(...) app_log(Log_Level::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) app_log(Log_Level::INFO, __VA_ARGS__)
#define LOG_WARN(...) app_log(Log_Level::WARN, __VA_ARGS__)
#define LOG_ERROR(...) app_log(Log_Level::ERROR, __VA_ARGS__)
#define LOG_FATAL(...) app_log(Log_Level::FATAL, __VA_ARGS__)

/// Config definitions ///

struct Config {
  int window_width, window_height; // using int since that's what sdl takes
  bool window_start_maximized;

  Log_Level log_level_threshold;
};

Config default_config();

////////////////////////
/// Math definitions ///
////////////////////////

// A modified cantor pairing for full 2d
u64 mod_cantor(s32 a, s32 b);

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
struct Entity {
  f64 x, y;
  f32 vx, vy;
  f32 ax, ay;
  f32 camx, camy; // This is relative to the main pos
};

Entity default_entity();

/////////////////////////
/// World definitions ///
/////////////////////////

enum class Cell_Type : u8 { DIRT, AIR, WATER };

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
  u8 cr, cg, cb, ca; // Color rgba8
};

// All cell interactions are done in chunks. This is how they're simulated,
// loaded, and generated.
constexpr u16 CHUNK_CELL_WIDTH = 16;
constexpr u16 CHUNK_CELLS = CHUNK_CELL_WIDTH * CHUNK_CELL_WIDTH; // 256

// All a chunk ever should be is a list of cells, but it's easier to struct
// then type that all out
struct Chunk {
  Chunk_Coord coord;
  Cell cells[CHUNK_CELLS];
};

Chunk_Coord get_chunk_coord(f64 x, f64 y);
Result gen_chunk(Chunk &chunk, const Chunk_Coord &chunk_coord);

// Entities will also go here in their own map
// Using map right now because implementation is quick,
// but it'd be better to have a specialized binary tree
struct Dimension {
  std::map<Chunk_Coord, Chunk> chunks;
};

Result load_chunk(Dimension &dim, const Chunk_Coord &coord);
Result load_chunks_square(Dimension &dim, f64 x, f64 y, u8 radius);

//////////////////////////
/// Update definitions ///
//////////////////////////

struct Update_State {
  Dimension overworld;
  Entity active_player;
  std::vector<SDL_Event> pending_events;
};

Result init_updating(Update_State &update_state);
Result update(Update_State &update_state);

/////////////////////////////
/// Rendering definitions ///
/////////////////////////////

constexpr u8 SCREEN_CHUNK_SIZE =
    12; // 16 * 12 = 196; 196 * 196 = 36864 pixels in texture

// This is the part of the texture that will not be shown
constexpr u8 SCREEN_CELL_PADDING = 27; // Makes screen width 169 (13*13) cells

struct Render_State {
  int window_width, window_height;

  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Surface *surface;

  u32 cell_texture_buffer[SCREEN_CHUNK_SIZE * SCREEN_CHUNK_SIZE * CHUNK_CELLS];
  SDL_Texture *cell_texture;

  std::vector<SDL_Event> pending_events;
};

// Uses global config
Result init_rendering(Render_State &render_state);
Result render(Render_State &render_state, Update_State &update_state);
void destroy_rendering(Render_State &render_state);

Result handle_window_resize(Render_State &render_state);

Result gen_world_texture(Render_State &render_state,
                         Update_State &update_state);

/////////////////////////
/// State definitions ///
/////////////////////////

struct App {
  Update_State update_state;
  Render_State render_state;
  Config config;
};

Result poll_events(App &app);
Result init_app(App &app);
Result run_app(App &app);
void destroy_app(App &app);
} // namespace VV
