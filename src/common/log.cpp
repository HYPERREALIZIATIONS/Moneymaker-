#include "log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <vector>

namespace mm::common {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
}

static spdlog::level::level_enum to_spdlog(LogLevel l) {
    switch (l) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
    }
    return spdlog::level::info;
}

void log_init(std::string_view file_path, LogLevel level, bool console) {
    std::vector<spdlog::sink_ptr> sinks;
    if (console) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (!file_path.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            std::string(file_path), true));
    }
    if (sinks.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    g_logger = std::make_shared<spdlog::logger>("moneymaker", sinks.begin(), sinks.end());
    g_logger->set_level(to_spdlog(level));
    g_logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(g_logger);
}

void log_set_level(LogLevel level) {
    if (g_logger) g_logger->set_level(to_spdlog(level));
}

static void emit(LogLevel l, std::string_view msg) {
    if (!g_logger) log_init("", LogLevel::Info, true);
    g_logger->log(to_spdlog(l), "{}", msg);
}

void log_trace(std::string_view msg)    { emit(LogLevel::Trace, msg); }
void log_debug(std::string_view msg)    { emit(LogLevel::Debug, msg); }
void log_info(std::string_view msg)     { emit(LogLevel::Info, msg); }
void log_warn(std::string_view msg)     { emit(LogLevel::Warn, msg); }
void log_error(std::string_view msg)    { emit(LogLevel::Error, msg); }
void log_critical(std::string_view msg) { emit(LogLevel::Critical, msg); }

} // namespace mm::common
