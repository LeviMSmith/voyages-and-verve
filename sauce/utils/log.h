#pragma once

namespace YC {
enum Log_Level {
  DEBUG = 0,
  INFO,
  WARN,
  ERROR,
  FATAL,
};

void app_log(Log_Level level, const char *format, ...);

#define LOG_DEBUG(...) app_log(Log_Level::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) app_log(Log_Level::INFO, __VA_ARGS__)
#define LOG_WARN(...) app_log(Log_Level::WARN, __VA_ARGS__)
#define LOG_ERROR(...) app_log(Log_Level::ERROR, __VA_ARGS__)
#define LOG_FATAL(...) app_log(Log_Level::FATAL, __VA_ARGS__)

} // namespace YC
