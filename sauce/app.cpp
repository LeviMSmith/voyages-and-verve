#include "app.h"

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_render.h"
#include "SDL_timer.h"
#include "SDL_video.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace VV {
/////////////////////////////////
/// Utilities implementations ///
/////////////////////////////////

/// Log implemenations ///

Log_Level g_log_level_threshold;

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

  if (level >= g_log_level_threshold) {
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

//////////////////////////////
/// Entity implementations ///
//////////////////////////////

Entity default_entity() {
  Entity return_entity;
  return_entity.x = 0;
  return_entity.y = 0;
  return_entity.vx = 0;
  return_entity.vy = 0;
  return_entity.ax = 0;
  return_entity.ay = 0;
  return_entity.camx = 0;
  return_entity.camy = 0;

  return return_entity;
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

Result gen_chunk(Chunk &chunk, const Chunk_Coord &chunk_coord) {
  if (chunk_coord.y <= 0) {
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

  SDL_ClearError();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    LOG_ERROR("Failed to initialize sdl: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("SDL initialized");

  LOG_DEBUG("Config window values: %d, %d", app.config.window_width,
            app.config.window_height);

  // Window
  SDL_ClearError();
  int window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (app.config.window_start_maximized) {
    window_flags = window_flags | SDL_WINDOW_MAXIMIZED;
    LOG_DEBUG("Starting window maximized");
  }

  app.render_state.window = SDL_CreateWindow(
      "Yellow Copper", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      app.config.window_width, app.config.window_height, window_flags);

  SDL_ClearError();
  if (app.render_state.window == nullptr) {
    LOG_ERROR("Failed to create sdl window: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("Window created");

  // Renderer
  SDL_ClearError();
  app.render_state.renderer =
      SDL_CreateRenderer(app.render_state.window, -1, 0);
  if (app.render_state.renderer == nullptr) {
    LOG_ERROR("Failed to create sdl renderer: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  // Do an initial resize to get all the base info from the screen loading
  // into the state
  Result resize_res = handle_window_resize(app.render_state);
  if (resize_res != Result::SUCCESS) {
    LOG_WARN("Failed to handle window resize! EC: %d", resize_res);
  }

  // Create the world cell texture
  SDL_ClearError();
  app.render_state.cell_texture = SDL_CreateTexture(
      app.render_state.renderer, SDL_PIXELFORMAT_RGBA8888,
      SDL_TEXTUREACCESS_STREAMING, SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH,
      SCREEN_CHUNK_SIZE * CHUNK_CELL_WIDTH);
  if (app.render_state.cell_texture == nullptr) {
    LOG_ERROR("Failed to create cell texture with SDL: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  LOG_INFO("Created cell texture");

  return Result::SUCCESS;
}

Result render(Render_State &render_state, Update_State &update_state) {
  SDL_RenderClear(render_state.renderer);

  gen_world_texture(render_state, update_state);

  // The world texture
  SDL_Rect destRect = {
      0, 0, render_state.window_width,
      render_state.window_height}; // Example destination rectangle; adjust
                                   // as necessary
  SDL_RenderCopy(render_state.renderer, render_state.cell_texture, NULL,
                 &destRect);

  SDL_RenderPresent(render_state.renderer);

  return Result::SUCCESS;
}

void destroy_rendering(Render_State &render_state) {
  if (render_state.cell_texture != nullptr) {
    SDL_DestroyTexture(render_state.cell_texture);
    LOG_INFO("Destroyed cell texture");
  }
  if (render_state.window != nullptr) {
    SDL_DestroyWindow(render_state.window);
    LOG_INFO("Destroyed SDL window");
  }

  SDL_Quit();
  LOG_INFO("Quit SDL");
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
  LOG_INFO("SDL window resized to %d, %d", render_state.window_width,
           render_state.window_height);

  return Result::SUCCESS;
}

Result gen_world_texture(Render_State &render_state,
                         Update_State &update_state) {
  // TODO: Might be good to have this ask the world for chunks it needs.

  // How to generate:
  // First need to find which chunks are centered around active_player cam.
  // Then essentially write the colors of each cell to the texture. Should
  // probably multithread.
  // TODO: multithread

  Entity &active_player = update_state.active_player;

  // Remember, Entity::cam is relative to the entity's position
  f64 camx, camy;
  camx = active_player.camx + active_player.x;
  camy = active_player.camy + active_player.y;

  Chunk_Coord center = get_chunk_coord(camx, camy);
  Chunk_Coord ic;
  Chunk_Coord ic_max;
  u8 radius = SCREEN_CHUNK_SIZE / 2;

  // NOTE (Levi): There's no overflow checking on this right now.
  // Just don't go to the edge of the world I guess.
  ic.x = center.x - radius;
  ic.y = center.y - radius;

  ic_max.x = ic.x + SCREEN_CHUNK_SIZE;
  ic_max.y = ic.y + SCREEN_CHUNK_SIZE;

  u8 chunk_x = 0;
  u8 chunk_y = 0;

  // This should be the dimension that the player in. But that will come later
  Dimension &active_dimension = update_state.overworld;

  // LOG_DEBUG("Generating world texture");
  // For each chunk in the texture...

  u32 *pixels;
  int pitch;
  SDL_ClearError();
  if (SDL_LockTexture(render_state.cell_texture, NULL, (void **)&pixels,
                      &pitch) != 0) {
    LOG_WARN("Failed to lock cell texture for updating: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  assert(pitch == CHUNK_CELL_WIDTH * SCREEN_CHUNK_SIZE * sizeof(u32));

  // For each chunk in the texture...
  for (ic.y = center.y - radius; ic.y < ic_max.y; ic.y++) {
    for (ic.x = center.x - radius; ic.x < ic_max.x; ic.x++) {
      Chunk &chunk = active_dimension.chunks[ic];

      for (u16 cell_y = 0; cell_y < CHUNK_CELL_WIDTH; cell_y++) {
        for (u16 cell_x = 0; cell_x < CHUNK_CELL_WIDTH; cell_x++) {
          size_t buffer_index =
              (cell_x + chunk_x * CHUNK_CELL_WIDTH) +
              (cell_y * CHUNK_CELL_WIDTH * SCREEN_CHUNK_SIZE) +
              (chunk_y * CHUNK_CELL_WIDTH * CHUNK_CELL_WIDTH *
               SCREEN_CHUNK_SIZE);
          size_t chunk_index = chunk_x + chunk_y * CHUNK_CELL_WIDTH;
          u8 cr = chunk.cells[chunk_index].cr;
          u8 cg = chunk.cells[chunk_index].cg;
          u8 cb = chunk.cells[chunk_index].cb;
          u8 ca = chunk.cells[chunk_index].ca;
          pixels[buffer_index] = (cr << 24) | (cg << 16) | (cb << 8) | ca;
          if (buffer_index >
              SCREEN_CHUNK_SIZE * SCREEN_CHUNK_SIZE * CHUNK_CELLS - 1) {
            LOG_ERROR(
                "Somehow surpassed the texture size while generating: %d "
                "cell texture chunk_x: %d, chunk_y: %d, cell_x: %d, cell_y: "
                "%d",
                buffer_index, chunk_x, chunk_y, cell_x, cell_y);
          }
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

//////////////////////////////
/// Update implementations ///
//////////////////////////////

Result init_updating(Update_State &update_state) {
  update_state.active_player = default_entity();
  load_chunks_square(update_state.overworld, update_state.active_player.x,
                     update_state.active_player.y, SCREEN_CHUNK_SIZE / 2);

  return Result::SUCCESS;
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
#ifndef NDEBUG
  g_log_level_threshold = Log_Level::DEBUG;
#else
  g_log_level_threshold = Log_Level::INFO;
#endif
  app.config = default_config();
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

    // Render
    render(app.render_state, app.update_state);
    SDL_Delay(1);
  }

  return Result::SUCCESS;
}

void destroy_app(App &app) { destroy_rendering(app.render_state); }
} // namespace VV
