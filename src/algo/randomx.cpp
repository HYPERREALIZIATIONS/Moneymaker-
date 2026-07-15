#include "randomx.h"

#include <randomx.h>

#include <cstring>
#include <stdexcept>

#include "../common/log.h"

namespace mm::algo {

RandomXContext::RandomXContext() = default;

RandomXContext::~RandomXContext() { release(); }

void RandomXContext::release() {
    if (dataset_) { randomx_release_dataset(dataset_); dataset_ = nullptr; }
    if (cache_)   { randomx_release_cache(cache_);     cache_ = nullptr; }
    have_seed_ = false;
}

void RandomXContext::prepare(const uint8_t seed[32], bool full, bool large_pages) {
    if (have_seed_ && std::memcmp(seed_, seed, 32) == 0 && full == full_) {
        return; // unchanged
    }
    release();

    randomx_flags flags = randomx_get_flags();
    // JIT and hardware AES are already selected by randomx_get_flags when
    // supported; we just add the optional flags.
    if (large_pages) flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_LARGE_PAGES);
    if (full)        flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_FULL_MEM);

    cache_ = randomx_alloc_cache(flags);
    if (!cache_) {
        throw std::runtime_error("RandomX: failed to allocate cache");
    }
    randomx_init_cache(cache_, seed, 32);

    if (full) {
        dataset_ = randomx_alloc_dataset(flags);
        if (!dataset_) {
            throw std::runtime_error("RandomX: failed to allocate dataset");
        }
        unsigned long items = randomx_dataset_item_count();
        randomx_init_dataset(dataset_, cache_, 0, items);
    }

    std::memcpy(seed_, seed, 32);
    have_seed_ = true;
    full_ = full;
}

bool RandomXContext::build(const uint8_t seed[32], bool full, bool large_pages) {
    try {
        prepare(seed, full, large_pages);
        return true;
    } catch (const std::exception& e) {
        mm::common::log_error(std::string("RandomX build failed: ") + e.what());
        return false;
    }
}

RandomXVm::RandomXVm(RandomXContext& ctx, bool full) {
    randomx_flags flags = randomx_get_flags();
    if (full) flags = static_cast<randomx_flags>(flags | RANDOMX_FLAG_FULL_MEM);
    vm_ = randomx_create_vm(flags, full ? nullptr : ctx.cache(), full ? ctx.dataset() : nullptr);
    if (!vm_) {
        throw std::runtime_error("RandomX: failed to create VM");
    }
}

RandomXVm::~RandomXVm() {
    if (vm_) randomx_destroy_vm(vm_);
}

void RandomXVm::hash(const uint8_t* input, size_t len, uint8_t out[32]) {
    randomx_calculate_hash(vm_, input, len, out);
}

} // namespace mm::algo
