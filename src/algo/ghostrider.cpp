#include "ghostrider.h"

#include "randomx.h"

#include <cstring>

#include "../common/log.h"

namespace mm::algo {

struct GhostRiderHasher::Impl {
    RandomXContext rx;
    RandomXVm* vm = nullptr;
    bool full = false;
};

GhostRiderHasher::GhostRiderHasher() : impl_(new Impl()) {}

GhostRiderHasher::~GhostRiderHasher() {
    delete impl_->vm;
    delete impl_;
}

void GhostRiderHasher::prepare(const uint8_t seed[32], bool full, bool large_pages) {
    impl_->rx.prepare(seed, full, large_pages);
    impl_->full = full;
    delete impl_->vm;
    impl_->vm = new RandomXVm(impl_->rx, full);
}

void GhostRiderHasher::hash(const uint8_t* input, size_t len, uint8_t out[32]) {
    if (!impl_->vm) {
        log_error("GhostRider: hash() called before prepare()");
        std::memset(out, 0, 32);
        return;
    }

    // Stage 1: RandomX
    uint8_t stage1[32];
    impl_->vm->hash(input, len, stage1);

    // ---------------------------------------------------------------------
    // INTEGRATION POINT: GhostRider stages 2 and 3.
    //   2. yespower(stage1)  -> 32 bytes
    //   3. CPUpower(result)  -> 32 bytes (final)
    // Wire a GhostRider reference (RandomX + yespower + CPUpower) here so the
    // output matches the Raptoreum network. Until then we return stage 1.
    // ---------------------------------------------------------------------
    std::memcpy(out, stage1, 32);
}

} // namespace mm::algo
