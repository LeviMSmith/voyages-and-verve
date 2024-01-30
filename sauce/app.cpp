#include "app.h"

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_hints.h"
#include "SDL_render.h"
#include "SDL_timer.h"
#include "SDL_video.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace YC {
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
Result init_rendering(App &app) {
  // For some reason SDL_QUIT is triggered randomly on my system, so we're not
  // quiting on that. If SDL holds on to the handlers, we can't exit with a
  // break otherwise.
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

  SDL_ClearError();
  app.render_state.renderer =
      SDL_CreateRenderer(app.render_state.window, -1, 0);
  if (app.render_state.renderer == nullptr) {
    LOG_ERROR("Failed to create sdl renderer: %s", SDL_GetError());
    return Result::SDL_ERROR;
  }

  Result resize_res = handle_window_resize(app.render_state);
  if (resize_res != Result::SUCCESS) {
    LOG_WARN("Failed to handle window resize! EC: %d", resize_res);
  }

  return Result::SUCCESS;
}

Result render(Render_State &render_state) {
  SDL_RenderClear(render_state.renderer);

  SDL_RenderPresent(render_state.renderer);

  return Result::SUCCESS;
}

void destroy_rendering(Render_State &render_state) {
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

//////////////////////////////
/// Update implementations ///
//////////////////////////////

Result init_updating(Update_State &update_state) {
  load_chunks_square(update_state.overworld, 0.0, 0.0, 4);

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
      LOG_DEBUG("Got event SDL_QUIT. Don't know why. Continuing.");
    }
    }
    LOG_DEBUG("Finished polling an event");
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

  render(app.render_state);

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
    render(app.render_state);
    SDL_Delay(1);
  }

  return Result::SUCCESS;
}

void destroy_app(App &app) { destroy_rendering(app.render_state); }
} // namespace YC
