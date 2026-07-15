#include "util.h"

#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace mm::common {

static const char* kHexLower = "0123456789abcdef";

std::string to_hex(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kHexLower[(data[i] >> 4) & 0xF]);
        out.push_back(kHexLower[data[i] & 0xF]);
    }
    return out;
}

std::string to_hex(std::string_view bytes) {
    return to_hex(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

std::string to_hex(const std::vector<uint8_t>& v) {
    return to_hex(v.data(), v.size());
}

bool from_hex(std::string_view hex, std::vector<uint8_t>& out) {
    out.clear();
    if (hex.size() % 2 != 0) return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = nibble(hex[i]);
        int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::vector<uint8_t> from_hex_throw(std::string_view hex, size_t expected) {
    std::vector<uint8_t> out;
    if (!from_hex(hex, out)) {
        throw std::runtime_error("invalid hex string: " + std::string(hex));
    }
    if (expected != 0 && out.size() != expected) {
        throw std::runtime_error("hex length mismatch: expected " +
            std::to_string(expected) + " bytes, got " + std::to_string(out.size()));
    }
    return out;
}

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            out.push_back(std::string(s.substr(start)));
            break;
        }
        out.push_back(std::string(s.substr(start, pos - start)));
        start = pos + 1;
    }
    return out;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

double parse_hashrate(std::string_view s) {
    std::string t = trim(s);
    if (t.empty()) return 0.0;
    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str()) return 0.0;
    double mult = 1.0;
    if (*end != '\0') {
        char suf = static_cast<char>(std::tolower(static_cast<unsigned char>(*end)));
        if (suf == 'k') mult = 1e3;
        else if (suf == 'm') mult = 1e6;
        else if (suf == 'g') mult = 1e9;
        else if (suf == 't') mult = 1e12;
    }
    return v * mult;
}

std::string format_hashrate(double hps) {
    const char* units[] = {"H/s", "kH/s", "MH/s", "GH/s", "TH/s"};
    double v = hps;
    int u = 0;
    while (v >= 1000.0 && u < 4) { v /= 1000.0; ++u; }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f %s", v, units[u]);
    return std::string(buf);
}

uint64_t steady_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace mm::common
