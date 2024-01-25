#include "SDL_timer.h"
#include "app.h"

using namespace YC;

int main(int argc, char *argv[]) {
  g_config = default_config();
  LOG_INFO("Log initialized");

  Dimension overworld;

  load_chunks_square(overworld, 0.0, 0.0, 3);

  if (init_rendering() != Result::SUCCESS) {
    LOG_FATAL("Failed to initialize rendering. Quiting...");
    destroy_rendering();
  }

  SDL_Delay(2000);

  destroy_rendering();

  return 0;
}
