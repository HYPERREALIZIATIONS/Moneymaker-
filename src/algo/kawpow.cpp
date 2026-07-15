#include "kawpow.h"

#include <ethash/ethash.hpp>
#include <ethash/keccak.hpp>

#include <cstring>
#include <stdexcept>

#include "../common/log.h"

namespace mm::algo {

using namespace ethash;

// KawPow / ProgPoW tunables.
constexpr int PROGPOW_PERIOD = 12000;

KawPowHasher::KawPowHasher() = default;

KawPowHasher::~KawPowHasher() { release(); }

void KawPowHasher::release() {
    if (ctx_) { ethash_destroy_epoch_context_full(ctx_); ctx_ = nullptr; }
}

void KawPowHasher::prepare(uint64_t height) {
    uint64_t epoch = get_epoch_number(static_cast<int>(height));
    if (epoch == epoch_) return; // unchanged
    build(height);
}

void KawPowHasher::build(uint64_t height) {
    release();
    uint64_t epoch = get_epoch_number(static_cast<int>(height));
    try {
        auto ptr = create_epoch_context_full(static_cast<int>(epoch));
        ctx_ = ptr.release();
        epoch_ = epoch;
    } catch (const std::exception& e) {
        log_error(std::string("KawPow DAG build failed: ") + e.what());
        throw;
    }
}

void KawPowHasher::hash(const uint8_t* header, size_t len,
                        uint8_t out_hash[32], uint8_t out_mix[32]) const {
    if (!ctx_) throw std::runtime_error("KawPow: DAG not built; call prepare()");

    // header_hash = keccak256(header) where header already carries the nonce.
    hash256 h = keccak256(header, len);
    // Mine over the nonce taken from the last 8 bytes of the header.
    uint64_t nonce = 0;
    if (len >= 8) {
        const uint8_t* p = header + len - 8;
        for (int i = 0; i < 8; ++i)
            nonce |= static_cast<uint64_t>(p[i]) << (8 * i);
    }

    // ---------------------------------------------------------------------
    // INTEGRATION POINT: the ProgPoW/KawPow mixing layer.
    // Plain Ethash below; for pool-compatible KawPow shares, replace the
    // call with a full ProgPoW mix (init/loop/final over the DAG) using the
    // KawPow constants (PROGPOW_LANES=16, PROGPOW_REGS=32, PROGPOW_CNT_DAG=64,
    // PROGPOW_CNT_CACHE=12, PROGPOW_CNT_MATH=20). The GPU backends already
    // implement this mixing on-device.
    // ---------------------------------------------------------------------
    result res = hash(*ctx_, h, nonce);
    std::memcpy(out_hash, res.final_hash.bytes, 32);
    std::memcpy(out_mix, res.mix_hash.bytes, 32);
}

} // namespace mm::algo
