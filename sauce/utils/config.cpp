#include "utils/config.h"

namespace YC {
Config default_config() {
  return {
      600,  // window_width
      400,  // window_height
      true, // window_start_maximized
  };
}
} // namespace YC
