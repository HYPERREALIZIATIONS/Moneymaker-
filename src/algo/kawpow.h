#pragma once

#include <cstdint>
#include <vector>

// KawPow (Ravencoin) hashing wrapper.
//
// CPU path: uses the vendored ethash library to compute the base Ethash
// hash + mix over the generated DAG. The ProgPoW mixing layer that
// distinguishes KawPow from plain Ethash is the documented integration
// point (see kawpow.cpp, `kawpow_mix`); wire a KawPow reference there for
// pool-compatible shares. GPU backends implement the full ProgPoW mixing
// on-device, so the algorithm is consistent across devices.
//
// Mining convention: the blob is the block header with the 8-byte nonce
// stored in its final 8 bytes (little-endian). header_hash = keccak256(blob).

struct ethash_epoch_context_full;

namespace mm::algo {

class KawPowHasher {
public:
    KawPowHasher();
    ~KawPowHasher();

    KawPowHasher(const KawPowHasher&) = delete;
    KawPowHasher& operator=(const KawPowHasher&) = delete;

    // Build the DAG for the epoch implied by `height`
    // (epoch = height / 30000, matching Ethash). Rebuilds only on change.
    void prepare(uint64_t height);

    // Hash `header` (includes the nonce in the last 8 bytes) -> 32-byte
    // `out_hash` and 32-byte `out_mix`. Read-only; safe to call concurrently.
    void hash(const uint8_t* header, size_t len,
              uint8_t out_hash[32], uint8_t out_mix[32]) const;

    uint64_t epoch() const { return epoch_; }

private:
    void release();
    void build(uint64_t height);

    uint64_t epoch_ = ~0ull;
    ethash_epoch_context_full* ctx_ = nullptr;
};

} // namespace mm::algo
