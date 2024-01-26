#include "SDL_timer.h"
#include "app.h"

using namespace YC;

int main(int argc, char *argv[]) {
  App *app = new App;
  LOG_INFO("Log initialized");

  init_app(*app);
  run_app(*app);
  destroy_app(*app);

  delete app;

  return 0;
}
