#pragma once

namespace YC {
struct Config {
  int window_width, window_height; // using int since that's what sdl takes
  bool window_start_maximized;
};

Config default_config();
} // namespace YC
