#pragma once

#include <cstdint>

#include "spdlog/spdlog.h"

namespace VV {

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef bool b8;

enum class Result : s32 {
  SUCCESS,
  SDL_ERROR,
  WINDOW_CLOSED,
  NONEXIST,
  FILESYSTEM_ERROR,
  GENERAL_ERROR,
  ENTITY_POOL_FULL,
  BAD_ARGS_ERROR,
  OS_ERROR,
  VALUE_ERROR,
};

#ifndef NDEBUG
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define LOG_DEBUG(...) (void)0
#endif
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_FATAL(...) spdlog::critical(__VA_ARGS__)

}  // namespace VV
