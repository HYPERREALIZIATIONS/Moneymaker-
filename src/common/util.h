#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mm::common {

// RAII helper to make a type non-copyable (move-only friendly).
class NonCopyable {
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

// Convert a byte buffer to a lowercase hex string.
std::string to_hex(const uint8_t* data, size_t len);
std::string to_hex(std::string_view bytes);
std::string to_hex(const std::vector<uint8_t>& v);

// Parse a hex string (case-insensitive, any length) into bytes.
// Returns false on malformed input (odd length or non-hex chars).
bool from_hex(std::string_view hex, std::vector<uint8_t>& out);

// Parse a hex string into exactly `expected` bytes (or as many as present when
// expected == 0). Throws std::runtime_error on mismatch.
std::vector<uint8_t> from_hex_throw(std::string_view hex, size_t expected = 0);

// Trim whitespace from both ends of a string view (returns a copy).
std::string trim(std::string_view s);

// Split a string by a delimiter.
std::vector<std::string> split(std::string_view s, char delim);

// Case-insensitive string compare.
bool iequals(std::string_view a, std::string_view b);

// Parse a human hashrate string like "120.5k", "4.2M", "1.1G" into H/s.
double parse_hashrate(std::string_view s);

// Format H/s into a compact human string.
std::string format_hashrate(double hps);

// Current time as milliseconds since epoch (steady, monotonic).
uint64_t steady_ms();

} // namespace mm::common
