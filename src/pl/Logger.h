#include <android/log.h>
#include <format>
#include <string>
#include <string_view>

namespace pl::log {

    class Logger {
    public:
        explicit Logger(std::string name) : loggerName(std::move(name)) {}

        template <typename... Args>
        void info(std::string_view fmt_str, Args &&...args) const {
            log(ANDROID_LOG_INFO, fmt_str, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void debug(std::string_view fmt_str, Args &&...args) const {
            log(ANDROID_LOG_DEBUG, fmt_str, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void warn(std::string_view fmt_str, Args &&...args) const {
            log(ANDROID_LOG_WARN, fmt_str, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void error(std::string_view fmt_str, Args &&...args) const {
            log(ANDROID_LOG_ERROR, fmt_str, std::forward<Args>(args)...);
        }

    private:
        std::string loggerName;

        template <typename... Args>
        void log(int android_level, std::string_view fmt_str, Args &&...args) const {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));

            __android_log_print(android_level, loggerName.c_str(), "%s %s",
                                loggerName.c_str(), msg.c_str());
        }
    };

} // namespace pl::log