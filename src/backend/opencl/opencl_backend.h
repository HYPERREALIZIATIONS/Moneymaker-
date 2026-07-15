#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "../../core/backend.h"
#include "../../core/job.h"
#include "../../algo/algo.h"

namespace mm::backend::opencl {

// AMD/portable OpenCL mining backend. Implements KawPow with full ProgPoW
// mixing in the OpenCL kernel (kawpow.cl); RandomX/GhostRider are CPU-only.
class OpenClBackend : public core::IBackend {
public:
    OpenClBackend(int device, Algo algo);
    ~OpenClBackend() override;

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

    void* cl_ctx_ = nullptr;     // cl_context
    void* cl_queue_ = nullptr;   // cl_command_queue
    void* cl_program_ = nullptr; // cl_program
    void* cl_kernel_ = nullptr;  // cl_kernel
    void* d_dag_ = nullptr;      // cl_mem
    uint32_t dag_items_ = 0;
    uint64_t current_epoch_ = ~0ull;

    uint64_t nonce_base_ = 0;
    core::HashrateTracker tracker_;
};

} // namespace mm::backend::opencl
