#pragma once

#include <cstdint>
#include <map>

namespace YC {
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

extern Config g_config;

////////////////////////
/// Math definitions ///
////////////////////////

// A modified cantor pairing for full 2d
u64 mod_cantor(s32 a, s32 b);

/// Chunk_Coord ///
struct Chunk_Coord {
  s32 x, y;

  bool operator<(const Chunk_Coord &b) const;
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
  Cell cells[CHUNK_CELLS];
};

Result gen_chunk(Chunk &chunk);

// Entities will also go here in their own map
// Using map right now because implementation is quick,
// but it'd be better to have a specialized binary tree
struct Dimension {
  std::map<Chunk_Coord, Chunk> chunks;
};

Result load_chunk(Dimension &dim, const Chunk_Coord &coord);
Result load_chunks_square(Dimension &dim, f64 x, f64 y, u8 radius);

/////////////////////////////
/// Rendering definitions ///
/////////////////////////////

// Uses global config
Result init_rendering();

} // namespace YC
