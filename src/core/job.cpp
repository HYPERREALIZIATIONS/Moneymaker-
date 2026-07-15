#include "job.h"

namespace mm::core {

bool hash_meets_target(const uint8_t hash[32], const std::vector<uint8_t>& target) {
    if (target.size() != 32) return false;
    // Fast pre-check: compare the top 8 bytes (big-endian) first.
    uint64_t h_top = 0, t_top = 0;
    for (int i = 0; i < 8; ++i) {
        h_top = (h_top << 8) | hash[i];
        t_top = (t_top << 8) | target[i];
    }
    if (h_top > t_top) return false;
    if (h_top < t_top) return true;
    // Top bytes equal: full big-endian compare.
    for (int i = 0; i < 32; ++i) {
        if (hash[i] < target[i]) return true;
        if (hash[i] > target[i]) return false;
    }
    return true; // equal
}

std::vector<uint8_t> target_from_u64(uint64_t boundary) {
    std::vector<uint8_t> t(32, 0);
    // Store `boundary` in the top 8 bytes, big-endian.
    for (int i = 0; i < 8; ++i) {
        t[i] = static_cast<uint8_t>((boundary >> (8 * (7 - i))) & 0xFF);
    }
    return t;
}

} // namespace mm::core
