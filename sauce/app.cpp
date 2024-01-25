#include "app.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace YC {
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

Config g_config;

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

bool Chunk_Coord::operator<(const Chunk_Coord &b) const {
  u64 a_cantor = mod_cantor(x, y);
  u64 b_cantor = mod_cantor(b.x, b.y);

  return a_cantor < b_cantor;
}

/////////////////////////////
/// World implementations ///
/////////////////////////////

Chunk_Coord get_chunk_coord(f64 x, f64 y) {
  Chunk_Coord return_chunk_coord;

  return_chunk_coord.x = x / 16;
  return_chunk_coord.y = y / 16;

  if (x < 0) {
    return_chunk_coord.x -= 1;
  }

  if (y < 0) {
    return_chunk_coord.y -= 1;
  }

  return return_chunk_coord;
}

Result gen_chunk(Chunk &chunk) {
  std::memset(&chunk.cells, (int)Cell_Type::DIRT, CHUNK_CELLS);
  // Leaving the color unset for now so that it looks trippy
  // when we get to rendering

  return Result::SUCCESS;
}

Result load_chunk(Dimension &dim, const Chunk_Coord &coord) {
  // Eventually we'll also load from disk
  gen_chunk(dim.chunks[coord]);

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
Result init_rendering() { return Result::SUCCESS; }

} // namespace YC
