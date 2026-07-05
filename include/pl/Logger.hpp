#pragma once

/**
 * @file Logger.hpp
 * @brief Lightweight Android logger used by native mods.
 */

#include <android/log.h>

#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace pl::log {

/**
 * @brief Named Android logger with std::format-style messages.
 */
class Logger {
public:
  explicit Logger(std::string name) : mLoggerName(std::move(name)) {}

  /**
   * @brief Returns a process-wide logger for the given tag.
   */
  static Logger &getOrCreate(std::string name) {
    std::lock_guard<std::mutex> lock(sLoggerMutex);

    auto it = sLoggers.find(name);
    if (it != sLoggers.end()) {
      return *it->second;
    }

    auto logger = std::make_unique<Logger>(std::move(name));
    auto &ref = *logger;
    sLoggers.emplace(ref.mLoggerName, std::move(logger));
    return ref;
  }

  /**
   * @brief Writes an informational log line.
   */
  template <typename... Args>
  void info(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_INFO, fmtStr, std::forward<Args>(args)...);
  }

  /**
   * @brief Writes a debug log line.
   */
  template <typename... Args>
  void debug(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_DEBUG, fmtStr, std::forward<Args>(args)...);
  }

  /**
   * @brief Writes a warning log line.
   */
  template <typename... Args>
  void warn(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_WARN, fmtStr, std::forward<Args>(args)...);
  }

  /**
   * @brief Writes an error log line.
   */
  template <typename... Args>
  void error(std::string_view fmtStr, Args &&...args) const {
    log(ANDROID_LOG_ERROR, fmtStr, std::forward<Args>(args)...);
  }

private:
  std::string mLoggerName;

  inline static std::unordered_map<std::string, std::unique_ptr<Logger>>
      sLoggers{};
  inline static std::mutex sLoggerMutex{};

  template <typename... Args>
  void log(int androidLevel, std::string_view fmtStr, Args &&...args) const {
    const auto message = std::vformat(fmtStr, std::make_format_args(args...));
    __android_log_print(androidLevel, mLoggerName.c_str(), "%s",
                        message.c_str());
  }
};

} // namespace pl::log

/**
 * @brief Shared logger used by the preloader runtime itself.
 */
inline auto &preloaderLogger = pl::log::Logger::getOrCreate("Preloader");
