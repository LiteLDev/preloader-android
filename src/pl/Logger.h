#pragma once

#include <string>
namespace pl {
namespace log {

class Logger {
public:
  explicit Logger(const std::string &name);

  void info(const char *fmt, ...);
  void debug(const char *fmt, ...);
  void warn(const char *fmt, ...);
  void error(const char *fmt, ...);

private:
  std::string loggerName;
  void log(int level, const char *fmt, va_list args);
};
} // namespace log
} // namespace pl