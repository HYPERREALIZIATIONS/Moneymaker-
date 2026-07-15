#include "benchmark.h"

#include <random>

#include "../common/log.h"
#include "../common/util.h"
#include "../core/backend.h"

using namespace mm;

namespace mm::benchmark {

core::Job make_job(Algo algo) {
    core::Job job;
    job.algo = algo;
    job.job_id = "benchmark";
    job.height = 0;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> d(0, 255);

    // Blob sizes follow each algorithm's header convention.
    size_t n = (algo == Algo::KawPow) ? 80 : 76;
    job.blob.resize(n);
    for (size_t i = 0; i < n; ++i) job.blob[i] = static_cast<uint8_t>(d(gen));

    // Maximum target: nothing can meet it, so this is a pure speed test
    // (no shares are emitted).
    job.target.assign(32, 0xFF);

    if (algo == Algo::KawPow) {
        job.nonce_offset = n - 8;
        job.nonce_is_64 = true;
    } else {
        job.nonce_offset = 39;
        job.nonce_is_64 = false;
    }
    return job;
}

double run(const config::Config& cfg,
           std::vector<std::unique_ptr<core::IBackend>>& backends) {
    if (backends.empty()) {
        log_error("benchmark: no backends available");
        return 0.0;
    }
    core::Job job = make_job(cfg.algo);
    log_info(std::string("Benchmark: algo=") + algo_to_string(cfg.algo) +
             ", duration=" + std::to_string(cfg.benchmark.duration) + "s");

    for (auto& b : backends) {
        b->on_share = [](const core::Share&) {};
        b->on_log = [](const std::string&) {};
        b->set_job(job);
        b->start();
    }

    using namespace std::chrono_literals;
    auto dur_ms = static_cast<uint64_t>(cfg.benchmark.duration * 1000);
    uint64_t elapsed = 0;
    while (elapsed < dur_ms) {
        uint64_t step = std::min<uint64_t>(500, dur_ms - elapsed);
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        elapsed += step;
        double total = 0;
        for (auto& b : backends) total += b->hashrate().current;
        log_info("  ... " + common::format_hashrate(total) + " (running)");
    }

    double total = 0;
    for (auto& b : backends) {
        auto h = b->hashrate();
        log_info(std::string("  ") + b->name() + ": " + common::format_hashrate(h.average));
        total += h.average;
        b->stop();
    }
    log_info("Benchmark total: " + common::format_hashrate(total));
    return total;
}

} // namespace mm::benchmark
