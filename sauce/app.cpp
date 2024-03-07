#include "app.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <numeric>
#include <set>
#include <stdexcept>

#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_render.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "core.h"
#include "update/update.h"
#include "update/world.h"

namespace VV {
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

  Dimension &active_dimension = *get_active_dimension(app.update_state);

  u16 screen_cell_size = render_state.screen_cell_size;
  Entity &active_player = *get_active_player(app.update_state);

  Entity_Coord tl;
  tl.x = active_player.camx + active_player.coord.x;
  tl.x -= (render_state.window_width / 2.0f) / screen_cell_size;

  tl.y = active_player.camy + active_player.coord.y;
  tl.y += (render_state.window_height / 2.0f) / screen_cell_size;

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
      case SDL_MOUSEBUTTONUP: {
        if (event.button.button == SDL_BUTTON_LEFT) {
          // Place a gold cell in the world where the button was pressed.

          Entity_Coord c;
          c.x =
              static_cast<s32>(event.button.x / render_state.screen_cell_size) +
              tl.x;
          c.y = tl.y - static_cast<s32>(event.button.y /
                                        render_state.screen_cell_size);

          Chunk_Coord cc = get_chunk_coord(c.x, c.y);

          Chunk &chunk = active_dimension.chunks[cc];
          u16 cx = std::abs((cc.x * CHUNK_CELL_WIDTH) - c.x);
          u16 cy = c.y - (cc.y * CHUNK_CELL_WIDTH);
          u32 cell_index = cx + cy * CHUNK_CELL_WIDTH;

          assert(cell_index < CHUNK_CELLS);

          chunk.cells[cell_index] = default_gold_cell();

          app.update_state.events.emplace(Update_Event::CELL_CHANGE);
        }
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
    app.update_state.events.clear();
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
