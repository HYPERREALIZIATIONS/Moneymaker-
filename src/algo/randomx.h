#pragma once

#include <cstdint>
#include <memory>
#include <vector>

// Thin, RAII wrappers around the vendored RandomX library
// (third_party/RandomX). A single RandomXContext (cache + optional full
// dataset) is shared across threads; each mining thread owns its own
// RandomXVm because a VM is not thread-safe.

struct randomx_cache;
struct randomx_dataset;
struct randomx_vm;

namespace mm::algo {

class RandomXContext {
public:
    RandomXContext();
    ~RandomXContext();

    RandomXContext(const RandomXContext&) = delete;
    RandomXContext& operator=(const RandomXContext&) = delete;

    // (Re)build the cache/dataset for a 32-byte seed. Rebuilds only when the
    // seed changes. `full` allocates the 2 GiB dataset (faster, more memory);
    // `large_pages` requests huge/large pages where supported.
    void prepare(const uint8_t seed[32], bool full, bool large_pages);

    bool is_full() const { return full_; }
    randomx_cache* cache() const { return cache_; }
    randomx_dataset* dataset() const { return dataset_; }

private:
    void release();
    bool build(const uint8_t seed[32], bool full, bool large_pages);

    uint8_t seed_[32] = {0};
    bool have_seed_ = false;
    bool full_ = false;
    randomx_cache* cache_ = nullptr;
    randomx_dataset* dataset_ = nullptr;
};

class RandomXVm {
public:
    // `full` must match the context it binds to.
    RandomXVm(RandomXContext& ctx, bool full);
    ~RandomXVm();

    RandomXVm(const RandomXVm&) = delete;
    RandomXVm& operator=(const RandomXVm&) = delete;

    // Hash `input` (len bytes) -> 32-byte `out`.
    void hash(const uint8_t* input, size_t len, uint8_t out[32]);

private:
    randomx_vm* vm_ = nullptr;
};

} // namespace mm::algo
