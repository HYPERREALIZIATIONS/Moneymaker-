#include "kawpow_dag.h"

#include <ethash/ethash.hpp>
#include <ethash/keccak.hpp>

#include <cstring>

namespace mm::algo {

static inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint32_t fnv(uint32_t a, uint32_t b) {
    return (a * 0x01000193u) ^ b; // Ethash FNV
}

void build_kawpow_dag(uint64_t epoch, std::vector<uint8_t>& out_dag, uint32_t& out_items) {
    using namespace ethash;
    auto ctx = create_epoch_context(static_cast<int>(epoch)); // light cache
    const int num_cache = ctx->light_cache_num_items;
    const auto* cache = ctx->light_cache; // array of hash512 (16x uint32)

    int num_items = calculate_full_dataset_num_items(static_cast<int>(epoch));
    out_dag.resize(static_cast<size_t>(num_items) * 128);
    out_items = static_cast<uint32_t>(num_items);

    for (int i = 0; i < num_items; ++i) {
        uint8_t seed8[8];
        for (int k = 0; k < 8; ++k) seed8[k] = static_cast<uint8_t>((uint64_t(i) >> (8 * k)) & 0xFF);
        auto h = keccak512(seed8, 8); // 64 bytes

        uint32_t mix[32];
        for (int j = 0; j < 16; ++j) mix[j] = le32(h.bytes + j * 4);
        for (int j = 16; j < 32; ++j) mix[j] = mix[j - 16];

        // Dataset parents
        for (int p = 0; p < 256; ++p) {
            uint32_t parent = fnv(static_cast<uint32_t>(p) ^ static_cast<uint32_t>(i), mix[p % 16]) % num_cache;
            const auto& c = cache[parent]; // ethash_hash512 union, .word[16]
            for (int j = 0; j < 16; ++j) mix[j] = fnv(mix[j], c.word32s[j]);
        }
        // Cache rounds
        for (int c = 0; c < 3; ++c) {
            for (int j = 0; j < 16; ++j) {
                uint32_t parent = fnv(static_cast<uint32_t>(c) ^ static_cast<uint32_t>(i), mix[j]) % num_cache;
                const auto& cc = cache[parent];
                for (int k = 0; k < 16; ++k) mix[j] = fnv(mix[j], cc.word32s[k]);
            }
        }
        // Final fold with seed
        for (int j = 0; j < 16; ++j) mix[j] = fnv(mix[j], le32(h.bytes + j * 4));

        uint8_t mixbytes[128];
        for (int j = 0; j < 32; ++j) {
            mixbytes[j * 4 + 0] = static_cast<uint8_t>(mix[j] & 0xFF);
            mixbytes[j * 4 + 1] = static_cast<uint8_t>((mix[j] >> 8) & 0xFF);
            mixbytes[j * 4 + 2] = static_cast<uint8_t>((mix[j] >> 16) & 0xFF);
            mixbytes[j * 4 + 3] = static_cast<uint8_t>((mix[j] >> 24) & 0xFF);
        }
        auto out = keccak512(mixbytes, 128);
        std::memcpy(out_dag.data() + (size_t)i * 128, out.bytes, 128);
    }
}

} // namespace mm::algo
