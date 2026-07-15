#include "cuda_backend.h"

#include <cuda_runtime.h>
#include <ethash/ethash.hpp>

#include <vector>

#include "../../algo/kawpow_dag.h"
#include "../../common/log.h"
#include "../../common/util.h"

extern "C" void launch_kawpow_search(
    const uint8_t* header, int header_len,
    uint64_t start_nonce, uint32_t count,
    const uint8_t* dag, uint32_t dag_items,
    const uint8_t* target,
    uint32_t* result_found, uint64_t* result_nonce,
    uint8_t* result_hash, uint8_t* result_mix,
    cudaStream_t stream);

using namespace mm;
using namespace mm::backend::cuda;

CudaBackend::CudaBackend(int device, Algo algo) : device_(device), algo_(algo) {
    name_ = "cuda:" + std::to_string(device);
}

CudaBackend::~CudaBackend() {
    stop();
    if (d_dag_) cudaFree(d_dag_);
}

bool CudaBackend::init() {
    if (algo_ != Algo::KawPow) {
        log_warn(std::string(name_) + ": RandomX/GhostRider are CPU-only; CUDA backend idle");
        return true;
    }
    if (cudaSetDevice(device_) != cudaSuccess) {
        log_error(std::string(name_) + ": cudaSetDevice failed");
        return false;
    }
    log_info(std::string(name_) + ": CUDA device initialized");
    return true;
}

void CudaBackend::set_job(const core::Job& job) {
    if (algo_ != Algo::KawPow) return;
    job_ = job;
    have_job_ = true;

    uint64_t epoch = ethash::get_epoch_number(static_cast<int>(job.height));
    if (epoch != current_epoch_) {
        std::vector<uint8_t> dag;
        uint32_t items = 0;
        algo::build_kawpow_dag(epoch, dag, items);
        if (d_dag_) cudaFree(d_dag_);
        if (!dag.empty()) {
            cudaMalloc(&d_dag_, dag.size());
            cudaMemcpy(d_dag_, dag.data(), dag.size(), cudaMemcpyHostToDevice);
        }
        dag_items_ = items;
        current_epoch_ = epoch;
        log_info(std::string(name_) + ": built KawPow DAG (" + std::to_string(items) + " items)");
    }
}

void CudaBackend::start() {
    if (algo_ != Algo::KawPow || running_) return;
    running_ = true;
    thread_ = std::thread([this]() { worker(); });
}

void CudaBackend::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void CudaBackend::worker() {
    const uint32_t batch = 1u << 20;

    uint8_t* d_header = nullptr;
    uint8_t* d_target = nullptr;
    uint32_t* d_found = nullptr;
    uint64_t* d_nonce = nullptr;
    uint8_t* d_hash = nullptr;
    uint8_t* d_mix = nullptr;
    cudaMalloc(&d_header, 128);
    cudaMalloc(&d_target, 32);
    cudaMalloc(&d_found, batch * sizeof(uint32_t));
    cudaMalloc(&d_nonce, batch * sizeof(uint64_t));
    cudaMalloc(&d_hash, batch * 32);
    cudaMalloc(&d_mix, batch * 32);

    std::vector<uint32_t> h_found(batch);
    std::vector<uint64_t> h_nonce(batch);
    std::vector<uint8_t> h_hash(batch * 32);

    while (running_ && have_job_) {
        std::vector<uint8_t> hdr = job_.blob;
        cudaMemcpy(d_header, hdr.data(), hdr.size(), cudaMemcpyHostToDevice);
        cudaMemcpy(d_target, job_.target.data(), 32, cudaMemcpyHostToDevice);

        uint64_t start = nonce_base_;
        nonce_base_ += batch;

        launch_kawpow_search(d_header, static_cast<int>(hdr.size()), start, batch,
                             static_cast<const uint8_t*>(d_dag_), dag_items_, d_target,
                             d_found, d_nonce, d_hash, d_mix, nullptr);
        cudaMemcpy(h_found.data(), d_found, batch * sizeof(uint32_t), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_nonce.data(), d_nonce, batch * sizeof(uint64_t), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_hash.data(), d_hash, batch * 32, cudaMemcpyDeviceToHost);
        cudaDeviceSynchronize();

        for (uint32_t i = 0; i < batch; ++i) {
            if (h_found[i]) {
                core::Share s;
                s.job_id = job_.job_id;
                s.nonce = h_nonce[i];
                s.hash.assign(h_hash.begin() + i * 32, h_hash.begin() + i * 32 + 32);
                emit_share(s);
            }
        }
        tracker_.add(batch);
    }

    cudaFree(d_header); cudaFree(d_target); cudaFree(d_found);
    cudaFree(d_nonce); cudaFree(d_hash); cudaFree(d_mix);
}
