#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <functional>
#include <string>
#include <vector>

#include "job.h"
#include "../../common/util.h"

namespace mm::core {

// A mining backend (CPU threads, a CUDA device, or an OpenCL device).
// The Miner owns a set of backends and dispatches jobs to all of them.
class IBackend {
public:
    virtual ~IBackend() = default;

    // Human-readable backend label, e.g. "cpu", "cuda:0", "opencl:0".
    virtual const char* name() const = 0;

    // One-time hardware initialisation (detect devices, allocate context).
    // Returns false on fatal error.
    virtual bool init() = 0;

    // Push a new job. Backends should finish any in-flight work for the
    // previous job before applying `clean` jobs.
    virtual void set_job(const Job& job) = 0;

    // Start the mining loop(s) on the current job.
    virtual void start() = 0;

    // Stop the mining loop(s) and join worker threads.
    virtual void stop() = 0;

    // Rolling + total hashrate for this backend.
    virtual Hashrate hashrate() const = 0;

    // --- callbacks wired by the Miner ---
    std::function<void(const Share&)> on_share;
    std::function<void(const std::string&)> on_log;

protected:
    // Helper for backends to report a share safely through the callback.
    void emit_share(const Share& s) const {
        if (on_share) on_share(s);
    }
    void emit_log(const std::string& m) const {
        if (on_log) on_log(m);
    }
};

// Tracks hashes over a sliding window to report current + average hashrate.
// Safe to call add() from multiple worker threads.
class HashrateTracker {
public:
    void add(uint64_t hashes) {
        total_.fetch_add(hashes, std::memory_order_relaxed);
        uint64_t wc = window_count_.fetch_add(hashes, std::memory_order_relaxed) + hashes;
        if (wc >= kWindowHashes || steady_ms() - window_start_.load() > kWindowMs) {
            std::lock_guard<std::mutex> lk(mtx_);
            uint64_t dt = steady_ms() - window_start_.load();
            if (dt > 0) {
                double cur = static_cast<double>(window_count_.load()) / (dt / 1000.0);
                current_ = current_ == 0.0 ? cur : (current_ * 0.7 + cur * 0.3);
            }
            window_count_.store(0);
            window_start_.store(steady_ms());
        }
    }

    Hashrate snapshot() const {
        Hashrate h;
        uint64_t total = total_.load();
        h.current = current_;
        h.average = total == 0 ? 0.0
            : static_cast<double>(total) / ((steady_ms() - start_) / 1000.0 + 1e-9);
        h.total_hashes = total;
        return h;
    }

    void reset() {
        total_.store(0); window_count_.store(0); current_ = 0.0;
        start_ = window_start_ = steady_ms();
    }

private:
    static constexpr uint64_t kWindowHashes = 4096;
    static constexpr uint64_t kWindowMs = 1000;

    std::atomic<uint64_t> total_{0};
    std::atomic<uint64_t> window_count_{0};
    std::atomic<uint64_t> window_start_{0};
    uint64_t start_ = steady_ms();
    double current_ = 0.0;
    mutable std::mutex mtx_;
};

} // namespace mm::core
