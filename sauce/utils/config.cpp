#include "utils/config.h"

#ifdef _WIN32
#include <windows.h>
#undef min  // SDL has a macro for this on windows for some reason
#elif defined(__linux__)
#include <linux/limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#error "Unsupported platform"
#endif

namespace VV {
Config default_config() {
  return {
      600,    // window_width
      400,    // window_height
      true,   // window_start_maximized
      false,  // show_chunk_corners
      4,      // num_threads
      "",     // res_dir: Should be set by caller
      "",     // tex_dir: set with res_dir
  };
}

// NOTE (Levi): I don't know the other platforms, so ChatGPT did them. Platform
// specific includes for this are at the top of the file.
Result get_resource_dir(std::filesystem::path &res_dir) {
#ifdef _WIN32
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  res_dir = std::filesystem::path(path).parent_path() / "res";
  return Result::SUCCESS;
#elif defined(__linux__)
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX - 1);
  if (count > 0) {
    path[count] = '\0';
    res_dir = std::filesystem::path(path).parent_path() / "res";
    return Result::SUCCESS;
  } else {
    LOG_ERROR("Failed to find resource dir on linux!");
    return Result::FILESYSTEM_ERROR;
  }
#elif defined(__APPLE__)
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    // _NSGetExecutablePath doesn't guarantee null-termination if the buffer is
    // exactly the size needed
    path[size - 1] = '\0';  // Correctly ensure null-termination
    res_dir = std::filesystem::path(path).parent_path() / "res";
    return Result::SUCCESS;
  } else {
    LOG_ERROR("Failed to find resource dir on Apple!");
    return Result::FILESYSTEM_ERROR;
  }
#endif
}
}  // namespace VV
