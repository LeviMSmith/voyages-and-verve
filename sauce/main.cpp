#include "app.h"

using namespace YC;

int main(int argc, char *argv[]) {
  g_config = default_config();
  LOG_INFO("Log initialized");

  Dimension overworld;

  load_chunks_square(overworld, 0.0, 0.0, 3);

  return 0;
}
