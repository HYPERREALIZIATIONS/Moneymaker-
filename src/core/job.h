#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../algo/algo.h"

namespace mm::core {

// A mining job received from a pool (or synthesised for benchmark mode).
//
// For all algorithms the "input" is `blob`, with the nonce patched at a
// fixed offset before hashing:
//   - RandomX : nonce at bytes 39..43 (Monero 76-byte blob convention)
//   - KawPow  : nonce at the last 8 bytes of the header
//   - GhostRider: nonce at bytes 39..43
struct Job {
    std::string job_id;
    Algo algo = Algo::RandomX;
    std::vector<uint8_t> blob;       // template; nonce patched in place while mining
    size_t nonce_offset = 39;         // where the 4/8-byte nonce lives in `blob`
    bool nonce_is_64 = false;         // KawPow uses an 8-byte nonce
    std::vector<uint8_t> target;      // 32 bytes, big-endian boundary
    uint64_t height = 0;              // block height / seed source
    uint64_t seed = 0;                // dataset/epoch seed (RandomX: first 32 bytes of blob as LE u64)
    bool clean = true;                // true => discard in-flight shares for previous job
};

// Returns true if the 32-byte little-endian `hash` is <= the big-endian
// `target` (i.e. the share meets difficulty). A fast 64-bit pre-check on the
// top bytes rejects almost all candidates before the full compare.
bool hash_meets_target(const uint8_t hash[32], const std::vector<uint8_t>& target);

// Build a 32-byte big-endian target from a 64-bit leading-zero boundary.
// `boundary` is the maximum value the top 8 bytes (big-endian) may have.
std::vector<uint8_t> target_from_u64(uint64_t boundary);

// Result of a found solution, submitted to the pool.
struct Share {
    std::string job_id;
    uint64_t nonce = 0;
    std::vector<uint8_t> hash;   // 32-byte resulting hash
    std::vector<uint8_t> mix;    // KawPow mix hash (empty for others)
    bool stale = false;
};

// Rolling hashrate statistics per backend.
struct Hashrate {
    double current = 0.0;  // H/s over the last window
    double average = 0.0;  // H/s since start
    uint64_t total_hashes = 0;
};

} // namespace mm::core
