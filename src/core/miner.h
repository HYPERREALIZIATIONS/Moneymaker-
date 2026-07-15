#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "backend.h"
#include "../config/config.h"

namespace mm::core {

// Top-level coordinator: builds backends (CPU always; CUDA/OpenCL when
// enabled and available), connects to the pool, dispatches jobs to all
// backends, submits found shares, and prints periodic hashrate stats.
class Miner {
public:
    explicit Miner(const config::Config& cfg);
    ~Miner();

    Miner(const Miner&) = delete;
    Miner& operator=(const Miner&) = delete;

    // Run until stop() is called (pool mode) or the benchmark finishes.
    // Returns false on a fatal configuration/runtime error.
    bool run();

    // Request a graceful shutdown (used by the signal handler).
    void stop() { stop_.store(true); }

private:
    void build_backends();
    void wire_backend(core::IBackend* b);
    void on_job(const core::Job& job);
    void on_share(const core::Share& share);
    void stats_loop();
    void run_benchmark();

    config::Config cfg_;
    std::vector<std::unique_ptr<core::IBackend>> backends_;
    std::unique_ptr<stratum::StratumClient> client_;

    std::atomic<bool> stop_{false};
    std::thread stats_thread_;
    uint64_t shares_found_ = 0;
};

} // namespace mm::core
