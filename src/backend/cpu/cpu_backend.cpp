#include "cpu_backend.h"

#include <ethash/keccak.hpp>
#include <ethash/ethash.hpp>

#include <algorithm>
#include <chrono>
#include <random>

#include "../../algo/randomx.h"
#include "../../algo/kawpow.h"
#include "../../common/log.h"
#include "../../common/util.h"

using namespace mm;

namespace mm::backend::cpu {

static int hardware_threads() {
    int n = static_cast<int>(std::thread::hardware_concurrency());
    return n > 0 ? n : 1;
}

CpuBackend::CpuBackend(const config::CpuConfig& cfg) : cfg_(cfg) {
    num_threads_ = cfg_.threads > 0 ? static_cast<uint32_t>(cfg_.threads) : hardware_threads();
}

CpuBackend::~CpuBackend() { stop(); }

bool CpuBackend::init() {
    log_info("CPU backend: " + std::to_string(num_threads_) + " thread(s)" +
             (cfg_.full_dataset ? ", full RandomX dataset" : ", RandomX light mode"));
    return true;
}

// Derive the 32-byte RandomX seed from the blob (keccak256 of first 32 bytes),
// matching the xmrig/Monero convention.
static void derive_rx_seed(const std::vector<uint8_t>& blob, uint8_t seed[32]) {
    if (blob.size() >= 32) {
        auto h = ethash::keccak256(blob.data(), 32);
        std::memcpy(seed, h.bytes, 32);
    } else {
        auto h = ethash::keccak256(blob.data(), blob.size());
        std::memcpy(seed, h.bytes, 32);
    }
}

void CpuBackend::prepare_contexts(const core::Job& job) {
    std::lock_guard<std::mutex> lk(ctx_mutex_);
    need_rx_ = (job.algo == Algo::RandomX || job.algo == Algo::GhostRider);
    need_kawpow_ = (job.algo == Algo::KawPow);

    if (need_rx_) {
        uint8_t seed[32];
        derive_rx_seed(job.blob, seed);
        rx_ctx_.prepare(seed, cfg_.full_dataset, cfg_.huge_pages);
    }
    if (need_kawpow_) {
        kawpow_.prepare(job.height);
    }
}

void CpuBackend::set_job(const core::Job& job) {
    core::Job prepared = job;
    if (prepared.algo == Algo::KawPow) {
        prepared.nonce_offset = prepared.blob.size() >= 8 ? prepared.blob.size() - 8 : 0;
        prepared.nonce_is_64 = true;
    } else {
        prepared.nonce_offset = 39;
        prepared.nonce_is_64 = false;
    }

    // Determine whether the heavy context (RandomX seed / KawPow epoch) must
    // be rebuilt. Rebuilding is rare (seed/epoch change), so we safely stop
    // workers, rebuild, and restart to avoid touching live contexts.
    uint8_t seed[32] = {0};
    if (prepared.algo == Algo::RandomX || prepared.algo == Algo::GhostRider)
        derive_rx_seed(prepared.blob, seed);
    std::string seed_hex = common::to_hex(seed, 32);
    uint64_t epoch = (prepared.algo == Algo::KawPow)
        ? ethash::get_epoch_number(static_cast<int>(prepared.height)) : 0;

    bool rebuild = (seed_hex != last_seed_ || epoch != last_epoch_);

    {
        std::lock_guard<std::mutex> lk(job_mutex_);
        job_ = prepared;
    }

    if (rebuild) {
        stop();
        prepare_contexts(job_);
        last_seed_ = seed_hex;
        last_epoch_ = epoch;
        start(); // restart workers with the new contexts
    } else {
        prepare_contexts(job_); // no-op for unchanged seed/epoch
        job_gen_.fetch_add(1, std::memory_order_relaxed);
    }
}

void CpuBackend::start() {
    if (running_) return;
    running_ = true;
    tracker_.reset();
    nonce_counter_.store(std::random_device{}() | 1u);
    for (uint32_t i = 0; i < num_threads_; ++i) {
        threads_.emplace_back([this, i]() { worker(i); });
    }
}

void CpuBackend::stop() {
    running_ = false;
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
}

void CpuBackend::worker(uint32_t thread_index) {
    (void)thread_index;
    // Per-thread RandomX VM (recreated when the seed changes).
    std::unique_ptr<algo::RandomXVm> rx_vm;
    std::string last_seed;

    core::Job local = job_;
    uint64_t my_gen = 0;

    // Local job copy + reload on generation change.
    auto reload_if_needed = [&]() -> bool {
        uint64_t g = job_gen_.load(std::memory_order_acquire);
        if (g == my_gen) return true;
        {
            std::lock_guard<std::mutex> lk(job_mutex_);
            local = job_;
        }
        my_gen = g;
        return false; // caller should re-check contexts
    };

    // Seed tracking for vm (re)creation.
    auto ensure_vm = [&](const uint8_t seed[32]) {
        std::string s = common::to_hex(seed, 32);
        if (s != last_seed) {
            rx_vm = std::make_unique<algo::RandomXVm>(rx_ctx_, cfg_.full_dataset);
            last_seed = s;
        }
    };

    std::vector<uint8_t> blob;
    uint8_t hash[32];
    uint8_t mix[32];

    while (running_) {
        reload_if_needed();
        if (my_gen == 0) { // never got a job
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // (Re)bind RandomX VM to the current seed if needed.
        if (need_rx_) {
            uint8_t seed[32];
            derive_rx_seed(local.blob, seed);
            ensure_vm(seed);
        }

        blob = local.blob;
        size_t off = local.nonce_offset;
        bool is64 = local.nonce_is_64;
        const auto& target = local.target;
        bool rx = (local.algo == Algo::RandomX || local.algo == Algo::GhostRider);

        // Grab a batch of nonces to amortise atomics/locks.
        constexpr uint64_t batch = 256;
        uint64_t start_n = nonce_counter_.fetch_add(batch, std::memory_order_relaxed);
        for (uint64_t k = 0; k < batch; ++k) {
            uint64_t n = start_n + k;
            if (is64) {
                for (int i = 0; i < 8; ++i) blob[off + i] = static_cast<uint8_t>((n >> (8 * i)) & 0xFF);
            } else {
                uint32_t n32 = static_cast<uint32_t>(n);
                for (int i = 0; i < 4; ++i) blob[off + i] = static_cast<uint8_t>((n32 >> (8 * i)) & 0xFF);
            }

            if (rx) {
                if (!rx_vm) break;
                rx_vm->hash(blob.data(), blob.size(), hash);
            } else { // KawPow
                kawpow_.hash(blob.data(), blob.size(), hash, mix);
            }

            tracker_.add(1);

            if (core::hash_meets_target(hash, target)) {
                core::Share share;
                share.job_id = local.job_id;
                share.nonce = n;
                share.hash.assign(hash, hash + 32);
                if (is64) share.mix.assign(mix, mix + 32);
                emit_share(share);
            }
        }
    }
}

} // namespace mm::backend::cpu
