#pragma once

#include <android/log.h>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pl::log {

class Logger {
public:
  explicit Logger(std::string name) : loggerName(std::move(name)) {}

  static Logger &getOrCreate(std::string name) {
    std::lock_guard<std::mutex> lock(loggerMutex);

    auto it = loggers.find(name);
    if (it != loggers.end()) {
      return *it->second;
    }

    auto logger = std::make_unique<Logger>(std::move(name));
    auto &ref = *logger;
    loggers.emplace(ref.loggerName, std::move(logger));
    return ref;
  }

  template <typename... Args>
  void info(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_INFO, fmtStr, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void debug(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_DEBUG, fmtStr, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void warn(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_WARN, fmtStr, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void error(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_ERROR, fmtStr, std::forward<Args>(args)...);
  }

private:
  std::string loggerName;

  inline static std::unordered_map<std::string, std::unique_ptr<Logger>>
      loggers{};
  inline static std::mutex loggerMutex{};

  template <typename... Args>
  void log(int androidLevel, std::string_view fmtStr, Args &&...args) const {
    const auto message = std::vformat(fmtStr, std::make_format_args(args...));

    __android_log_print(androidLevel, loggerName.c_str(), "%s",
                        message.c_str());
  }
};

} // namespace pl::log

inline auto &preloader_logger = pl::log::Logger::getOrCreate("Preloader");
