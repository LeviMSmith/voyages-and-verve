#include <cstdlib>

#include "app.h"

using namespace VV;

int main(int argv, const char** argc) {
  App* app = new App;
  LOG_INFO("Log initialized");

  LOG_INFO("The App state struct is {} bytes", sizeof(App));

  if (init_app(*app, argv, argc) != Result::SUCCESS) {
    destroy_app(*app);
    return EXIT_FAILURE;
  }
  run_app(*app);
  destroy_app(*app);

  delete app;

  return EXIT_SUCCESS;
}
