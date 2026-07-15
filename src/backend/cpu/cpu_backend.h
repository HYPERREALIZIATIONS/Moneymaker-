#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../core/backend.h"
#include "../../core/job.h"
#include "../../config/config.h"

namespace mm::backend::cpu {

// Unified CPU mining backend. Runs RandomX, KawPow and GhostRider across a
// pool of worker threads. The RandomX cache/dataset is shared (one copy),
// while each thread owns its own RandomX VM (VMs are not thread-safe).
class CpuBackend : public core::IBackend {
public:
    explicit CpuBackend(const config::CpuConfig& cfg);
    ~CpuBackend() override;

    const char* name() const override { return "cpu"; }

    bool init() override;
    void set_job(const core::Job& job) override;
    void start() override;
    void stop() override;
    core::Hashrate hashrate() const override { return tracker_.snapshot(); }

private:
    void worker(uint32_t thread_index);
    void prepare_contexts(const core::Job& job);

    config::CpuConfig cfg_;
    uint32_t num_threads_ = 0;

    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};

    core::Job job_;
    std::mutex job_mutex_;
    std::atomic<uint64_t> job_gen_{0};

    std::atomic<uint64_t> nonce_counter_{0};
    core::HashrateTracker tracker_;

    // Shared algorithm state (thread-safe to read).
    algo::RandomXContext rx_ctx_;
    algo::KawPowHasher kawpow_;
    bool need_rx_ = false;     // current job uses RandomX-family
    bool need_kawpow_ = false; // current job uses KawPow
    std::mutex ctx_mutex_;

    std::string last_seed_;    // last RandomX seed (hex) used to build contexts
    uint64_t last_epoch_ = 0;  // last KawPow epoch used to build the DAG
};

} // namespace mm::backend::cpu
