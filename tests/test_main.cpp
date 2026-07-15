#include "common/util.h"
#include "core/job.h"

#include <cassert>
#include <cstdio>

using namespace mm;

int main() {
    // Hex round-trip
    std::vector<uint8_t> raw = {0x00, 0xab, 0xcd, 0xef, 0x10};
    std::string hex = common::to_hex(raw);
    assert(hex == "00abcdef10");
    std::vector<uint8_t> back;
    assert(common::from_hex(hex, back) && back == raw);

    // Target comparison: a hash equal to the target meets it.
    uint8_t hash[32];
    for (int i = 0; i < 32; ++i) hash[i] = 0;
    auto target = core::target_from_u64(0x0000FFFF00000000ULL);
    assert(core::hash_meets_target(hash, target) == true);

    // A hash with a larger top byte does not meet a tight target.
    uint8_t tight[32];
    for (int i = 0; i < 32; ++i) tight[i] = 0;
    tight[0] = 0x01; // top byte 0x01 > 0x00
    auto tight_target = core::target_from_u64(0x000000FF00000000ULL);
    assert(core::hash_meets_target(tight, tight_target) == false);

    std::printf("all tests passed\n");
    return 0;
}
