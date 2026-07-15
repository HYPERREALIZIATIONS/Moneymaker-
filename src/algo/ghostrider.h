#pragma once

#include <cstdint>
#include <vector>

// GhostRider (Raptoreum) hashing wrapper.
//
// GhostRider is a 3-stage chain:
//   1. RandomX   (real, via the vendored library)
//   2. yespower  (INTEGRATION POINT - see ghostrider.cpp)
//   3. CPUpower  (INTEGRATION POINT - see ghostrider.cpp)
//
// Stage 1 is implemented here. Stages 2 and 3 are marked integration points
// that should be wired to a GhostRider reference (RandomX + yespower +
// CPUpower) so the produced hash matches the network. Until then the wrapper
// returns the RandomX stage output.

namespace mm::algo {

class GhostRiderHasher {
public:
    GhostRiderHasher();
    ~GhostRiderHasher();

    GhostRiderHasher(const GhostRiderHasher&) = delete;
    GhostRiderHasher& operator=(const GhostRiderHasher&) = delete;

    // Build the RandomX seed context. `seed` is the 32-byte RandomX key
    // (typically keccak256 of the first 32 bytes of the blob).
    void prepare(const uint8_t seed[32], bool full, bool large_pages);

    // Hash `input` (len bytes) -> 32-byte `out`.
    void hash(const uint8_t* input, size_t len, uint8_t out[32]);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace mm::algo
