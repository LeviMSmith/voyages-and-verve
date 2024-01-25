#include "app.h"

using namespace YC;

int main(int argc, char *argv[]) {
  g_config = default_config();
  LOG_INFO("Log initialized");

  return 0;
}
