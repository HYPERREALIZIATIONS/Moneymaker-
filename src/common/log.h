#pragma once

#include <string>
#include <string_view>

namespace mm::common {

// Thin wrapper around spdlog so the rest of the codebase has a stable,
// minimal logging API that can be swapped or extended later.
enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical };

void log_init(std::string_view file_path, LogLevel level, bool console);
void log_set_level(LogLevel level);

void log_trace(std::string_view msg);
void log_debug(std::string_view msg);
void log_info(std::string_view msg);
void log_warn(std::string_view msg);
void log_error(std::string_view msg);
void log_critical(std::string_view msg);

} // namespace mm::common
