#pragma once

#include <filesystem>

namespace VV {
struct Config {
  int window_width, window_height;  // using int since that's what sdl takes
  bool window_start_maximized;

  bool debug_overlay;

  std::filesystem::path res_dir;
  std::filesystem::path tex_dir;
};

Config default_config();
}  // namespace VV
