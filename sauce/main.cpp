#include "utils/config.h"
#include "utils/log.h"

using namespace YC;

int main(int argc, char *argv[]) {
  LOG_INFO("Log initialized");
  YC::Config config = YC::default_config();

  return 0;
}
