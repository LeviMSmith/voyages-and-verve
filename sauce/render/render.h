#pragma once

#include "SDL_ttf.h"
#include "core.h"
#include "update/update.h"
#include "update/world.h"
#include "utils/config.h"

namespace VV {

constexpr u8 SCREEN_CHUNK_SIZE =
    8;  // 64 * 8 = 512; 512 * 512 = 262144 pixels in texture

// This is the part of the texture that will not be shown
constexpr u8 SCREEN_CELL_PADDING = 160;  // Makes screen width 352 cells
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

}  // namespace VV
