#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mm {

// Supported mining algorithms.
enum class Algo {
    RandomX,   // Monero (CPU only)
    KawPow,    // Ravencoin (CPU + CUDA + OpenCL)
    GhostRider // Raptoreum (CPU; GPU phase-2)
};

inline const char* algo_to_string(Algo a) {
    switch (a) {
        case Algo::RandomX:   return "randomx";
        case Algo::KawPow:    return "kawpow";
        case Algo::GhostRider:return "ghostrider";
    }
    return "unknown";
}

inline Algo algo_from_string(std::string_view s) {
    if (s == "randomx" || s == "rx")     return Algo::RandomX;
    if (s == "kawpow" || s == "kp")      return Algo::KawPow;
    if (s == "ghostrider" || s == "gr")  return Algo::GhostRider;
    return Algo::RandomX;
}

} // namespace mm
