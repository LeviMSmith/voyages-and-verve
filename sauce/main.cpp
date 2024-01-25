#include "SDL_timer.h"
#include "app.h"

using namespace YC;

int main(int argc, char *argv[]) {
  App *app = new App;
  LOG_INFO("Log initialized");

  Dimension overworld;

  load_chunks_square(overworld, 0.0, 0.0, 3);

  init_app(*app);

  SDL_Delay(2000);

  destroy_app(*app);

  delete app;

  return 0;
}
