#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

#include <spdlog/spdlog.h>

#include <memory>

namespace core {

class Log {
public:
    static bool init();

    inline static std::shared_ptr<spdlog::logger> &getLogger() { return m_logger; }

private:
    static std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace core

#define TRACE(...) ::core::Log::getLogger()->trace(__VA_ARGS__)
#define INFO(...) ::core::Log::getLogger()->info(__VA_ARGS__)
#define WARN(...) ::core::Log::getLogger()->warn(__VA_ARGS__)
#define ERROR(...) ::core::Log::getLogger()->error(__VA_ARGS__)

#endif