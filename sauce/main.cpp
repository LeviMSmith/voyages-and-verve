#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>

/////////////////
/// Utilities ///
/////////////////

/// Basic Types ///

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

/// Result ///

enum class Result {
  SUCCESS,
};

/// Log ///

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

/// Config ///

struct Config {
  int window_width, window_height; // using int since that's what sdl takes
  bool window_start_maximized;

  Log_Level log_level_threshold;
};

Config default_config();

Config g_config;

////////////
/// Math ///
////////////

// A modified cantor pairing for full 2d
u64 mod_cantor(s32 a, s32 b);

/// Chunk_Coord ///
struct Chunk_Coord {
  s32 x, y;

  // Cantor 0.5 * ()
  bool operator<(const Chunk_Coord &b);
};

/////////////
/// World ///
/////////////

enum class Cell_Type : u8 { DIRT, AIR, WATER };

// Monolithic cell struct. Everything the cell does should be put in here.
// There should be support for millions of cells, so avoid putting too much here
// If it isn't something that every cell needs, it should be in the cell's type
// info or static.
struct Cell {
  Cell_Type type;
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

///////////////////////////////////////////////
/// The almighty main runtime               ///
/// Everything after this is implementations ///
///////////////////////////////////////////////

int main(int argc, char *argv[]) {
  g_config = default_config();
  LOG_INFO("Log initialized");

  return 0;
}

/////////////////////////////////
/// Utilities implementations ///
/////////////////////////////////

/// Log implemenations ///

void app_log(Log_Level level, const char *format, ...) {
  const char *log_level_names[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
  const char *log_level_colors[] = {
      "\033[94m", // Bright Blue
      "\033[32m", // Dark Green
      "\033[93m", // Bright Yellow
      "\033[31m", // Dark Red
      "\033[35m"  // Dark Magenta
  };
  const long CLOCKS_PER_MILLISEC = CLOCKS_PER_SEC / 1000;

  if (level >= g_config.log_level_threshold) {
    va_list args;
    va_start(args, format);

    printf("[%ld] [%s%s\033[0m] ", clock() / CLOCKS_PER_MILLISEC,
           log_level_colors[level], log_level_names[level]);
    vprintf(format, args);
    printf("\n");

    va_end(args);
  }
}

/// Config implementations ///

Config default_config() {
  return {
      600,  // window_width
      400,  // window_height
      true, // window_start_maximized
  };
}

////////////////////////////
/// Math implementations ///
////////////////////////////

u64 mod_cantor(s32 a, s32 b) {
  if (a < 0) {
    a = a * 2;
  } else {
    a = a * 2 - 1;
  }

  if (b < 0) {
    b = b * 2;
  } else {
    b = b * 2 - 1;
  }

  return 0.5 * (a + b) * (a + b + 1) + b;
}

bool Chunk_Coord::operator<(const Chunk_Coord &b) {
  u64 a_cantor = mod_cantor(x, y);
  u64 b_cantor = mod_cantor(b.x, b.y);

  return a_cantor < b_cantor;
}

/////////////////////////////
/// World implementations ///
/////////////////////////////

Result gen_chunk(Chunk &chunk) {
  std::memset(&chunk.cells, (int)Cell_Type::DIRT, CHUNK_CELLS);

  return Result::SUCCESS;
}
