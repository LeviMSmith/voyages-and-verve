#include "app.h"
#include <cstdlib>

using namespace VV;

int main(int argc, char *argv[]) {
  App *app = new App;
  LOG_INFO("Log initialized");

  LOG_DEBUG("The App state struct is %d bytes", sizeof(App));

  if (init_app(*app) != Result::SUCCESS) {
    return EXIT_FAILURE;
  }
  run_app(*app);
  destroy_app(*app);

  delete app;

  return EXIT_SUCCESS;
}
