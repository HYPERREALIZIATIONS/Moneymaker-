#include "miner.h"

#include <chrono>

#include "../backend/cpu/cpu_backend.h"
#ifdef HAVE_CUDA
    #include "../backend/cuda/cuda_backend.h"
#endif
#ifdef HAVE_OPENCL
    #include "../backend/opencl/opencl_backend.h"
#endif
#include "../benchmark/benchmark.h"
#include "../stratum/stratum_client.h"
#include "../common/log.h"
#include "../common/util.h"

using namespace mm;
using namespace mm::core;

Miner::Miner(const config::Config& cfg) : cfg_(cfg) {}

Miner::~Miner() {
    stop();
    for (auto& b : backends_) b->stop();
    if (stats_thread_.joinable()) stats_thread_.join();
}

void Miner::build_backends() {
    if (cfg_.cpu.enabled) {
        auto b = std::make_unique<backend::cpu::CpuBackend>(cfg_.cpu);
        wire_backend(b.get());
        backends_.push_back(std::move(b));
    }
#ifdef HAVE_CUDA
    if (cfg_.cuda.enabled) {
        for (int dev : cfg_.cuda.devices) {
            auto b = std::make_unique<backend::cuda::CudaBackend>(dev, cfg_.algo);
            wire_backend(b.get());
            backends_.push_back(std::move(b));
        }
    }
#endif
#ifdef HAVE_OPENCL
    if (cfg_.opencl.enabled) {
        for (int dev : cfg_.opencl.devices) {
            auto b = std::make_unique<backend::opencl::OpenClBackend>(dev, cfg_.algo);
            wire_backend(b.get());
            backends_.push_back(std::move(b));
        }
    }
#endif
}

void Miner::wire_backend(core::IBackend* b) {
    b->on_share = [this](const core::Share& s) { on_share(s); };
    b->on_log = [](const std::string& m) { log_info(m); };
}

void Miner::on_job(const core::Job& job) {
    for (auto& b : backends_) b->set_job(job);
}

void Miner::on_share(const core::Share& s) {
    ++shares_found_;
    if (client_ && client_->is_connected()) {
        client_->submit(s);
    } else {
        log_info("share found: " + common::to_hex(s.hash));
    }
}

void Miner::stats_loop() {
    using namespace std::chrono;
    while (!stop_) {
        std::this_thread::sleep_for(seconds(5));
        double total = 0;
        std::string line = "HASHRATE ";
        for (auto& b : backends_) {
            auto h = b->hashrate();
            total += h.current;
            line += std::string("[") + b->name() + ":" + common::format_hashrate(h.current) + "] ";
        }
        line += "total=" + common::format_hashrate(total);
        line += " shares=" + std::to_string(shares_found_);
        log_info(line);
    }
}

void Miner::run_benchmark() {
    benchmark::run(cfg_, backends_);
}

bool Miner::run() {
    build_backends();
    if (backends_.empty()) {
        log_error("no mining backends are enabled/available");
        return false;
    }

    if (cfg_.benchmark.enabled) {
        run_benchmark();
        return true;
    }

    if (cfg_.pools.empty()) {
        log_error("no pools configured and benchmark disabled");
        return false;
    }

    client_ = std::make_unique<stratum::StratumClient>(cfg_.pools[0]);
    client_->on_job([this](const core::Job& j) { on_job(j); });
    client_->on_log([](const std::string& m) { log_info(m); });
    if (!client_->connect()) {
        return false;
    }

    for (auto& b : backends_) b->start();

    stop_ = false;
    stats_thread_ = std::thread([this]() { stats_loop(); });

    while (!stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    for (auto& b : backends_) b->stop();
    client_->disconnect();
    if (stats_thread_.joinable()) stats_thread_.join();
    return true;
}
