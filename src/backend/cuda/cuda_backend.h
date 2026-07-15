#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "../../core/backend.h"
#include "../../core/job.h"
#include "../../algo/algo.h"

namespace mm::backend::cuda {

// NVIDIA CUDA mining backend. Implements KawPow (the GPU-friendly algorithm)
// with the full ProgPoW mixing performed on-device; RandomX/GhostRider are
// CPU-only and reported as unsupported here.
class CudaBackend : public core::IBackend {
public:
    CudaBackend(int device, Algo algo);
    ~CudaBackend() override;

    const char* name() const override { return name_.c_str(); }

    bool init() override;
    void set_job(const core::Job& job) override;
    void start() override;
    void stop() override;
    core::Hashrate hashrate() const override { return tracker_.snapshot(); }

private:
    void worker();

    int device_;
    Algo algo_;
    std::string name_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    core::Job job_;
    std::atomic<bool> have_job_{false};

    // Device pointers (KawPow DAG + working buffers).
    void* d_dag_ = nullptr;
    uint32_t dag_items_ = 0;
    uint64_t current_epoch_ = ~0ull;

    uint64_t nonce_base_ = 0;
    core::HashrateTracker tracker_;
};

} // namespace mm::backend::cuda
