#include "log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <iostream>

namespace core {

std::shared_ptr<spdlog::logger> Log::m_logger;

class StaticClass {
public:
    StaticClass();
};

static StaticClass staticClass{};

StaticClass::StaticClass() {
    if (!Log::init()) {
        std::cout << "Failed to initialize logger!\n";
        std::terminate();
    }
}

bool Log::init() {
    bool ok = false;

    std::vector<spdlog::sink_ptr> logSink;
    logSink.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    spdlog::set_pattern("%v");
    m_logger = std::make_shared<spdlog::logger>("Engine", begin(logSink), end(logSink));
    if (m_logger) {
        ok = true;
        spdlog::register_logger(m_logger);
        m_logger->set_level(spdlog::level::trace);
        m_logger->flush_on(spdlog::level::trace);
    }

    return ok;
}

} // namespace core
